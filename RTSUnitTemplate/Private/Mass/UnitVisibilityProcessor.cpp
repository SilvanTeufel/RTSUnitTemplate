// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitVisibilityProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Actors/EffectArea.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "System/PlayerTeamSubsystem.h"
#include "Kismet/GameplayStatics.h"

UUnitVisibilityProcessor::UUnitVisibilityProcessor()
{
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
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
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitVisibilityProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!World) return;

	APlayerController* LocalPC = nullptr;
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PC = Iterator->Get();
			if (PC && PC->IsLocalController())
			{
				LocalPC = PC;
				break;
			}
		}
	}
	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(LocalPC);
	
	int64 LocalAllianceMask = 0;
	int32 LocalTeamId = 0;
	if (CustomPC)
	{
		LocalTeamId = CustomPC->SelectableTeamId;
		LocalAllianceMask = CustomPC->AlliedTeamsMask;
		
		// Fallback if mask is not yet set but we have a TeamId
		if (LocalAllianceMask == 0 && LocalTeamId != 0)
		{
			LocalAllianceMask = (1LL << LocalTeamId);
		}
	}

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
			AActor* Actor = ActorList.Num() > 0 ? ActorList[i].GetMutable() : nullptr;
			IMassVisibilityInterface* VisInterface = Cast<IMassVisibilityInterface>(Actor);

			APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor);
			AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
			AEffectArea* Area = Cast<AEffectArea>(Actor);
			
			FVector Location = Transforms[i].GetTransform().GetLocation();
			bool bIsVisibleByFog = true;

			if (bDoThrottledUpdate)
			{
				if (CustomPC && CustomPC->IsLocalController())
				{
					// 1) Update Team Visibility
   		Vis.bIsMyTeam = (LocalAllianceMask & (1LL << StatsList[i].TeamId)) != 0 || LocalTeamId == 0;

					// 2) Update Viewport Visibility
					FVector2D ScreenPosition;
					bool bCalculatedOnViewport = false;
					if (UGameplayStatics::ProjectWorldToScreen(CustomPC, Location, ScreenPosition))
					{
						bCalculatedOnViewport = (ScreenPosition.X >= -Vis.VisibilityOffset && ScreenPosition.X <= (float)ViewportSizeX + Vis.VisibilityOffset &&
											 ScreenPosition.Y >= -Vis.VisibilityOffset && ScreenPosition.Y <= (float)ViewportSizeY + Vis.VisibilityOffset);
					}
					
					Vis.bIsOnViewport = bCalculatedOnViewport;

					// 1.5) Update Fog Visibility
					if (Vis.bIsMyTeam || !Vis.bAffectedByFogOfWar)
					{
						bIsVisibleByFog = true;
					}
					else if (SightList.Num() > 0)
					{
						bool bSeenByAlliance = false;
						for (const auto& Pair : SightList[i].ConsistentTeamOverlapsPerTeam)
						{
							if (Pair.Value > 0 && (LocalAllianceMask & (1LL << Pair.Key)) != 0)
							{
								bSeenByAlliance = true;
								break;
							}
						}

						if (!bSeenByAlliance)
						{
							for (const auto& Pair : SightList[i].ConsistentAttackerTeamOverlapsPerTeam)
							{
								if (Pair.Value > 0 && (LocalAllianceMask & (1LL << Pair.Key)) != 0)
								{
									bSeenByAlliance = true;
									break;
								}
							}
						}

						bool bAttacksMyAlliance = false;
						if (!bSeenByAlliance && TargetList)
						{
							const FMassAITargetFragment& TargetFrag = TargetList[i];
							if (TargetFrag.bHasValidTarget && TargetFrag.TargetEntity.IsSet())
							{
								if (EntityManager.IsEntityActive(TargetFrag.TargetEntity))
								{
									if (const FMassCombatStatsFragment* TgtStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity))
									{
       			bAttacksMyAlliance = (TgtStats->Health > 0.f && (LocalAllianceMask & (1LL << TgtStats->TeamId)) != 0);
									}
								}
							}
						}
						bIsVisibleByFog = bSeenByAlliance || bAttacksMyAlliance;
					}
					
					if (bIsVisibleByFog)
					{
						Vis.bIsVisibleEnemy = true;
						Vis.LastVisibleTime = CurrentTime;
					}
					else if (!DoesEntityHaveTag(EntityManager, ChunkCtx.GetEntity(i), FMassStateStopMovementTag::StaticStruct()) && (!UnitBase || UnitBase->CanMove))
					{
						// Hysteresis: Only set to false if enough time has passed since last visible
						if (CurrentTime - Vis.LastVisibleTime > 0.2f)
						{
							Vis.bIsVisibleEnemy = false;
						}
					}

					if (Unit)
					{
						Unit->IsOnViewport = bCalculatedOnViewport;
						Unit->IsMyTeam = (LocalAllianceMask & (1LL << StatsList[i].TeamId)) != 0 || LocalTeamId == 0;
						Unit->IsVisibleEnemy = Vis.bIsVisibleEnemy;
					}
					
					if (UnitBase && Vis.bIsVisibleEnemy)
					{
						UnitBase->LastVisibleTime = CurrentTime;
					}
					
					if (Area)
					{
						Area->bIsOnViewport = bCalculatedOnViewport;
						Area->bIsVisibleByFog = Vis.bIsVisibleEnemy;
					}
				}
				else if (World->GetNetMode() == NM_DedicatedServer)
				{
					// On Server (Dedicated), we should ensure it's visible by default for viewport
					// This prevents the server's viewport from affecting clients if this fragment is ever shared or replicated.
					Vis.bIsOnViewport = true;
					Vis.bIsMyTeam = true; 
					Vis.bIsVisibleEnemy = true;
					bIsVisibleByFog = true;
				}
			}
			else
			{
				// If not throttled update, keep the last known visibility state for calculations below
				bIsVisibleByFog = Vis.bIsVisibleEnemy;
			}

			bool bChanged = (Vis.bIsMyTeam != Vis.bLastIsMyTeam) || 
							(Vis.bIsOnViewport != Vis.bLastIsOnViewport) ||
							(Vis.bIsVisibleEnemy != Vis.bLastIsVisibleEnemy) ||
							(CharList[i].bIsInvisible != Vis.bLastIsInvisible);

			if (Unit) bChanged = bChanged || (Unit->OpenHealthWidget != Vis.bLastOpenHealthWidget) || (Unit->bShowLevelOnly != Vis.bLastShowLevelOnly);
			
			if (Unit)
			{
				// We already updated Unit properties above for local controllers, 
				// but we ensure it's synced here for all cases if needed.
				// However, if we are not a local controller, we should be careful.
				if (!(CustomPC && CustomPC->IsLocalController()))
				{
					Unit->IsMyTeam = Vis.bIsMyTeam;
					Unit->IsOnViewport = Vis.bIsOnViewport;
					Unit->IsVisibleEnemy = Vis.bIsVisibleEnemy;
				}
				
				if (UnitBase)
				{
					UnitBase->bIsInvisible = CharList[i].bIsInvisible;
				}
			}
			else if (Area)
			{
				Area->bIsInvisible = CharList[i].bIsInvisible;
				Area->bIsVisibleByFog = Vis.bIsVisibleEnemy;
				Area->bIsOnViewport = Vis.bIsOnViewport;
			}

			if (bChanged)
			{
				EntitiesToSignal.Add(ChunkCtx.GetEntity(i));
				
				Vis.bLastIsMyTeam = Vis.bIsMyTeam;
				Vis.bLastIsOnViewport = Vis.bIsOnViewport;
				Vis.bLastIsVisibleEnemy = Vis.bIsVisibleEnemy;
				Vis.bLastIsInvisible = CharList[i].bIsInvisible;

				if (Unit)
				{
					Vis.bLastOpenHealthWidget = Unit->OpenHealthWidget;
					Vis.bLastShowLevelOnly = Unit->bShowLevelOnly;
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
				const bool bNotFull = (Stats.Health > 0.f) && ((Stats.MaxHealth > 0.f && Stats.Health < Stats.MaxHealth - 1.0f) || (Stats.MaxShield > 0.f && Stats.Shield < Stats.MaxShield - 1.0f));

				const bool bShowDueToPing = bRecentPing && bNotFull && (Vis.bIsMyTeam || Vis.bIsVisibleEnemy);

				// Include construction check and recently damaged/leveled/pinged popups; always auto-collapse after VisibleDuration or if health <= 0
				// Special case: Construction units show healthbar even at 0 health while being built
				Unit->OpenHealthWidget = (Stats.Health > 0.f || bIsConstruction) && (bIsConstruction || bRecentlyDamaged || bRecentlyLeveled || bShowDueToPing);
				Unit->bShowLevelOnly = (Stats.Health > 0.f) && (bRecentlyLeveled && !bShowDueToPing && !bIsConstruction && !bRecentlyDamaged);
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
