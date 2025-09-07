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
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
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
    // VVVV --- ADD THIS LINE --- VVVV
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // ^^^^ --- ADD THIS LINE --- ^^^^
    // Optionally Read Velocity if needed for decisions (but don't write it here)
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);

    // Add relevant tags (like FUnitMassTag)
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);

    // 2. State Tag (Must have EITHER Run OR Chase)
    // Using EMassFragmentPresence::Any creates an OR group for these tags.
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
   // EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    //EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    // Add other tags like Dead/Rooted checks if necessary

    EntityQuery.RegisterWithProcessor(*this);
}



void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem) return;

    if (!EntitySubsystem) return;

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
                // --- FLYING UNIT LOGIC ---
                // Flying units ignore navmesh and move directly to the target.
                if (FVector::DistSquared(CurrentLocation, FinalDestination) > FMath::Square(AcceptanceRadius))
                {
                    //FinalDestination.Z += CharFrag.FlyHeight;
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                }
                // Pathing and ground logic is skipped entirely for flying units.
                continue; 
            }
            
            // --- GROUND UNIT LOGIC (existing code) ---
            // State 0: Set path Current to Bottom.
            if (const AUnitBase* UnitBase = Cast<AUnitBase>( ActorList[i].Get()))
            {
                if (const UCapsuleComponent* Capsule = UnitBase->GetCapsuleComponent())
                {
                    CurrentLocation.Z -= Capsule->GetScaledCapsuleHalfHeight()/2;
                }
            }
     
            // State 1: Unit has arrived at its destination.
            if (FVector::DistSquared(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadius))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
                // Steering is already zero, so unit will stop.
            }
            // State 2: Unit needs a new path.
            else if ((!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress)
            {
                PathFrag.bIsPathfindingInProgress = true;
                PathFrag.PathTargetLocation = FinalDestination;

                FNavLocation ProjectedDestinationLocation;
                if (NavSystem->ProjectPointToNavigation(FinalDestination, ProjectedDestinationLocation, NavMeshProjectionExtent))
                {
                    
                    // The destination is on the navmesh. Now check if our current location is.
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
                    // SUGGESTION: Add a log here to confirm this code is running!
                    //UE_LOG(LogTemp, Warning, TEXT("Entity %d: Target %s is unreachable."), Entity.Index, *FinalDestination.ToString());
                    MoveTarget.Center = CurrentLocation;
                    PathFrag.bIsPathfindingInProgress = false;
                    PathFrag.ResetPath();
                }
            }
            // State 3: Unit is waiting for a path to be generated.
            else if (PathFrag.bIsPathfindingInProgress)
            {
                // Do nothing. Unit stands still while waiting.
            }
            // State 4: Unit has a valid path and is following it.
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
                    // Fall through to the final 'else' which will handle the last leg of the journey.
                }
            }
            
            // FINAL FIX: This is now the FINAL 'else', ensuring it only runs if ALL other conditions fail.
            // State 5: Fallback - No path and not waiting, so move directly to the destination.
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

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [NavSystem, NavData, QueryFilter, Entity, StartLocation, EndLocation, World] () mutable
    {
        // --- Runs on a Background Thread ---
        FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation, QueryFilter);
        Query.SetAllowPartialPaths(true);

        FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);
        
        AsyncTask(ENamedThreads::GameThread,
            [Entity, PathResult, World]() mutable // Note: EndLocation is no longer needed in this capture list
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
                [Entity, PathResult](FMassEntityManager& System) // Note: EndLocation is no longer needed in this capture list
                {
                    // This lambda now runs at a safe time.
                    if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
                    {
                        PathFrag->bIsPathfindingInProgress = false; // Reset flag regardless of success/failure

                        if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                        {
                            PathFrag->CurrentPath = PathResult.Path;
                            PathFrag->CurrentPathPointIndex = 1;

                            // CRITICAL FIX: DO NOT UPDATE PathTargetLocation HERE.
                            // Updating it to the projected location was causing an infinite loop
                            // because it would no longer match the original target from the MoveTargetFragment.
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