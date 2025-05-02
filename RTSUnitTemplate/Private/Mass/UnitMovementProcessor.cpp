// Source: UnitMovementProcessor.cpp (Changes)
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

UUnitMovementProcessor::UUnitMovementProcessor()
{
    // Run BEFORE steering, avoidance, and movement integration
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; // Or potentially Input
    ProcessingPhase = EMassProcessingPhase::PrePhysics; // Good phase for pathfinding/intent

    //ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
    //ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UUnitMovementProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());
}

void UUnitMovementProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);        // READ current position
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);  // READ the target location/speed
    EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite); // WRITE desired velocity
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite); // Keep for managing path state

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

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);   // ...OR if this tag is present.
    // Add other tags like Dead/Rooted checks if necessary

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Time accumulation (optional, depending if you want interval execution)
    // TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    // if (TimeSinceLastRun < ExecutionInterval) return;
    // TimeSinceLastRun -= ExecutionInterval;

    UWorld* World = GetWorld(); // Get World from Processor's context
    if (!World) return;

    //UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem) return; // Needed for async result application

    // --- Path Following & Steering Logic ---
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            FMassSteeringFragment& Steering = SteeringList[i];

            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius;
            const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);

            // Reset steering for this frame
            Steering.DesiredVelocity = FVector::ZeroVector;

            // 1. Check if already at the final destination
            if (FVector::DistSquared(CurrentLocation, FinalDestination) <= AcceptanceRadiusSq)
            {
                if (PathFrag.HasValidPath() || PathFrag.bIsPathfindingInProgress) // Also reset if pathfinding was in progress
                {
                     PathFrag.ResetPath();
                     PathFrag.bIsPathfindingInProgress = false; // Ensure flag is cleared
                }

                // Signal Idle only if not already Idle potentially
                 UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (SignalSubsystem)
                {
                     SignalSubsystem->SignalEntity(UnitSignals::Idle, Entity); // Use your actual signal name
                }
                continue; // Stop processing movement for this entity
            }

            // 2. Manage Path Request
            // Need a new path if: No valid path exists AND path target is wrong AND no pathfinding is currently running
            bool bNeedsNewPath = (!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress;

            if (bNeedsNewPath)
            {
                // Set the flag immediately to prevent requests on subsequent frames
                PathFrag.bIsPathfindingInProgress = true;
                PathFrag.PathTargetLocation = FinalDestination; // Store intended target
                // Request pathfinding asynchronously
                RequestPathfindingAsync(Entity, CurrentLocation, FinalDestination); // Pass necessary info
            }

            // 3. Follow Existing Path (if available)
            FVector CurrentTargetPoint = FinalDestination; // Default to final goal if no path points
            if (PathFrag.HasValidPath())
            {
                 const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                 if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex)) // Check if index is valid
                 {
                     CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;

                     // Check if the current waypoint is reached
                     const float WaypointAcceptanceRadiusSq = FMath::Square(PathWaypointAcceptanceRadius); // Use defined radius
                     if (FVector::DistSquared(CurrentLocation, CurrentTargetPoint) <= WaypointAcceptanceRadiusSq)
                     {
                         PathFrag.CurrentPathPointIndex++;
                         // Check if we reached the end of the path points
                         if (!PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                         {
                             // Reached end of calculated path points, but maybe not final destination yet
                             // Continue towards final destination. A new path might be requested next frame if needed.
                             CurrentTargetPoint = FinalDestination;
                              PathFrag.ResetPath(); // Reset path progress, keep PathTargetLocation
                                                    // Keep bIsPathfindingInProgress as is (should be false here)
                         }
                         else
                         {
                             // Move to next point
                             CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;
                         }
                     }
                 }
                 else // Invalid index, path is broken somehow
                 {
                    PathFrag.ResetPath(); // Reset path progress
                    CurrentTargetPoint = FinalDestination;
                    // Keep bIsPathfindingInProgress as is
                 }
            }
            // If !PathFrag.HasValidPath(), CurrentTargetPoint remains FinalDestination

            // 4. Calculate Steering towards CurrentTargetPoint
            FVector MoveDir = CurrentTargetPoint - CurrentLocation;
            if (!MoveDir.IsNearlyZero(0.1f)) // Use a small tolerance
            {
                MoveDir.Normalize();
                Steering.DesiredVelocity = MoveDir * DesiredSpeed;
            }

            //UE_LOG(LogTemp, Log, TEXT("Entity [%d] MovementIntent: DesiredVelocity = %s"),
              //Entity.Index, *Steering.DesiredVelocity.ToString());
            // If MoveDir is zero (e.g., exactly at CurrentTargetPoint), DesiredVelocity remains ZeroVector
        } // End loop through entities
    }); // End ForEachEntityChunk
}



void UUnitMovementProcessor::RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation)
{
    UWorld* World = GetWorld(); // Get World from Processor context
    if (!World) return;

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        // Failed to get NavSystem, need to reset the flag on the entity
        ResetPathfindingFlag(Entity);
        return;
    }

    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
    if (!NavData)
    {
        // Failed to get NavData, need to reset the flag on the entity
        ResetPathfindingFlag(Entity);
        return;
    }

    // Consider caching the filter if it's static or retrieving it based on entity type
    FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());
    // --- Capture necessary data for the async task ---
    // Capture by value where possible, especially for data used across threads.
    // Shared pointers (like QueryFilter, NavPath) are thread-safe reference counted.
    // Entity handle is just an ID.

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, // Or another appropriate thread pool
        [NavSystem, NavData, QueryFilter, Entity, StartLocation, EndLocation, World] () mutable // Capture necessary vars
    {
        // --- Runs on a Background Thread ---
        FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation, QueryFilter);
        Query.SetAllowPartialPaths(true); // Or false, depending on requirements

        // FindPathSync will block THIS background thread, not the game thread
        FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);
        //FPathFindingResult PathResult = NavSystem->FindPathSync(Query);
        // --- Pathfinding Complete - Schedule result application back on Game Thread ---
        AsyncTask(ENamedThreads::GameThread,
            [Entity, PathResult, EndLocation, World]() mutable // Capture result and entity handle
        {
            // --- Runs back on the Game Thread ---
            if (!World) return; // World might become invalid

                UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
                if (!EntitySubsystem) 
                {
                    return; // Subsystem missing
                }


             FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

                if (!EntityManager.IsEntityValid(Entity))
                {
                    // Entity is no longer valid, nothing to do
                    return; 
                }
                
             FUnitNavigationPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity);

             if (!PathFrag)
             {
                 // Fragment removed, nothing to do
                 return;
             }

            // --- Apply Path Result ---
            PathFrag->bIsPathfindingInProgress = false; // Reset flag regardless of success/failure

            if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
            {
                PathFrag->CurrentPath = PathResult.Path;
                PathFrag->CurrentPathPointIndex = 1; // Start moving towards the second point
                PathFrag->PathTargetLocation = EndLocation; // Confirm the target this path was for
                 // UE_LOG(LogTemp, Log, TEXT("Entity %d: Async path found successfully."), Entity.Index);
            }
            else
            {
                // Pathfinding failed or resulted in an invalid path
                PathFrag->ResetPath(); // Clear any old path data
                 // Keep PathTargetLocation as it might be used to retry pathfinding
                 // UE_LOG(LogTemp, Warning, TEXT("Entity %d: Async pathfinding failed or path invalid."), Entity.Index);
                 // Maybe signal a specific state here if needed
            }
        }); // End Game Thread AsyncTask Lambda
    }); // End Background Thread AsyncTask Lambda
}

// Helper function to reset the flag if async task cannot even start
void UUnitMovementProcessor::ResetPathfindingFlag(FMassEntityHandle Entity)
{
     UWorld* World = GetWorld();
     if (!World) return;
     //UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
      if (!EntitySubsystem) return;

      FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();


        if (!EntityManager.IsEntityValid(Entity))
        {
            // Entity is no longer valid, nothing to do
            return; 
        }
    
      FUnitNavigationPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity);
      if (PathFrag)
      {
          PathFrag->bIsPathfindingInProgress = false;
      }
}