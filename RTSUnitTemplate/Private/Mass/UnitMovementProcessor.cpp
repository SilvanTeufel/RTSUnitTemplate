// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitMovementProcessor.h"

// ... other includes ...
#include "MassExecutionContext.h"
#include "Steering/MassSteeringFragments.h" // Ensure included
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "NavFilters/NavigationQueryFilter.h"
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
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client] UUnitMovementProcessor: EntitySubsystem not yet available; skipping this tick."));
            }
            return;
        }
    }

    if (World->IsNetMode(NM_Client))
    {
        static int32 GUnitMoveExecTickCounter = 0;
        if ((++GUnitMoveExecTickCounter % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Execute tick"));
            }
        }
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

        int32 SkippedCannotMove = 0;
        int32 SkippedNotInit = 0;
        int32 Arrivals = 0;
        int32 PathRequests = 0;
        int32 FollowingPath = 0;
        int32 DirectSteer = 0;
        int32 WaypointAdvance = 0;
        int32 PathfindingInProgressCount = 0;

        static int32 GUnitMoveClientChunkCounter = 0;
        if (((++GUnitMoveClientChunkCounter) % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] ExecuteClient: Entities=%d, HasNav=%d"), NumEntities, bHasNavSystem ? 1 : 0);
            }
        }
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassSteeringFragment& Steering = SteeringList[i];
            Steering.DesiredVelocity = FVector::ZeroVector;

            const FMassAIStateFragment& AIState = AIStateList[i];
            
            if (!AIState.IsInitialized)
            {
                ++SkippedNotInit;
                continue;
            }
            if (!AIState.CanMove)
            {
                ++SkippedCannotMove;
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
                    if (bShowLogs)
                    {
                        if (const AActor* PredActor = ActorList[i].Get())
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Using fallback speed for %s: %.1f (MoveTarget.DesiredSpeed was %.2f)"), *PredActor->GetName(), DesiredSpeedUsed, MoveTarget.DesiredSpeed.Get());
                        }
                    }
                }
                if (Pred.PredAcceptanceRadius > KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = Pred.PredAcceptanceRadius;
                }
                else if (AcceptanceRadiusUsed <= KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = 100.f;
                }
                if (bShowLogs)
                {
                    if (const AActor* PredActor = ActorList[i].Get())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Active for %s -> Dest=%s (SrvTarget=%s), Speed=%.1f, AccRad=%.1f"),
                            *PredActor->GetName(), *FinalDestination.ToString(), *MoveTarget.Center.ToString(), DesiredSpeedUsed, AcceptanceRadiusUsed);
                    }
                }
                // Clear prediction when server target converges to predicted (2D check)
                if (FVector::DistSquared2D(MoveTarget.Center, Pred.Location) <= FMath::Square(AcceptanceRadiusUsed))
                {
                    Pred.bHasData = false;
                    if (bShowLogs)
                    {
                        if (const AActor* PredActor = ActorList[i].Get())
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Reconciled for %s (server MoveTarget matched predicted)"), *PredActor->GetName());
                        }
                    }
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
                ++Arrivals;
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || FVector::DistSquared2D(PathFrag.PathTargetLocation, FinalDestination) > FMath::Square(1.f)) && !PathFrag.bIsPathfindingInProgress)
            {
                // Begin a new path request if we have navigation; otherwise, steer directly.
                if (bHasNavSystem)
                {
                    ++PathRequests;
                    PathFrag.bIsPathfindingInProgress = true;

                    FNavLocation ProjectedDestinationLocation;
                    if (NavSystem->ProjectPointToNavigation(FinalDestination, ProjectedDestinationLocation, NavMeshProjectionExtent))
                    {
                        // Store the projected destination as our target to keep consistent with async result
                        PathFrag.PathTargetLocation = ProjectedDestinationLocation.Location;
                        // Keep predicted location consistent too (optional)
                        if (Pred.bHasData)
                        {
                            Pred.Location = ProjectedDestinationLocation.Location;
                        }

                        FNavLocation ProjectedStartLocation;
                        if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                        {
                            if (bShowLogs)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Start pathfinding: Start=%s Dest=%s"), *ProjectedStartLocation.Location.ToString(), *ProjectedDestinationLocation.Location.ToString());
                            }
                            RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, ProjectedDestinationLocation.Location);
                        }
                        else
                        {
                            if (bShowLogs)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Project start failed for %s; steering directly"), *CurrentLocation.ToString());
                            }
                            FVector MoveDir = (ProjectedDestinationLocation.Location - CurrentLocation).GetSafeNormal();
                            Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                            PathFrag.bIsPathfindingInProgress = false;
                        }
                    }
                    else
                    {
                        if (bShowLogs)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Project destination failed for %s; prediction-safe cancel (no client MoveTarget write)"), *FinalDestination.ToString());
                        }
                        // Prediction-safe: never write to authoritative MoveTarget on clients
                        PathFrag.bIsPathfindingInProgress = false;
                        PathFrag.ResetPath();
                        // Optionally steer directly toward the unprojected destination this tick
                        FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                        if (MoveDir.SizeSquared() > KINDA_SMALL_NUMBER)
                        {
                            Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                        }
                    }
                }
                else
                {
                    // No navmesh on client: steer straight to destination for prediction
                    ++DirectSteer;
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    if (MoveDir.SizeSquared() > KINDA_SMALL_NUMBER)
                    {
                        Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                    }
                    PathFrag.bIsPathfindingInProgress = false;
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
                ++PathfindingInProgressCount;
                // Keep moving while waiting for async path result to avoid visible freeze
                FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                if (MoveDir.SizeSquared() > KINDA_SMALL_NUMBER)
                {
                    Steering.DesiredVelocity = MoveDir * (DesiredSpeedUsed * 0.5f);
                }
            }
            else if (PathFrag.HasValidPath())
            {
                ++FollowingPath;
                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    if (FVector::DistSquared(CurrentLocation, PathPoints[PathFrag.CurrentPathPointIndex].Location) <= FMath::Square(PathWaypointAcceptanceRadius))
                    {
                        ++WaypointAdvance;
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
                 ++DirectSteer;
                 FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                 if (MoveDir.SizeSquared() > KINDA_SMALL_NUMBER)
                 {
                    Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                 }
            }
        }

        static int32 GUnitMoveClientSummaryCounter = 0;
        if (((++GUnitMoveClientSummaryCounter) % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Summary: Arrivals=%d, PathReq=%d, Following=%d, InProgress=%d, Direct=%d, WaypointAdvance=%d, SkippedInit=%d, SkippedCanMove=%d"),
                    Arrivals, PathRequests, FollowingPath, PathfindingInProgressCount, DirectSteer, WaypointAdvance, SkippedNotInit, SkippedCannotMove);
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
     
            if (FVector::DistSquared(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadius))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress)
            {
                PathFrag.bIsPathfindingInProgress = true;
                PathFrag.PathTargetLocation = FinalDestination;

                FNavLocation ProjectedDestinationLocation;
                if (NavSystem->ProjectPointToNavigation(FinalDestination, ProjectedDestinationLocation, NavMeshProjectionExtent))
                {
                    FNavLocation ProjectedStartLocation;
                    if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                    {
                        RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, ProjectedDestinationLocation.Location);
                    }
                    else
                    {
                        FVector MoveDir = (ProjectedDestinationLocation.Location - CurrentLocation).GetSafeNormal();
                        Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                        PathFrag.bIsPathfindingInProgress = false; // We are not pathfinding, just steering.
                    }
                }
                else
                {
                    MoveTarget.Center = CurrentLocation;
                    PathFrag.bIsPathfindingInProgress = false;
                    PathFrag.ResetPath();
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
            }
            else if (PathFrag.HasValidPath())
            {
                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    if (FVector::DistSquared(CurrentLocation, PathPoints[PathFrag.CurrentPathPointIndex].Location) <= FMath::Square(PathWaypointAcceptanceRadius))
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
                 FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                 if (MoveDir.SizeSquared() > KINDA_SMALL_NUMBER)
                 {
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                 }
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
    
    FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());

    const bool bClientWorld = World->IsNetMode(NM_Client);
    const bool bLog = bShowLogs;
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [NavSystem, NavData, QueryFilter, Entity, StartLocation, EndLocation, World, bClientWorld, bLog] () mutable
    {
        // --- Runs on a Background Thread ---
        FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation, QueryFilter);
        Query.SetAllowPartialPaths(true);

        FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);
        
        AsyncTask(ENamedThreads::GameThread,
            [Entity, PathResult, World, EndLocation, bClientWorld, bLog]() mutable
        {
            // --- Runs back on the Game Thread ---
            if (!World) return;

            UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
            if (!EntitySubsystem) return;

            FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

            if (!EntityManager.IsEntityValid(Entity))
            {
                return;
            }

            EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
                [Entity, PathResult, EndLocation, bClientWorld, bLog](FMassEntityManager& System)
                {
                    // This lambda now runs at a safe time.
                    if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
                    {
                        PathFrag->bIsPathfindingInProgress = false; // Reset flag regardless of success/failure

                        // Drop stale results if target changed (compare in XY only, ignore Z disparities)
                        const bool bTargetChanged = FVector::DistSquared2D(PathFrag->PathTargetLocation, EndLocation) > FMath::Square(1.f);
                        if (bTargetChanged)
                        {
                            if (bClientWorld && bLog)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Dropping stale path result (target changed) TargetNow=%s Requested=%s"),
                                    *PathFrag->PathTargetLocation.ToString(), *EndLocation.ToString());
                            }
                            return; // do not apply
                        }

                        if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                        {
                            const int32 NumPts = PathResult.Path->GetPathPoints().Num();
                            if (bClientWorld && bLog)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Path result: SUCCESS points=%d"), NumPts);
                            }
                            PathFrag->CurrentPath = PathResult.Path;
                            PathFrag->CurrentPathPointIndex = 1;
                        }
                        else
                        {
                            if (bClientWorld && bLog)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[Client][UnitMovement] Path result: FAIL (IsSuccessful=%d, Valid=%d, Points=%d)"),
                                    PathResult.IsSuccessful() ? 1 : 0,
                                    PathResult.Path.IsValid() ? 1 : 0,
                                    PathResult.Path.IsValid() ? PathResult.Path->GetPathPoints().Num() : 0);
                            }
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