// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Abilitys/EffectAreaDuplicateProcessor.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "MassProcessor.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Actors/EffectArea.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "GameModes/RTSGameModeBase.h"

UMassEffectAreaDuplicateProcessor::UMassEffectAreaDuplicateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UMassEffectAreaDuplicateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	if (UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld()))
	{
		SubscribeToSignal(*SignalSubsystem, UnitSignals::DuplicateEffectArea);
	}
}

void UMassEffectAreaDuplicateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AreaQuery.Initialize(EntityManager);
	AreaQuery.AddRequirement<FEffectAreaDuplicateFragment>(EMassFragmentAccess::ReadWrite);
	AreaQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	AreaQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	AreaQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	AreaQuery.AddTagRequirement<FMassEffectAreaDuplicateTag>(EMassFragmentPresence::All);
	AreaQuery.AddTagRequirement<FMassEffectAreaLoadingTag>(EMassFragmentPresence::None);
	AreaQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	AreaQuery.RegisterWithProcessor(*this);

	EnemyQuery.Initialize(EntityManager);
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);

	GlobalUpdateQuery.Initialize(EntityManager);
	GlobalUpdateQuery.AddRequirement<FEffectAreaDuplicateFragment>(EMassFragmentAccess::ReadWrite);
	GlobalUpdateQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	GlobalUpdateQuery.AddTagRequirement<FMassEffectAreaDuplicateTag>(EMassFragmentPresence::All);
	GlobalUpdateQuery.AddTagRequirement<FMassEffectAreaLoadingTag>(EMassFragmentPresence::None);
	GlobalUpdateQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
}

void UMassEffectAreaDuplicateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	ProcessingTime += DeltaTime;

	if (ProcessingTime >= ExecutionInterval)
	{
		// 1. Global population count per ID
		TMap<int32, int32> IdCounts;
		FMassExecutionContext GlobalContext(EntityManager);

		GlobalUpdateQuery.ForEachEntityChunk(GlobalContext, [&IdCounts](FMassExecutionContext& ChunkContext)
		{
			auto DuplicateList = ChunkContext.GetFragmentView<FEffectAreaDuplicateFragment>();
			for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
			{
				if (DuplicateList[i].DuplicationId > 0)
				{
					IdCounts.FindOrAdd(DuplicateList[i].DuplicationId)++;
				}
			}
		});

		//for (auto& Pair : IdCounts)
		//{
			//UE_LOG(LogTemp, Log, TEXT("EffectAreaDuplicate: Population ID=%d, Count=%d"), Pair.Key, Pair.Value);
		//}

		UMassSignalSubsystem* SignalSubsystem = Context.GetWorld() ? Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>() : nullptr;
		if (!SignalSubsystem) return;

		GlobalUpdateQuery.ForEachEntityChunk(GlobalContext, [this, &IdCounts, SignalSubsystem](FMassExecutionContext& ChunkContext)
		{
			const int32 NumEntities = ChunkContext.GetNumEntities();
			TArrayView<FEffectAreaDuplicateFragment> DuplicateList = ChunkContext.GetMutableFragmentView<FEffectAreaDuplicateFragment>();
			TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				FEffectAreaDuplicateFragment& DuplicateFrag = DuplicateList[i];
				FMassEntityHandle Entity = ChunkContext.GetEntity(i);

				// Assign ID if missing (root entity)
				if (DuplicateFrag.DuplicationId == 0)
				{
					DuplicateFrag.DuplicationId = NextDuplicationId++;
					IdCounts.FindOrAdd(DuplicateFrag.DuplicationId) = 1;

					// Write back to Actor so children can inherit it
					if (AActor* Owner = ActorList[i].GetMutable())
					{
						if (AEffectArea* Area = Cast<AEffectArea>(Owner))
						{
							Area->DuplicationId = DuplicateFrag.DuplicationId;
						}
					}
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Assigned New Root ID=%d to Entity=%d"), DuplicateFrag.DuplicationId, Entity.Index);
				}

				// Case 1: Valid Child exists - check if it has a Mass entity
				if (DuplicateFrag.SpawnedChild.IsValid())
				{
					bool bChildIsActiveInMass = false;
					if (UMassActorBindingComponent* ChildBinding = DuplicateFrag.SpawnedChild->FindComponentByClass<UMassActorBindingComponent>())
					{
						if (ChildBinding->GetMassEntityHandle().IsValid())
						{
							bChildIsActiveInMass = true;
						}
					}

					if (bChildIsActiveInMass)
					{
						//UE_LOG(LogTemp, Log, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) has valid child in Mass, skipping timer."), Entity.Index, DuplicateFrag.DuplicationId);
						DuplicateFrag.ChildMassWaitTimer = 0.f; // Reset wait timer
						continue;
					}
					else
					{
						// Child exists but has no Mass yet. Wait for a few seconds.
						DuplicateFrag.ChildMassWaitTimer += ProcessingTime;
						if (DuplicateFrag.ChildMassWaitTimer < 10.0f)
						{
							//UE_LOG(LogTemp, Log, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) child %s exists but not yet in Mass. Waiting (%.2fs)..."), 
								//Entity.Index, DuplicateFrag.DuplicationId, *DuplicateFrag.SpawnedChild->GetName(), DuplicateFrag.ChildMassWaitTimer);
							continue;
						}
						else
						{
							//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) child %s failed to get Mass after 10s. Resetting child to try again."), 
								//Entity.Index, DuplicateFrag.DuplicationId, *DuplicateFrag.SpawnedChild->GetName());
							DuplicateFrag.SpawnedChild.Reset();
							DuplicateFrag.ChildMassWaitTimer = 0.f;
							DuplicateFrag.DuplicationTimer = 0.f;
						}
					}
				}

				// Case 2: Child was destroyed - Reset Timer and clear child
				if (DuplicateFrag.SpawnedChild.IsStale())
				{
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) child is stale, resetting timer."), Entity.Index, DuplicateFrag.DuplicationId);
					DuplicateFrag.DuplicationTimer = 0.f;
					DuplicateFrag.SpawnedChild.Reset();
				}

				// Case 3: Population Limit Check for this specific chain
				int32 CurrentCount = IdCounts.FindRef(DuplicateFrag.DuplicationId);
				if (DuplicateFrag.MaxDuplicationCount > 0 && CurrentCount >= DuplicateFrag.MaxDuplicationCount)
				{
					//UE_LOG(LogTemp, Log, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) reached Limit (%d/%d), skipping timer."), 
						//Entity.Index, DuplicateFrag.DuplicationId, CurrentCount, DuplicateFrag.MaxDuplicationCount);
					continue;
				}

				// Case 4: No child and under limit - Increment Timer
				DuplicateFrag.DuplicationTimer += ProcessingTime; 
				
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) Timer=%.2f/%.2f (Pop=%d/%d)"), 
					//Entity.Index, DuplicateFrag.DuplicationId, DuplicateFrag.DuplicationTimer, DuplicateFrag.DuplicationTime, CurrentCount, DuplicateFrag.MaxDuplicationCount);

				if (DuplicateFrag.DuplicationTimer >= DuplicateFrag.DuplicationTime)
				{
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Entity=%d (ID=%d) Sending Signal!"), Entity.Index, DuplicateFrag.DuplicationId);
					SignalSubsystem->SignalEntity(UnitSignals::DuplicateEffectArea, Entity);
				}
			}
		});

		ProcessingTime = 0.f;
	}

	Super::Execute(EntityManager, Context);
}

void UMassEffectAreaDuplicateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	// 1. Pre-calculate enemy locations
	TArray<FVector> EnemyLocations;
	TArray<int32> EnemyTeams;
	
	FMassExecutionContext GlobalContext(EntityManager);
	EnemyQuery.ForEachEntityChunk(GlobalContext, [&EnemyLocations, &EnemyTeams](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			EnemyLocations.Add(TransformList[i].GetTransform().GetLocation());
			EnemyTeams.Add(StatsList[i].TeamId);
		}
	});

	// 2. Global population count per ID for the signal phase
	TMap<int32, int32> IdCounts;
	FMassExecutionContext GlobalCountContext(EntityManager);
	GlobalUpdateQuery.ForEachEntityChunk(GlobalCountContext, [&IdCounts](FMassExecutionContext& ChunkContext)
	{
		auto DuplicateList = ChunkContext.GetFragmentView<FEffectAreaDuplicateFragment>();
		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			if (DuplicateList[i].DuplicationId > 0)
			{
				IdCounts.FindOrAdd(DuplicateList[i].DuplicationId)++;
			}
		}
	});
	

	//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Found %d potential units in world."), EnemyLocations.Num());
	
	AreaQuery.ForEachEntityChunk(Context, [this, &EnemyLocations, &EnemyTeams, &EntitySignals, &IdCounts, &EntityManager](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		auto DuplicateList = ChunkContext.GetMutableFragmentView<FEffectAreaDuplicateFragment>();
		auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		auto ImpactList = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();
		bool bHasImpact = ImpactList.Num() > 0;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			TArray<FName> Signals;
			EntitySignals.GetSignalsForEntity(Entity, Signals);
			if (!Signals.Contains(UnitSignals::DuplicateEffectArea))
			{
				continue;
			}

			FEffectAreaDuplicateFragment& DuplicateFrag = DuplicateList[i];
			
			// Assign ID if missing (root entity) - Safe check if signal arrives before Execute assigns ID
			if (DuplicateFrag.DuplicationId == 0)
			{
				DuplicateFrag.DuplicationId = NextDuplicationId++;
				IdCounts.FindOrAdd(DuplicateFrag.DuplicationId)++;

				if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
				{
					if (AActor* Owner = ActorFrag->GetMutable())
					{
						if (AEffectArea* Area = Cast<AEffectArea>(Owner))
						{
							Area->DuplicationId = DuplicateFrag.DuplicationId;
						}
					}
				}
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Signal phase assigned New Root ID=%d to Entity=%d"), DuplicateFrag.DuplicationId, Entity.Index);
			}

			// Final Population Limit Check before spawn
			int32 CurrentCount = IdCounts.FindRef(DuplicateFrag.DuplicationId);
			if (DuplicateFrag.SpawnedChild.IsValid())
			{
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Signal received for Entity=%d (ID=%d) but already has valid child! Skipping."), Entity.Index, DuplicateFrag.DuplicationId);
				continue;
			}

			if (DuplicateFrag.MaxDuplicationCount > 0 && CurrentCount >= DuplicateFrag.MaxDuplicationCount)
			{
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Signal received for Entity=%d (ID=%d) but Limit reached (%d/%d)! Skipping."), 
					//Entity.Index, DuplicateFrag.DuplicationId, CurrentCount, DuplicateFrag.MaxDuplicationCount);
				continue;
			}

			//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Processing Spawn for Entity=%d (ID=%d, Pop=%d/%d)"), 
				//Entity.Index, DuplicateFrag.DuplicationId, CurrentCount, DuplicateFrag.MaxDuplicationCount);

			FVector CurrentLoc = TransformList[i].GetTransform().GetLocation();
			FVector AvgEnemyLoc = FVector::ZeroVector;
			int32 EnemyCount = 0;

			for (int32 j = 0; j < EnemyLocations.Num(); ++j)
			{
				if (EnemyTeams[j] != DuplicateFrag.TeamId)
				{
					AvgEnemyLoc += EnemyLocations[j];
					EnemyCount++;
				}
			}

			FVector Direction;
			if (EnemyCount > 0)
			{
				AvgEnemyLoc /= (float)EnemyCount;
				float DistToEnemy = FVector::Dist2D(CurrentLoc, AvgEnemyLoc);
				FVector NewTargetDir = (AvgEnemyLoc - CurrentLoc).GetSafeNormal2D();
				float DotProduct = 0.f;

				if (!DuplicateFrag.LastDirection.IsNearlyZero())
				{
					DotProduct = FVector::DotProduct(DuplicateFrag.LastDirection, NewTargetDir);
				}

				// Wenn wir nah dran sind ODER wenn die neue Richtung zurück zum Gegner führt (Dot < 0)
				if ((DistToEnemy < DuplicateFrag.DuplicationRadius || DotProduct < 0.f) && !DuplicateFrag.LastDirection.IsNearlyZero())
				{
					Direction = DuplicateFrag.LastDirection;
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Close to enemy (Dist=%.2f) or Reverse Detected (Dot=%.2f), maintaining LastDirection"), DistToEnemy, DotProduct);
				}
				else
				{
					Direction = NewTargetDir;
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Entity=%d, CurrentLoc=%s, AvgEnemyLoc=%s, EnemyCount=%d, BaseDirection=%s"), 
						//Entity.Index, *CurrentLoc.ToString(), *AvgEnemyLoc.ToString(), EnemyCount, *Direction.ToString());
				}

				if (Direction.IsNearlyZero())
				{
					if (!DuplicateFrag.LastDirection.IsNearlyZero())
					{
						Direction = DuplicateFrag.LastDirection;
					}
					else
					{
						Direction = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal2D();
					}
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Direction was zero, using fallback: %s"), *Direction.ToString());
				}
			}
			else if (!DuplicateFrag.LastDirection.IsNearlyZero())
			{
				Direction = DuplicateFrag.LastDirection;
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: No enemies found, maintaining LastDirection: %s"), *Direction.ToString());
			}
			else
			{
				Direction = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal2D();
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: No enemies found, using random direction: %s"), *Direction.ToString());
			}

			// Apply random rotation
			if (DuplicateFrag.RandomAngleRange > 0.f)
			{
				float HalfRange = DuplicateFrag.RandomAngleRange * 0.5f;
				float RandomAngle = FMath::FRandRange(-HalfRange, HalfRange);
				Direction = Direction.RotateAngleAxis(RandomAngle, FVector::UpVector);
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: RotatedDirection=%s (RandomAngle=%.2f, Range=%.2f)"), 
					//*Direction.ToString(), RandomAngle, DuplicateFrag.RandomAngleRange);
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: FinalDirection=%s (RandomAngleRange is 0)"), *Direction.ToString());
			}

			UWorld* World = ChunkContext.GetWorld();
			if (!World) continue;

			int32 MaxAttempts = 10;
			float RotationIncrement = 360.f / (float)MaxAttempts;

			for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
			{
				FVector SpawnLocation = CurrentLoc + Direction * DuplicateFrag.DuplicationRadius;
				
				// Line trace to find landscape
				FHitResult HitResult;
				FVector TraceStart = SpawnLocation + FVector(0, 0, 2000);
				FVector TraceEnd = SpawnLocation - FVector(0, 0, 2000);
				FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(EffectAreaTrace), true);

				if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
				{
					AActor* HitActor = HitResult.GetActor();
					if (HitActor && HitActor->IsA(ALandscape::StaticClass()))
					{
						SpawnLocation.Z = HitResult.ImpactPoint.Z;

						FQuat ActorYawRotation = FRotationMatrix::MakeFromX(Direction).ToQuat();
						
						AEffectArea* NewArea = World->SpawnActorDeferred<AEffectArea>(DuplicateFrag.EffectAreaClass, FTransform(ActorYawRotation, SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
						if (NewArea)
						{
							if (ARTSGameModeBase* GM = World->GetAuthGameMode<ARTSGameModeBase>())
							{
								NewArea->AreaIndex = ++GM->HighestUnitIndex;
							}

							// Transfer properties
							NewArea->TeamId = DuplicateFrag.TeamId;
							if (bHasImpact)
							{
								NewArea->BaseRadius = ImpactList[i].BaseRadius;
								NewArea->StartRadius = ImpactList[i].StartRadius;
								NewArea->EndRadius = ImpactList[i].EndRadius;
							}
							NewArea->DuplicationRadius = DuplicateFrag.DuplicationRadius;
							NewArea->DuplicationTime = DuplicateFrag.DuplicationTime;
							NewArea->RandomAngleRange = DuplicateFrag.RandomAngleRange;
							NewArea->LastDuplicationDirection = Direction;
							NewArea->MaxDuplicationCount = DuplicateFrag.MaxDuplicationCount;
							NewArea->DuplicationId = DuplicateFrag.DuplicationId;

							float RandomZ = 0.f;
							if (NewArea->bAddRandomZRotation)
							{
								RandomZ = FMath::FRandRange(0.f, 360.f);
							}
							
							NewArea->VisualRotationOffset = AEffectArea::CalculateGroundRotationOffset(HitResult.ImpactNormal, Direction, RandomZ);
							
							UGameplayStatics::FinishSpawningActor(NewArea, FTransform(ActorYawRotation, SpawnLocation));
							DuplicateFrag.SpawnedChild = NewArea;
							DuplicateFrag.DuplicationTimer = 0.f; // Reset timer
							DuplicateFrag.LastDirection = Direction;
							DuplicateFrag.ChildMassWaitTimer = 0.f; // Reset wait timer
							
							// Update local count for this frame to prevent over-spawning
							IdCounts.FindOrAdd(DuplicateFrag.DuplicationId)++;

							//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Attempt %d successful at %s on Landscape %s (ID=%d, Population=%d/%d)"), 
								//Attempt, *SpawnLocation.ToString(), *HitActor->GetName(), DuplicateFrag.DuplicationId, IdCounts[DuplicateFrag.DuplicationId], DuplicateFrag.MaxDuplicationCount);
							break;
						}
					}
					else
					{
						//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Attempt %d failed. Hit actor: %s (not a landscape)"), Attempt, HitActor ? *HitActor->GetName() : TEXT("None"));
					}
				}
				else
				{
					//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Attempt %d failed. No hit."), Attempt);
				}
				
				// If not landscape or trace failed, rotate direction for next attempt
				Direction = Direction.RotateAngleAxis(RotationIncrement, FVector::UpVector);
				//UE_LOG(LogTemp, Warning, TEXT("EffectAreaDuplicate: Rotating for next attempt. New Direction: %s"), *Direction.ToString());
			}
		}
	});
}
