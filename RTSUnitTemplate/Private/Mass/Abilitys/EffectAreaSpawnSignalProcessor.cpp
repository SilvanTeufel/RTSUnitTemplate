// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Abilitys/EffectAreaSpawnSignalProcessor.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Actors/EffectArea.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameModes/RTSGameModeBase.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"

UEffectAreaSpawnSignalProcessor::UEffectAreaSpawnSignalProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void UEffectAreaSpawnSignalProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	if (UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld()))
	{
		SubscribeToSignal(*SignalSubsystem, UnitSignals::SpawnEffectAreaRequested);
	}
}

void UEffectAreaSpawnSignalProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SpawnQuery.Initialize(EntityManager);
	SpawnQuery.AddRequirement<FEffectAreaDuplicateFragment>(EMassFragmentAccess::ReadWrite);
	SpawnQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	SpawnQuery.AddTagRequirement<FMassEffectAreaDuplicateTag>(EMassFragmentPresence::All);
	SpawnQuery.RegisterWithProcessor(*this);
}

void UEffectAreaSpawnSignalProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	SpawnQuery.ForEachEntityChunk(Context, [&EntityManager, &EntitySignals](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		auto DuplicateList = ChunkContext.GetMutableFragmentView<FEffectAreaDuplicateFragment>();
		auto ImpactList = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();
		bool bHasImpact = ImpactList.Num() > 0;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			TArray<FName> Signals;
			EntitySignals.GetSignalsForEntity(Entity, Signals);
			if (!Signals.Contains(UnitSignals::SpawnEffectAreaRequested))
			{
				continue;
			}

			FEffectAreaDuplicateFragment& DuplicateFrag = DuplicateList[i];
			
			// Get PlayerController
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(ChunkContext.GetWorld(), 0))
			{
				if (AControllerBase* Controller = Cast<AControllerBase>(PC))
				{
					// Spawn!
					AEffectArea* NewArea = Controller->SpawnEffectArea(DuplicateFrag.TeamId, DuplicateFrag.PendingSpawnLocation, FVector::OneVector, DuplicateFrag.EffectAreaClass, nullptr);
					
					if (NewArea)
					{
						if (bHasImpact)
						{
							NewArea->BaseRadius = ImpactList[i].BaseRadius;
							NewArea->StartRadius = ImpactList[i].StartRadius;
							NewArea->EndRadius = ImpactList[i].EndRadius;
						}
						NewArea->DuplicationRadius = DuplicateFrag.DuplicationRadius;
						NewArea->DuplicationTime = DuplicateFrag.DuplicationTime;
						NewArea->RandomAngleRange = DuplicateFrag.RandomAngleRange;
						NewArea->LastDuplicationDirection = DuplicateFrag.LastDirection;
						NewArea->MaxDuplicationCount = DuplicateFrag.MaxDuplicationCount;
						NewArea->DuplicationId = DuplicateFrag.DuplicationId;

						DuplicateFrag.SpawnedChild = NewArea;
					}
				}
			}
		}
	});
}
