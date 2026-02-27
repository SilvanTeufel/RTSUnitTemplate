// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitVisibilityProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Actors/EffectArea.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Kismet/GameplayStatics.h"

UUnitVisibilityProcessor::UUnitVisibilityProcessor()
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true; // Required for ProjectWorldToScreen
}

void UUnitVisibilityProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	World = Owner.GetWorld();
	if (World)
	{
		SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();

		if (SignalSubsystem)
		{
			FDelegateHandle H = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateVisibility)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitVisibilityProcessor, HandleVisibilitySignals));
			VisibilitySignalDelegateHandle = H;
		}
	}
}

void UUnitVisibilityProcessor::HandleVisibilitySignals(FName /*SignalName*/, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem || !World)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (!EntityManager.IsEntityValid(Entity)) continue;

		FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (!ActorFrag) continue;

		IMassVisibilityInterface* VisInterface = Cast<IMassVisibilityInterface>(ActorFrag->GetMutable());
		if (VisInterface)
		{
			VisInterface->SetActorVisibility(VisInterface->ComputeLocalVisibility());
		}
	}
}

void UUnitVisibilityProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitVisibilityProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	
	//const bool bIsClient = (World->GetNetMode() != NM_DedicatedServer);
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// Throttled update flag
	TimeSinceLastRun += DeltaTime;
	bool bDoThrottledUpdate = (TimeSinceLastRun >= ExecutionInterval);
	if (bDoThrottledUpdate)
	{
		TimeSinceLastRun = 0.f;
	}

	int32 ViewportSizeX = 0, ViewportSizeY = 0;
	if (CustomPC && CustomPC->IsLocalController())
	{
		CustomPC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	}

	TArray<FMassEntityHandle> EntitiesToSignal;

	const float CurrentTime = World->GetTimeSeconds();

	static float LastLogTime = 0.f;
	bool bDoDiagnosticLog = false;
	if (CurrentTime - LastLogTime > 2.0f)
	{
		bDoDiagnosticLog = true;
		LastLogTime = CurrentTime;
	}

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkCtx)
	{
		const int32 N = ChunkCtx.GetNumEntities();
		const auto& Transforms = ChunkCtx.GetFragmentView<FTransformFragment>();
		const auto& StatsList = ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
		auto VisibilityList = ChunkCtx.GetMutableFragmentView<FMassVisibilityFragment>();
		auto ActorList = ChunkCtx.GetMutableFragmentView<FMassActorFragment>();
		const auto& CharList = ChunkCtx.GetFragmentView<FMassAgentCharacteristicsFragment>();
		const auto& SightList = ChunkCtx.GetFragmentView<FMassSightFragment>();
		auto* TargetList = ChunkCtx.GetFragmentView<FMassAITargetFragment>().GetData();

		for (int32 i = 0; i < N; ++i)
		{
			FMassVisibilityFragment& Vis = VisibilityList[i];
			const FMassCombatStatsFragment& Stats = StatsList[i];
			AActor* Actor = ActorList[i].GetMutable();
			IMassVisibilityInterface* VisInterface = Cast<IMassVisibilityInterface>(Actor);
			if (!VisInterface) continue;

			APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor);
			AEffectArea* Area = Cast<AEffectArea>(Actor);

			// 0) Detect Health/Shield changes (only for non-Units, Units handle this via signal-driven UpdateEntityHealth)
			const bool bHealthDifferent = !FMath::IsNearlyEqual(Vis.LastHealth, Stats.Health, 0.1f);
			const bool bShieldDifferent = !FMath::IsNearlyEqual(Vis.LastShield, Stats.Shield, 0.1f);

			if (bHealthDifferent || bShieldDifferent)
			{
				if (!Unit) // Only handle logic for non-Units here (e.g. EffectAreas). Units handle this via signal-driven UpdateEntityHealth.
				{
					// Only trigger recent damage popup if it's not the very first initialization
					if (Vis.LastHealth >= 0.f)
					{
						const bool bDamageTrigger = (Stats.Health < Vis.LastHealth - 1.0f) || (Stats.Shield < Vis.LastShield - 1.0f);
						const bool bHealTrigger = (Stats.Health > Vis.LastHealth + PositiveChangeThreshold) || (Stats.Shield > Vis.LastShield + PositiveChangeThreshold);

						if (bDamageTrigger || bHealTrigger)
						{
							Vis.LastHealthChangeTime = CurrentTime;
						}
					}
					Vis.LastHealth = Stats.Health;
					Vis.LastShield = Stats.Shield;
				}
				// For Units, we skip updating Vis.LastHealth here to prevent stealing the update from GAS signals.
				// UpdateEntityHealth will handle syncing them when the GAS signal arrives.
			}

			FVector Location = Transforms[i].GetTransform().GetLocation();
			
			if (bDoThrottledUpdate)
			{
				bool bIsVisibleByFog = true;

				if (CustomPC && CustomPC->IsLocalController())
				{
					// 1) Update Team Visibility
					int32 LocalTeamId = CustomPC->SelectableTeamId;
					Vis.bIsMyTeam = (LocalTeamId == StatsList[i].TeamId || LocalTeamId == 0);

					// 2) Update Viewport Visibility
					FVector2D ScreenPosition;
					if (UGameplayStatics::ProjectWorldToScreen(CustomPC, Location, ScreenPosition))
					{
						Vis.bIsOnViewport = (ScreenPosition.X >= -Vis.VisibilityOffset && ScreenPosition.X <= (float)ViewportSizeX + Vis.VisibilityOffset &&
											 ScreenPosition.Y >= -Vis.VisibilityOffset && ScreenPosition.Y <= (float)ViewportSizeY + Vis.VisibilityOffset);
					}
					else
					{
						Vis.bIsOnViewport = false;
					}

					// 1.5) Update Fog Visibility
					if (Vis.bIsMyTeam)
					{
						bIsVisibleByFog = true;
					}
					else
					{
						const int32* OverlapCount = SightList[i].ConsistentTeamOverlapsPerTeam.Find(LocalTeamId);
						const int32* AttackerOverlapCount = SightList[i].ConsistentAttackerTeamOverlapsPerTeam.Find(LocalTeamId);
						bool bAttacksMyTeam = false;
						if (TargetList)
						{
							const FMassAITargetFragment& TargetFrag = TargetList[i];
							if (TargetFrag.bHasValidTarget && TargetFrag.TargetEntity.IsSet())
							{
								if (EntityManager.IsEntityValid(TargetFrag.TargetEntity))
								{
									if (const FMassCombatStatsFragment* TgtStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity))
									{
										bAttacksMyTeam = (TgtStats->TeamId == LocalTeamId && TgtStats->Health > 0.f);
									}
								}
							}
						}
 					bIsVisibleByFog = (OverlapCount && *OverlapCount > 0) || (AttackerOverlapCount && *AttackerOverlapCount > 0) || bAttacksMyTeam;
 				}
 				Vis.bIsVisibleEnemy = bIsVisibleByFog;
 			}
 			else
 			{
 				// On Server (Dedicated) or non-local controller, we should ensure it's visible by default for viewport
 				Vis.bIsOnViewport = true;
 				Vis.bIsMyTeam = true; // Avoid hiding everything on dedicated server if team logic is local-player dependent
 				bIsVisibleByFog = true;
 				Vis.bIsVisibleEnemy = true;
 			}

				bool bChanged = (Vis.bIsMyTeam != Vis.bLastIsMyTeam) || 
								(Vis.bIsOnViewport != Vis.bLastIsOnViewport) ||
								(bIsVisibleByFog != Vis.bLastIsVisibleEnemy) ||
								(CharList[i].bIsInvisible != Vis.bLastIsInvisible);

				if (Unit) bChanged = bChanged || (Unit->OpenHealthWidget != Vis.bLastOpenHealthWidget) || (Unit->bShowLevelOnly != Vis.bLastShowLevelOnly);
				
				if (Unit)
				{
					Unit->IsMyTeam = Vis.bIsMyTeam;
					Unit->IsOnViewport = Vis.bIsOnViewport;
					Unit->IsVisibleEnemy = bIsVisibleByFog;
					
					if (AUnitBase* UnitBase = Cast<AUnitBase>(Unit))
					{
						UnitBase->bIsInvisible = CharList[i].bIsInvisible;
					}
				}
				else if (Area)
				{
					Area->bIsInvisible = CharList[i].bIsInvisible;
					Area->bIsVisibleByFog = bIsVisibleByFog;
				}

				if (bChanged)
				{
					EntitiesToSignal.Add(ChunkCtx.GetEntity(i));
					
					Vis.bLastIsMyTeam = Vis.bIsMyTeam;
					Vis.bLastIsOnViewport = Vis.bIsOnViewport;
					Vis.bLastIsVisibleEnemy = bIsVisibleByFog;
					Vis.bLastIsInvisible = CharList[i].bIsInvisible;

					if (Unit)
					{
						Vis.bLastOpenHealthWidget = Unit->OpenHealthWidget;
						Vis.bLastShowLevelOnly = Unit->bShowLevelOnly;
					}
				}
			}

			// Decide Healthbar visibility (High-frequency)
			if (Unit)
			{
				const bool bIsConstruction = Unit->IsA(AConstructionUnit::StaticClass());
				
				// Use per-unit duration if set, otherwise processor default
				const float VisibleDuration = (Unit->HealthWidgetDisplayDuration > 0.f) ? Unit->HealthWidgetDisplayDuration : DefaultHealthbarVisibleTime;
				const bool bRecentlyDamaged = (CurrentTime - Vis.LastHealthChangeTime) < VisibleDuration;
				const bool bRecentlyLeveled = (CurrentTime - Vis.LastLevelUpTime) < VisibleDuration;
				const bool bRecentPing = CustomPC && (CurrentTime - CustomPC->LastHealthBarPingTime) < VisibleDuration;
				const bool bNotFull = (Stats.Health > 0.f) && ((Stats.Health < Stats.MaxHealth - 1.0f) || (Stats.Shield < Stats.MaxShield - 1.0f));

				const bool bShowDueToPing = bRecentPing && bNotFull && (Vis.bIsMyTeam || Vis.bIsVisibleEnemy);

				if (bRecentPing && bDoDiagnosticLog && (World->GetNetMode() == NM_Client || (CustomPC && CustomPC->IsLocalController())))
				{
					UE_LOG(LogTemp, Error, TEXT("[CLIENT][V-Ping] %s: Show=%d | RecentPing=%d, NotFull=%d, IsMyTeam=%d, IsVisibleEnemy=%d | Health=%.2f/%.2f, Shield=%.2f/%.2f"),
						*Actor->GetName(), bShowDueToPing, bRecentPing, bNotFull, Vis.bIsMyTeam, Vis.bIsVisibleEnemy, Stats.Health, Stats.MaxHealth, Stats.Shield, Stats.MaxShield);
				}

				// Include construction check and recently damaged/leveled/pinged popups; always auto-collapse after VisibleDuration or if health <= 0
				Unit->OpenHealthWidget = (Stats.Health > 0.f) && (bIsConstruction || bRecentlyDamaged || bRecentlyLeveled || bShowDueToPing);
				Unit->bShowLevelOnly = (Stats.Health > 0.f) && (bRecentlyLeveled && !bShowDueToPing && !bIsConstruction);

				if (World->GetNetMode() == NM_Client || (CustomPC && CustomPC->IsLocalController()))
				{
					if (Unit->OpenHealthWidget && bDoDiagnosticLog)
					{
						/*
						const float DamagedTimeLeft = FMath::Max(0.f, VisibleDuration - (CurrentTime - Vis.LastHealthChangeTime));
						const float PingTimeLeft = CustomPC ? FMath::Max(0.f, VisibleDuration - (CurrentTime - CustomPC->LastHealthBarPingTime)) : 0.f;
						
						UE_LOG(LogTemp, Error, TEXT("[CLIENT][VisibilityProcessor] %s: OHW=1 | Damaged=%d (%.2fs left), Ping=%d (%.2fs left), NotFull=%d, ShowDueToPing=%d | Health=%.2f/%.2f, Shield=%.2f/%.2f"),
							*Actor->GetName(), bRecentlyDamaged, DamagedTimeLeft, bRecentPing, PingTimeLeft, bNotFull, bShowDueToPing, Stats.Health, Stats.MaxHealth, Stats.Shield, Stats.MaxShield);
						*/
					}
				}
			}

			// 4) High-frequency Updates (Client-only)
			if (Unit)
			{
				if (!Unit->StopVisibilityTick)
				{
					Unit->UpdateWidgetPositions(Location);
					Unit->SyncAttachedAssetsVisibility();
				}
			}
		}
	});

	if (bDoThrottledUpdate && SignalSubsystem && EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UnitSignals::UpdateVisibility, EntitiesToSignal);
	}
}
