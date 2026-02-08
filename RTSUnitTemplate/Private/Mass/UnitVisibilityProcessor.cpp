// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitVisibilityProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/PerformanceUnit.h"
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

		APerformanceUnit* Unit = Cast<APerformanceUnit>(ActorFrag->GetMutable());
		if (Unit && !Unit->StopVisibilityTick)
		{
			if (Unit->EnableFog) Unit->VisibilityTickFog();
			else Unit->SetCharacterVisibility(Unit->IsOnViewport);

			Unit->CheckHealthBarVisibility();
			Unit->CheckTimerVisibility();
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
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitVisibilityProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	
	const bool bIsClient = (World->GetNetMode() != NM_DedicatedServer);
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// Throttled update flag
	TimeSinceLastRun += DeltaTime;
	bool bDoThrottledUpdate = (TimeSinceLastRun >= ExecutionInterval);
	if (bDoThrottledUpdate)
	{
		TimeSinceLastRun = 0.f;
	}

	int32 ViewportSizeX = 0, ViewportSizeY = 0;
	if (bIsClient && CustomPC && CustomPC->IsLocalController())
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

		for (int32 i = 0; i < N; ++i)
		{
			FMassVisibilityFragment& Vis = VisibilityList[i];
			AActor* Actor = ActorList[i].GetMutable();
			APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor);
			if (!Unit) continue;

			FVector Location = Transforms[i].GetTransform().GetLocation();

			if (bDoThrottledUpdate)
			{
				if (bIsClient && CustomPC && CustomPC->IsLocalController())
				{
					// 1) Update Team Visibility
					Vis.bIsMyTeam = (CustomPC->SelectableTeamId == StatsList[i].TeamId || CustomPC->SelectableTeamId == 0);

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
				}
				
				// 3) Sync back to Unit and check if we need to trigger logic
				bool bChanged = (Vis.bIsMyTeam != Vis.bLastIsMyTeam) || (Vis.bIsOnViewport != Vis.bLastIsOnViewport) || (Unit->IsVisibleEnemy != Vis.bLastIsVisibleEnemy)
								|| (Unit->OpenHealthWidget != Vis.bLastOpenHealthWidget) || (Unit->bShowLevelOnly != Vis.bLastShowLevelOnly);
				
				Unit->IsMyTeam = Vis.bIsMyTeam;
				Unit->IsOnViewport = Vis.bIsOnViewport;

				if (bChanged && !Unit->StopVisibilityTick)
				{
					EntitiesToSignal.Add(ChunkCtx.GetEntity(i));
					
					Vis.bLastIsMyTeam = Vis.bIsMyTeam;
					Vis.bLastIsOnViewport = Vis.bIsOnViewport;
					Vis.bLastIsVisibleEnemy = Unit->IsVisibleEnemy;
					Vis.bLastOpenHealthWidget = Unit->OpenHealthWidget;
					Vis.bLastShowLevelOnly = Unit->bShowLevelOnly;
				}
			}

			// 4) High-frequency Widget Updates (Client-only)
			if (bIsClient && !Unit->StopVisibilityTick)
			{
				Unit->UpdateWidgetPositions(Location);
			}
		}
	});

	if (bDoThrottledUpdate && SignalSubsystem && EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UnitSignals::UpdateVisibility, EntitiesToSignal);
	}
}
