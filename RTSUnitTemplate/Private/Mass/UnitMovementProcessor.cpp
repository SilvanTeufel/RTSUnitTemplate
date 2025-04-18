// Source: UnitMovementProcessor.cpp
/*
#include "Mass/UnitMovementProcessor.h" // Passe Pfad an (Make sure this path is correct for your project)
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"      // Defines FMassMoveTargetFragment, FMassSteeringFragment, FMassVelocityFragment
#include "MassNavigationFragments.h"    // Defines FMassNavigationPathFragment
#include "MassActorSubsystem.h"         // Often needed for world access or other utilities
#include "MassExecutionContext.h"
#include "MassEntityManager.h"          // **ADDED**: Required for EntityManager.HasTag
#include "NavigationSystem.h"           // Defines UNavigationSystemV1
#include "NavigationData.h"             // Defines ANavigationData
#include "NavFilters/NavigationQueryFilter.h" // Defines UNavigationQueryFilter
#include "Steering/MassSteeringFragments.h" // Include path relative to MassNavigation/Public
#include "MassNavigationTypes.h"        // Defines related navigation types for Mass
#include "Mass/UnitNavigationFragments.h" // **REMOVED/COMMENTED**: Assuming you use the standard FMassNavigationPathFragment from MassNavigationFragments.h. If FMassNavigationPathFragment IS defined here, uncomment this and potentially comment out #include "MassNavigationFragments.h" - but using the standard one is recommended.
#include "Mass/UnitMassTag.h"           // FUnitMassTag (Passe Pfad an - Make sure this path is correct)
#include "MassCommonFragments.h" 
#include "MassStateTreeFragments.h"     // **ADDED**: May be needed depending on where FMassState...Tags are fully defined (e.g. FMassStateDeadTag)
#include "MassSignalSubsystem.h"        // Potentially useful for signalling events like arrival

// --- Define missing constant (or make it a UPROPERTY) ---
// This was used but not defined in the snippet. Define it here or,
// preferably, make it a member variable (UPROPERTY) of the class.
namespace // Use an anonymous namespace for file-local constants
{
    const float PathWaypointAcceptanceRadiusFactor = 1.5f; // Example value
}


UUnitMovementProcessor::UUnitMovementProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UUnitMovementProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);       // Now fully defined
    // Optional: Velocity
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);

    // Tags
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);           // Custom tag
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);     // Standard state tags
    EntityQuery.AddTagRequirement<FMassStateRootedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassReachedDestinationTag>(EMassFragmentPresence::Optional); // **ADDED**: Need to declare we might access this tag for HasTag checks/removals

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(EntityManager.GetWorld());
    if (!NavSystem) return;

    // GetDefaultNavDataInstance can be expensive if called every frame for many agents.
    // Consider caching it if performance becomes an issue, but ensure it remains valid.
    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
    if (!NavData) return;

    // Cache the query filter as well
    // Note: Using GetDefaultQueryFilter() might be slightly more robust if filter classes change.
    FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
            
        // Check if the archetype *can* have the tag. If not, EntityManager.HasTag would always be false anyway.
        const bool bArchetypeCanHaveReachedTag = ChunkContext.DoesArchetypeHaveTag<FMassReachedDestinationTag>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            FMassSteeringFragment& Steering = SteeringList[i];

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FVector CurrentLocation = Transform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius; // Use the slack radius from the target fragment
            const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);
            const EMassMovementAction CurrentAction = MoveTarget.GetCurrentAction();

            // Reset steering for this frame
            Steering.Reset(); // Sets DesiredSpeed and Force to 0

            // --- 1. Movement Intent Check ---
            if (CurrentAction != EMassMovementAction::Move || DesiredSpeed <= KINDA_SMALL_NUMBER)
            {
                if (PathFrag.HasValidPath()) PathFrag.ResetPath();
                // Remove tag if present (using the archetype check as a potential micro-optimization)
              
                     ChunkContext.Defer().RemoveTag<FMassReachedDestinationTag>(Entity);
                
                continue; // No movement desired, process next entity
            }

            // --- 2. Final Destination Arrival Check ---
            const float DistToFinalDestSq = FVector::DistSquared(CurrentLocation, FinalDestination);
            bool bIsAtFinalDestination = DistToFinalDestSq <= AcceptanceRadiusSq;

            if (bIsAtFinalDestination)
            {
                if (PathFrag.HasValidPath()) PathFrag.ResetPath();
                // Add tag if not already present (use EntityManager.HasTag for accurate per-entity check)
                // Note: Checking EntityManager is crucial here, as the archetype check only tells if *some* entity in the chunk *might* have it.
     
                ChunkContext.Defer().AddTag<FMassReachedDestinationTag>(Entity);
   
                // Steering remains ZeroVector (already reset), process next entity
                continue;
            }
            else
            {

                ChunkContext.Defer().RemoveTag<FMassReachedDestinationTag>(Entity);
                
            }

            // --- 3. Path Management ---
            bool bNeedsNewPath = !PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination;
            if (bNeedsNewPath)
            {
                // UE_LOG(LogMassNavigation, Verbose, TEXT("Entity [%s] needs new path."), *Entity.DebugGetDescription());
                PathFrag.ResetPath(); // Clear existing path data

                // Only request a path if we are reasonably far from the destination
                // Avoid pathfinding if already within the acceptance radius (or slightly outside)
                 const float PathRequestThresholdSq = FMath::Square(AcceptanceRadius * 1.1f); // Example threshold slightly larger than acceptance
                if (DistToFinalDestSq > PathRequestThresholdSq)
                {
                    // Prepare pathfinding query
                    FPathFindingQuery Query(this, *NavData, CurrentLocation, FinalDestination, QueryFilter);
                    Query.SetAllowPartialPaths(false); // Or true, depending on desired behavior

                    // Execute synchronous pathfinding
                    // WARNING: FindPathSync can be slow if called for many agents in one frame.
                    // Consider asynchronous pathfinding (NavSystem->FindPathAsync) for better performance.
                    FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);

                    if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                    {
                        // Store the found path
                        PathFrag.CurrentPath = PathResult.Path;
                        PathFrag.CurrentPathPointIndex = 1; // Start moving towards the second point (index 1)
                        PathFrag.PathTargetLocation = FinalDestination; // Store the target this path was generated for
                       // UE_LOG(LogMassNavigation, Log, TEXT("Entity [%s] Path found with %d points."), *Entity.DebugGetDescription(), PathResult.Path->GetPathPoints().Num());
                    }
                    // else { UE_LOG(LogMassNavigation, Warning, TEXT("Entity [%s] Pathfinding failed (%s)."), *Entity.DebugGetDescription(), *PathResult.Getdescription()); }
                }
                 else
                 {
                      // We are close but not quite there, and don't have a path. Aim directly for the final destination (handled below).
                      // UE_LOG(LogMassNavigation, Verbose, TEXT("Entity [%s] Close to destination, skipping pathfind."), *Entity.DebugGetDescription());
                 }
            }

            // --- 4. Determine Current Move Target Point ---
            FVector CurrentTargetPoint = FinalDestination; // Default: move directly towards the final destination
            bool bFollowingPath = false;

            if (PathFrag.HasValidPath())
            {
                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();

                // Check if the current path point index is valid
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    bFollowingPath = true;
                    CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;

                    // Calculate acceptance radius for the current waypoint (e.g., based on agent radius)
                    const float WaypointAcceptanceRadius = 100.f * PathWaypointAcceptanceRadiusFactor;
                    const float WaypointAcceptanceRadiusSq = FMath::Square(WaypointAcceptanceRadius);

                    // Check if the current waypoint is reached
                    const float DistToWaypointSq = FVector::DistSquared(CurrentLocation, CurrentTargetPoint);
                    if (DistToWaypointSq <= WaypointAcceptanceRadiusSq)
                    {
                        PathFrag.CurrentPathPointIndex++;
                        // UE_LOG(LogMassNavigation, Verbose, TEXT("Entity [%s] reached waypoint, advancing to index %d"), *Entity.DebugGetDescription(), PathFrag.CurrentPathPointIndex);

                        // Check if we reached the end of the path points
                        if (!PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                        {
                            // UE_LOG(LogMassNavigation, Log, TEXT("Entity [%s] reached end of path."), *Entity.DebugGetDescription());
                            PathFrag.ResetPath(); // Path completed
                            bFollowingPath = false;
                            CurrentTargetPoint = FinalDestination; // Target the final destination directly now
                        }
                        else
                        {
                            // Update target to the next waypoint
                            CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;
                        }
                    }
                     // else: Still moving towards the current waypoint (CurrentTargetPoint is already set)
                }
                else // Invalid path point index (shouldn't happen often if logic is correct, but good safeguard)
                {
                    // UE_LOG(LogMassNavigation, Warning, TEXT("Entity [%s] Invalid path index (%d), resetting path."), *Entity.DebugGetDescription(), PathFrag.CurrentPathPointIndex);
                    PathFrag.ResetPath();
                    bFollowingPath = false;
                    CurrentTargetPoint = FinalDestination; // Fallback to final destination
                }
            }
            // else: No valid path exists, CurrentTargetPoint remains FinalDestination

            // --- 5. Calculate Steering ---
            FVector DesiredDirection = CurrentTargetPoint - CurrentLocation;
            const float DesiredDirectionMagSq = DesiredDirection.SizeSquared();

            // Only apply steering if we are not already at the target point (avoid NaN from normalization)
            if (DesiredDirectionMagSq > FMath::Square(KINDA_SMALL_NUMBER)) // Use squared comparison
            {
                // Get the current direction (normalized vector). Use GetSafeNormal to avoid issues with zero vectors.
                FVector CurrentDirection = Steering.DesiredVelocity.GetSafeNormal(); 
                // Apply desired speed and calculate steering force
                Steering.DesiredVelocity = CurrentDirection*DesiredSpeed;
                //Steering. = NormalizedDirection * DesiredSpeed; // Simple steering force calculation
            }
            else
            {
                // Already at the current target point (waypoint or close to final destination without path)
                // Steering remains reset (zero force/speed)

                // Redundant check removed: The main arrival check (Section 2) handles adding the FMassReachedDestinationTag
                // when the *final* destination is reached. Being at an intermediate waypoint doesn't mean arrival.
                // If !bFollowingPath and we are here, it means we didn't need a path (were close) or pathfinding failed.
                // The main arrival check (Section 2) covers the case where we are close enough to the *final* destination.
            }

            // --- 6. Movement Application (Deferred) ---
            // NO direct manipulation of TransformFragment here.
            // Other processors (like MassSteeringProcessor, MassAvoidanceProcessor, MassMovementProcessor)
            // will use the Steering.Force and Steering.DesiredSpeed later in the frame
            // to calculate the final velocity and update the transform.

        } // End loop through entities in chunk
    }); // End ForEachEntityChunk
}
*/

/*
#include "Mass/UnitMovementProcessor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "MassNavigationFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "NavFilters/NavigationQueryFilter.h"

UUnitMovementProcessor::UUnitMovementProcessor()
{
	// Set the processor to run in the Movement phase.
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; 
    ProcessingPhase = EMassProcessingPhase::PrePhysics; 
	bAutoRegisterWithProcessingPhases = true;
}

void UUnitMovementProcessor::ConfigureQueries()
{
	// Add necessary fragments that the processor will need to access
	//EntityQuery.RegisterWithProcessor(*this);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly); // READ the target location
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	// Optionally, add tags if needed (FMyUnitTag is an example)
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this); // Important! Otherwise Process will not run
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnitMovementProcessor: World is null."));
        return;
    }
    
    // Get the navigation system (assumes synchronous access is acceptable)
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnitMovementProcessor: NavSystem not found."));
        return;
    }

    // Iterate over entity chunks
    EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& ChunkContext)
    {


        const int32 NumEntities = ChunkContext.GetNumEntities();

        // Get fragment views
        auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        auto TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>(); // Read-only view
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        // Process each entity in the chunk
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FTransform& Transform = TransformList[i].GetMutableTransform();
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FVector CurrentLocation = Transform.GetLocation();
            FVector GoalLocation = MoveTarget.Center;

            const float DistanceToGoal = FVector::Dist(CurrentLocation, GoalLocation);
            const float StopDistance = 50.0f; // oder ein anderer kleiner Wert

            if (DistanceToGoal < StopDistance)
            {
                // Ziel erreicht – keine Bewegung mehr
                AUnitBase* UnitBase = Cast<AUnitBase>(ActorFragments[i].GetMutable());
                UnitBase->SetUnitState(UnitData::Idle);
                continue;
            }
            // Setup Navigation Query for this entity
            ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance();
            if (!NavData)
            {
                continue;
            }

            // Create a default query filter
            FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());

            // Construct the pathfinding query.
            // Note: The first parameter (QueryUser) is often your pawn or actor.
            FPathFindingQuery Query( nullptr, *NavData, CurrentLocation, GoalLocation, QueryFilter);

            // Perform synchronous pathfinding
            FPathFindingResult PathResult = NavSystem->FindPathSync(Query);
            FVector NextMoveLocation = GoalLocation; // Fallback to the goal

            if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
            {
                // Retrieve the path points. Often, the first point is the current position.
                const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
                if (PathPoints.Num() > 1)
                {
                    // Use the second point as the next target location along the path.
                    NextMoveLocation = PathPoints[1].Location;
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Entity pathfinding failed. Falling back to direct movement."));
            }

            // Calculate the movement direction toward the next move location
            FVector MoveDir = (NextMoveLocation - CurrentLocation);
            if (!MoveDir.IsNearlyZero())
            {
                MoveDir.Normalize();
                // Use the desired speed from the move target fragment
                const float Speed = MoveTarget.DesiredSpeed.Get();
                FVector Velocity = MoveDir * Speed;

                // Apply movement: update transform's translation
                Transform.AddToTranslation(Velocity * DeltaTime);
            }
        }
    });
}
*/

// Source: UnitMovementProcessor.cpp (Changes)
#include "Mass/UnitMovementProcessor.h"

// ... other includes ...
#include "MassExecutionContext.h"
#include "Steering/MassSteeringFragments.h" // Ensure included
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Mass/UnitMassTag.h"
#include "NavFilters/NavigationQueryFilter.h"
// Remove direct velocity/transform includes if no longer directly used
// #include "MassCommonFragments.h" // Keep if still reading Transform
// #include "MassMovementFragments.h" // Keep for FMassMoveTargetFragment

UUnitMovementProcessor::UUnitMovementProcessor()
{
    // Run BEFORE steering, avoidance, and movement integration
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; // Or potentially Input
    ProcessingPhase = EMassProcessingPhase::PrePhysics; // Good phase for pathfinding/intent
    bAutoRegisterWithProcessingPhases = true;
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
    // Add other tags like Dead/Rooted checks if necessary

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
 
    UWorld* World = EntityManager.GetWorld();
    if (!World) return; // Early out

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem) return;

    // Consider caching NavData if performance is an issue and world doesn't change NavData frequently
    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
    if (!NavData) return;

 
    // Consider caching the filter as well
    FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
    
        if (NumEntities == 0) return; // Check if chunk is empty

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
        // Potentially read actor fragments if needed for logic (e.g., UnitState)
        // const TConstArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetFragmentView<FMassActorFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds(); // May not be needed directly here anymore

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform(); // Use const ref
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            FMassSteeringFragment& Steering = SteeringList[i];
            // AUnitBase* UnitBase = ActorFragments.Num() > 0 ? Cast<AUnitBase>(ActorFragments[i].Get()) : nullptr; // Example getting actor state

            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius; // Use slack from target
            const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);

            // Reset steering for this frame (IMPORTANT!)
            // Setting DesiredVelocity to zero ensures units stop if no target is calculated
            Steering.DesiredVelocity = FVector::ZeroVector;

            // --- Simplified Example Logic (Adapt your existing path logic) ---
          
            // 1. Check if already at the final destination
            if (FVector::DistSquared(CurrentLocation, FinalDestination) <= AcceptanceRadiusSq)
            {
                if (PathFrag.HasValidPath()) PathFrag.ResetPath();
                // Optional: Set UnitState Idle via a signal or deferred command if needed
                // UnitBase->SetUnitState(UnitData::Idle); // DON'T DO THIS DIRECTLY IN PROCESSOR - Use Signals/Deferred Commands

                UE_LOG(LogTemp, Warning, TEXT("!!!!!Stop here, DesiredVelocity remains ZeroVector!!!!! %s"),  *Steering.DesiredVelocity.ToString());
                continue; // Stop here, DesiredVelocity remains ZeroVector
            }

            // 2. Manage Path (Use your existing logic, slightly adapted)
            bool bNeedsNewPath = !PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination;
            if (bNeedsNewPath /* && distance > threshold etc. */)
            {
                 PathFrag.ResetPath();
                 // --- Your FindPathSync Logic Here ---
                 FPathFindingQuery Query( nullptr, *NavData, CurrentLocation, FinalDestination, QueryFilter);
                
                 FPathFindingResult PathResult = NavSystem->FindPathSync(Query);

                 if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                 {
                     PathFrag.CurrentPath = PathResult.Path;
                     PathFrag.CurrentPathPointIndex = 1;
                     PathFrag.PathTargetLocation = FinalDestination;
                 }
                 // --- End Pathfinding ---
            }

            // 3. Determine Current Move Target Point (Waypoint or Final Destination)
            FVector CurrentTargetPoint = FinalDestination; // Default to final goal
            bool bFollowingPath = false;
            if (PathFrag.HasValidPath())
            {
                 const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                 if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                 {
                     bFollowingPath = true;
                     CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;

                     // Use your waypoint acceptance logic
                     const float WaypointAcceptanceRadiusSq = FMath::Square(PathWaypointAcceptanceRadius);
                     if (FVector::DistSquared(CurrentLocation, CurrentTargetPoint) <= WaypointAcceptanceRadiusSq)
                     {
                         PathFrag.CurrentPathPointIndex++;
                         if (!PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                         {
                             PathFrag.ResetPath(); // Reached end of path points
                             bFollowingPath = false;
                             CurrentTargetPoint = FinalDestination;
                         }
                         else
                         {
                             CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;
                         }
                     }
                 }
                 else { PathFrag.ResetPath(); } // Invalid index
            }

            // 4. Calculate Desired Velocity towards the Current Target Point
            FVector MoveDir = CurrentTargetPoint - CurrentLocation;
            if (!MoveDir.IsNearlyZero(0.1f)) // Use a small tolerance
            {
                MoveDir.Normalize();
                // ******** KEY CHANGE ********
                // Store the result in the Steering Fragment, don't apply it to the transform!
                Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                UE_LOG(LogTemp, Warning, TEXT("!!!!!SET  Steering.DesiredVelocity!!!!! %s"),  *Steering.DesiredVelocity.ToString());
                // ***************************
            }

            UE_LOG(LogTemp, Warning, TEXT("!!!!!New  Steering.DesiredVelocity!!!!! %s"),  *Steering.DesiredVelocity.ToString());
            // else: We are very close to the current target point or final destination (if no path)
            // Steering.DesiredVelocity remains ZeroVector (from reset earlier) or gets set to zero by MoveDir being near zero.

            // --- REMOVED: Direct Transform Update ---
            // Transform.AddToTranslation(Velocity * DeltaTime); // <-- DELETE THIS

        } // End loop through entities
    }); // End ForEachEntityChunk
}