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
    //UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!UnitMovementProcessor Execute!!!!! "));
    // --- TEST ---

    
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

             
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem)
                {
                    UE_LOG(LogTemp, Warning, TEXT("NO SIGNALSUBSYSTEM FOUND!!!!!!"));
                     continue; // Handle missing subsystem
                }
            
                SignalSubsystem->SignalEntityDeferred(ChunkContext,UnitSignals::ReachedDestination, ChunkContext.GetEntity(i));
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