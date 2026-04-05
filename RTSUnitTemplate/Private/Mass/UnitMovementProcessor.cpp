// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitMovementProcessor.h"

// ... other includes ...
#include "MassExecutionContext.h"
#include "Steering/MassSteeringFragments.h" // Ensure included
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavAreas/NavArea_EnergyWall.h"
#include "Async/Async.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/CapsuleComponent.h"

UUnitMovementProcessor::UUnitMovementProcessor(): EntityQuery()
{
    // Run BEFORE steering, avoidance, and movement integration
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; // Or potentially Input
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
    ProcessingPhase = EMassProcessingPhase::PrePhysics; // Good phase for pathfinding/intent
    
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UUnitMovementProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());
}

void UUnitMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);        // READ current position
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);  // READ the target location/speed
    EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite); // WRITE desired velocity
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite); // Keep for managing path state
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
    
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);   
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);  
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    //EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);

	ClientEntityQuery.Initialize(EntityManager);
	ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite);

    ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
    
    ClientEntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);   
    ClientEntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);  
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   
    ClientEntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    //ClientEntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

    ClientEntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
    ClientEntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
    
	ClientEntityQuery.RegisterWithProcessor(*this);
}



void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Lazy-init EntitySubsystem on first Execute in case InitializeInternal ran before world subsystems were ready (common on clients)
    if (!EntitySubsystem)
    {
        EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        if (!EntitySubsystem)
        {
            return;
        }
    }

    if (World->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UUnitMovementProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    const bool bHasNavSystem = (NavSystem != nullptr);

    ClientEntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassSteeringFragment& Steering = SteeringList[i];
            Steering.DesiredVelocity = FVector::ZeroVector;

            const FMassAIStateFragment& AIState = AIStateList[i];
            
            if (!AIState.IsInitialized)
            {
                continue;
            }
            if (!AIState.CanMove)
            {
                continue;
            }
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            
            FVector CurrentLocation = CurrentMassTransform.GetLocation();
            FVector FinalDestination = MoveTarget.Center;
            float DesiredSpeedUsed = MoveTarget.DesiredSpeed.Get();
            float AcceptanceRadiusUsed = MoveTarget.SlackRadius;
            
            FMassClientPredictionFragment& Pred = PredList[i];
            if (Pred.bHasData)
            {
                FinalDestination = Pred.Location;
                if (Pred.PredDesiredSpeed > KINDA_SMALL_NUMBER)
                {
                    DesiredSpeedUsed = Pred.PredDesiredSpeed;
                }
                else if (DesiredSpeedUsed <= KINDA_SMALL_NUMBER)
                {
                    DesiredSpeedUsed = 100.f;
                }
                if (Pred.PredAcceptanceRadius > KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = Pred.PredAcceptanceRadius;
                }
                else if (AcceptanceRadiusUsed <= KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = 100.f;
                }
                // Clear prediction when server target converges to predicted (2D check)
                if (FVector::DistSquared2D(MoveTarget.Center, Pred.Location) <= FMath::Square(AcceptanceRadiusUsed))
                {
                    Pred.bHasData = false;
                }
            }

            
            if (CharFrag.bIsFlying)
            {
                if (FVector::DistSquared(CurrentLocation, FinalDestination) > FMath::Square(AcceptanceRadiusUsed))
                {
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                }
                continue;
            }
            
            if (const AUnitBase* UnitBase = Cast<AUnitBase>( ActorList[i].Get()))
            {
                if (const UCapsuleComponent* Capsule = UnitBase->GetCapsuleComponent())
                {
                    CurrentLocation.Z -= Capsule->GetScaledCapsuleHalfHeight()/2;
                }
            }
            
            if (FVector::DistSquared2D(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadiusUsed))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || FVector::DistSquared2D(PathFrag.PathTargetLocation, FinalDestination) > FMath::Square(1.f)) && !PathFrag.bIsPathfindingInProgress)
            {
                // Begin a new path request if we have navigation; otherwise, steer directly.
                if (bHasNavSystem)
                {
                    PathFrag.bIsPathfindingInProgress = true;
                    
                    // WICHTIG: Wir speichern exakt das ab, was wir anfragen (UNPROJIZIERT)
                    PathFrag.PathTargetLocation = FinalDestination;
                    
                    if (Pred.bHasData)
                    {
                        Pred.Location = FinalDestination;
                    }

                    FNavLocation ProjectedStartLocation;
                    if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                    {
                        // FIX A: Absolute Bremse sofort aktivieren, damit Momentum uns nicht in die Wand drückt!
                        Steering.DesiredVelocity = FVector::ZeroVector; 
                        
                        // Wir übergeben FinalDestination direkt an den Pathfinder!
                        RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, FinalDestination);
                    }
                    else
                    {
                        // GEÄNDERT: Kein Blindflug!
                        Steering.DesiredVelocity = FVector::ZeroVector;
                        PathFrag.bIsPathfindingInProgress = false;
                        PathFrag.ResetPath();
                    }
                }
                else
                {
                    // GEÄNDERT: DirectSteer für Bodentruppen OHNE Pfad ist gefährlich.
                    // Wenn sie keinen Pfad haben, sollen sie stehen bleiben, nicht fliegen!
                    Steering.DesiredVelocity = FVector::ZeroVector; 
                    PathFrag.bIsPathfindingInProgress = false;
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
                // GEÄNDERT: Kein blindes Vorwärtslaufen mehr! 
                // Wenn wir den Pfad berechnen, bleiben wir stehen, um nicht durch Wände zu glitchen.
                Steering.DesiredVelocity = FVector::ZeroVector; 
            }
            else if (PathFrag.HasValidPath())
            {
                // NEU: Wenn das Navmesh unter uns neu generiert wurde (EnergyWall gebaut),
                // verwerfen wir den alten Pfad sofort, damit wir im nächsten Frame neu planen!
                if (!PathFrag.CurrentPath->IsValid())
                {
                    PathFrag.ResetPath();
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    continue; 
                }

                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    if (FVector::DistSquared2D(CurrentLocation, PathPoints[PathFrag.CurrentPathPointIndex].Location) <= FMath::Square(PathWaypointAcceptanceRadius))
                    {
                        ++PathFrag.CurrentPathPointIndex;
                    }
                }

                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    FVector MoveDir = (PathPoints[PathFrag.CurrentPathPointIndex].Location - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                }
                else
                {
                    PathFrag.ResetPath();
                }
            }
            else
            {
                 // GEÄNDERT: DirectSteer für Bodentruppen OHNE Pfad ist gefährlich.
                 // Wenn sie keinen Pfad haben, sollen sie stehen bleiben, nicht fliegen!
                 Steering.DesiredVelocity = FVector::ZeroVector; 
            }
        }
    });
}

void UUnitMovementProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassSteeringFragment& Steering = SteeringList[i];
            Steering.DesiredVelocity = FVector::ZeroVector; // Default to stopped for this frame

            const FMassAIStateFragment& AIState = AIStateList[i];
            if (!AIState.CanMove || !AIState.IsInitialized)
            {
                continue;
            }
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            
            FVector CurrentLocation = CurrentMassTransform.GetLocation();
            FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius;

            if (CharFrag.bIsFlying)
            {
                if (FVector::DistSquared(CurrentLocation, FinalDestination) > FMath::Square(AcceptanceRadius))
                {
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                }
                continue; 
            }
            
            if (const AUnitBase* UnitBase = Cast<AUnitBase>( ActorList[i].Get()))
            {
                if (const UCapsuleComponent* Capsule = UnitBase->GetCapsuleComponent())
                {
                    CurrentLocation.Z -= Capsule->GetScaledCapsuleHalfHeight()/2;
                }
            }
     
            if (FVector::DistSquared2D(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadius))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress)
            {
                PathFrag.bIsPathfindingInProgress = true;
                
                // WICHTIG: Wir speichern exakt das ab, was wir anfragen (UNPROJIZIERT)
                PathFrag.PathTargetLocation = FinalDestination;

                FNavLocation ProjectedStartLocation;
                if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                {
                    // FIX A: Absolute Bremse sofort aktivieren, damit Momentum uns nicht in die Wand drückt!
                    Steering.DesiredVelocity = FVector::ZeroVector; 
                    
                    // Wir übergeben FinalDestination direkt an den Pathfinder!
                    RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, FinalDestination);
                }
                else
                {
                    // GEÄNDERT: Kein Blindflug mehr! Wenn wir nicht auf dem Navmesh sind, bleiben wir stehen.
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    PathFrag.bIsPathfindingInProgress = false; 
                    PathFrag.ResetPath();
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
            }
            else if (PathFrag.HasValidPath())
            {
                // NEU: Wenn das Navmesh unter uns neu generiert wurde (EnergyWall gebaut),
                // verwerfen wir den alten Pfad sofort, damit wir im nächsten Frame neu planen!
                if (!PathFrag.CurrentPath->IsValid())
                {
                    PathFrag.ResetPath();
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    continue; 
                }

                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    if (FVector::DistSquared2D(CurrentLocation, PathPoints[PathFrag.CurrentPathPointIndex].Location) <= FMath::Square(PathWaypointAcceptanceRadius))
                    {
                        PathFrag.CurrentPathPointIndex++;
                    }
                }

                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    FVector MoveDir = (PathPoints[PathFrag.CurrentPathPointIndex].Location - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                }
                else
                {
                    PathFrag.ResetPath();
                }
            }
            else
            {
                 // GEÄNDERT: Auch auf dem Server darf eine Bodentruppe ohne Pfad nicht einfach stur geradeaus laufen.
                 Steering.DesiredVelocity = FVector::ZeroVector; 
            }
        }
    });
}


void UUnitMovementProcessor::RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation)
{
    UWorld* World = GetWorld();
    if (!World) return;

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        ResetPathfindingFlagDeferred(Entity);
        return;
    }

    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
    if (!NavData)
    {
        ResetPathfindingFlagDeferred(Entity);
        return;
    }
    
    const bool bClientWorld = World->IsNetMode(NM_Client);

    if (!CachedStrictFilter.IsValid())
    {
        FSharedNavQueryFilter NewFilter = NavData->GetDefaultQueryFilter()->GetCopy();
        // GEÄNDERT: Exkludiere NUR die EnergyWall! Basis bleibt erreichbar.
        NewFilter->SetExcludedArea(NavData->GetAreaID(UNavArea_EnergyWall::StaticClass()));
        CachedStrictFilter = NewFilter;
    }
    
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [NavSystem, NavData, Entity, StartLocation, EndLocation, World, bClientWorld, StrictFilter = CachedStrictFilter] () mutable
    {
        // --- 1. STRICT MODE: Exklusion hartcodieren ---
        // Wir nutzen den gecachten Filter
        
        FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation, StrictFilter);
        
        // Erlaubt Partial Paths: Er plant bis zum Ufer, weil die Insel jetzt mathematisch "unendlich" weit weg ist.
        Query.SetAllowPartialPaths(true); 

        FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);

        // --- 2. ESCAPE MODE FALLBACK ---
        if (PathResult.Result == ENavigationQueryResult::Fail)
        {
            // Inline Escape Filter (Erlaubt alles wieder)
            FSharedConstNavQueryFilter EscapeFilter = NavData->GetDefaultQueryFilter();
            
            FPathFindingQuery EscapeQuery(nullptr, *NavData, StartLocation, EndLocation, EscapeFilter);
            EscapeQuery.SetAllowPartialPaths(true);

            PathResult = NavSystem->FindPathSync(EscapeQuery, EPathFindingMode::Regular);

            if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 0)
            {
                const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
                
                // Auf was für einem Polygon STARTEN wir?
                bool bStartsInWall = false;
                
                for (int32 i = 0; i < PathPoints.Num(); ++i)
                {
                    if (PathPoints[i].HasNodeRef())
                    {
                        const UClass* AreaClass = NavData->GetAreaClass(PathPoints[i].NodeRef);
                        // GEÄNDERT: Prüfe auf EnergyWall
                        bStartsInWall = AreaClass && AreaClass->IsChildOf(UNavArea_EnergyWall::StaticClass());
                        break; 
                    }
                }

                if (!bStartsInWall)
                {
                    PathResult.Result = ENavigationQueryResult::Fail;
                    PathResult.Path->ResetForRepath();
                }
                else
                {
                    int32 CutOffIndex = -1;
                    for (int32 i = 0; i < PathPoints.Num(); ++i)
                    {
                        if (!PathPoints[i].HasNodeRef()) continue;
                        const UClass* AreaClass = NavData->GetAreaClass(PathPoints[i].NodeRef);
                        // GEÄNDERT: Prüfe auf EnergyWall
                        const bool bIsObstacle = AreaClass && AreaClass->IsChildOf(UNavArea_EnergyWall::StaticClass());

                        if (!bIsObstacle) {
                            // FIX B: ANTI-TUNNELING CHECK
                            float DistToSafeGround = FVector::Dist2D(StartLocation, PathPoints[i].Location);
                            if (DistToSafeGround > 120.0f) // Toleranzwert für Wanddicke
                            {
                                PathResult.Result = ENavigationQueryResult::Fail;
                                PathResult.Path->ResetForRepath();
                                CutOffIndex = -1; 
                                break;
                            }

                            CutOffIndex = i + 1; 
                            break;
                        }
                    }

                    if (CutOffIndex > 0 && CutOffIndex < PathPoints.Num())
                    {
                        TArray<FNavPathPoint>& MutablePoints = const_cast<TArray<FNavPathPoint>&>(PathResult.Path->GetPathPoints());
                        MutablePoints.SetNum(CutOffIndex);
                    }
                }
            }
        }
        
        AsyncTask(ENamedThreads::GameThread,
            [Entity, PathResult, World, EndLocation, bClientWorld]() mutable
        {
            if (!World) return;

            UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
            if (!EntitySubsystem) return;

            FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

            if (!EntityManager.IsEntityValid(Entity)) return;

            EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
                [Entity, PathResult, EndLocation, bClientWorld](FMassEntityManager& System)
                {
                    if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
                    {
                        PathFrag->bIsPathfindingInProgress = false;

                        const bool bTargetChanged = FVector::DistSquared2D(PathFrag->PathTargetLocation, EndLocation) > FMath::Square(10.f);
                        if (bTargetChanged)
                        {
                            return; 
                        }

                        if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                        {
                            PathFrag->CurrentPath = PathResult.Path;
                            PathFrag->CurrentPathPointIndex = 1;

                            const FVector PathEndLoc = PathResult.Path->GetEndLocation();
                            if (FVector::DistSquared(PathEndLoc, EndLocation) > FMath::Square(10.0f))
                            {
                                PathFrag->PathTargetLocation = PathEndLoc;
                                if (FMassMoveTargetFragment* TargetFrag = System.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                                {
                                    TargetFrag->Center = PathEndLoc;
                                }
                            }
                            else
                            {
                                PathFrag->PathTargetLocation = EndLocation;
                            }
                        }
                        else
                        {
                            PathFrag->ResetPath();
                        }
                    }
                });
        });
    });
}

// Create a new deferred version of your helper function
void UUnitMovementProcessor::ResetPathfindingFlagDeferred(FMassEntityHandle Entity)
{
     if (!EntitySubsystem) return; // Make sure EntitySubsystem is valid
     
     FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
     if (!EntityManager.IsEntityValid(Entity)) return;

     EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
         [Entity](FMassEntityManager& System)
         {
             if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
             {
                 PathFrag->bIsPathfindingInProgress = false;
             }
         });
}