// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitVisibilityProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/UnitBase.h"
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
			AActor* Actor = ActorList[i].GetMutable();
			IMassVisibilityInterface* VisInterface = Cast<IMassVisibilityInterface>(Actor);
			if (!VisInterface) continue;

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
 				}
				else if (bDoThrottledUpdate)
				{
					// On Server (Dedicated) or non-local controller, we should ensure it's visible by default for viewport
					Vis.bIsOnViewport = true;
					Vis.bIsMyTeam = true; // Avoid hiding everything on dedicated server if team logic is local-player dependent
					bIsVisibleByFog = true;
				}
				
				// 3) Sync back to Unit and check if we need to trigger logic
				// For PerformanceUnit, we can still cast to sync extra flags, but for general actors, we rely on the interface
				APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor);
				AEffectArea* Area = Cast<AEffectArea>(Actor);

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

				// 4) High-frequency Updates (Client-only)
				if (APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor))
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
