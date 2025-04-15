// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Mass/UnitMovementProcessor.h"


#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "MassNavigationFragments.h"
#include "Characters/Mass/UnitMassTag.h"
#include "NavFilters/NavigationQueryFilter.h"

UUnitMovementProcessor::UUnitMovementProcessor()
{
	// Set the processor to run in the Movement phase.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	bAutoRegisterWithProcessingPhases = true;
}

void UUnitMovementProcessor::ConfigureQueries()
{
	// Add necessary fragments that the processor will need to access
	//EntityQuery.RegisterWithProcessor(*this);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly); // READ the target location
	
	// Optionally, add tags if needed (FMyUnitTag is an example)
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this); // Important! Otherwise Process will not run
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    /*
    AccumulatedTime += Context.GetDeltaTimeSeconds();
    
    if (constexpr float TickInterval = 0.01f; AccumulatedTime < TickInterval) // Nur alle 0.2 Sekunden (also 5x pro Sekunde)
    {
        return; // Zu früh – überspringen
    }

    AccumulatedTime = 0.0f; // Reset
    */
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
            FPathFindingQuery Query(/* QueryUser= */ nullptr, *NavData, CurrentLocation, GoalLocation, QueryFilter);

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



///////////////////////////////////////////////////
/*
void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Get NavSystem instance once (can be cached if needed)
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(EntityManager.GetWorld());
    if (!NavSys)
    {
        UE_LOG(LogTemp, Error, TEXT("UnitMovementProcessor: No NavigationSystemV1 found!"));
        return; // Cannot perform pathfinding without NavSys
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        const auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const auto NavPathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>(); // Get the path fragment view

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        const UWorld* World = EntityManager.GetWorld();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
            FTransform& Transform = TransformList[i].GetMutableTransform();
            FMassVelocityFragment& VelocityFrag = VelocityList[i];
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FUnitNavigationPathFragment& NavPathFrag = NavPathList[i]; // Get mutable ref

            const FVector CurrentLocation = Transform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float SlackRadius = MoveTarget.SlackRadius;
            const EMassMovementAction Intent = MoveTarget.GetCurrentAction();

            FVector CalculatedVelocity = FVector::ZeroVector;
            bool bShouldMove = (Intent == EMassMovementAction::Move && DesiredSpeed > KINDA_SMALL_NUMBER);

            if (!bShouldMove)
            {
                // Not supposed to move, clear any existing path and ensure velocity is zero
                if (NavPathFrag.CurrentPath.IsValid())
                {
                     NavPathFrag.ResetPath();
                     // UE_LOG(LogTemp, Log, TEXT("Entity [%s]: Intent is not Move. Clearing path."), *CurrentEntity.DebugGetDescription());
                }
                VelocityFrag.Value = FVector::ZeroVector;
                continue; // Process next entity
            }

            // --- Path Generation / Validation ---
            // Decide IF we need a new path. Conditions:
            // 1. Intent is Move.
            // 2. No valid path exists OR the final destination has changed since the path was generated.
            // NOTE: This runs FindPathSync *synchronously* here. For many entities, this IS A BOTTLENECK.
            // Consider moving this to a separate processor or using async pathfinding.
            bool bNeedsNewPath = !NavPathFrag.CurrentPath.IsValid() || (NavPathFrag.PathTargetLocation != FinalDestination);

            if (bNeedsNewPath)
            {
                 // --- Perform Pathfinding (Expensive Step) ---
                 // UE_LOG(LogTemp, Log, TEXT("Entity [%s]: Needs new path to %s."), *CurrentEntity.DebugGetDescription(), *FinalDestination.ToString());
                 NavPathFrag.ResetPath(); // Clear old path data first

                 ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
                 if (NavData)
                 {
                     // Simplified query setup - Adapt from your AUnitControllerBase logic as needed
                     FPathFindingQuery Query(this, *NavData, CurrentLocation, FinalDestination);
                     // You might need a valid UObject* context for the query, 'this' (the processor) might work, or you might need the owning world actor.
                     // You might also need to configure the Query Filter (e.g., based on agent properties if you add them)
                     // FSharedConstNavQueryFilter Filter = UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr); // Example generic filter
                     // Query.QueryFilter = Filter;

                     // Use FindPathSync (BLOCKING CALL!)
                     FPathFindingResult PathResult = NavSys->FindPathSync(Query, EPathFindingMode::Regular);

                     if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && !PathResult.Path->IsPartial())
                     {
                         // UE_LOG(LogTemp, Log, TEXT("Entity [%s]: Path found successfully with %d points."), *CurrentEntity.DebugGetDescription(), PathResult.Path->GetPathPoints().Num());
                         NavPathFrag.CurrentPath = PathResult.Path;
                         NavPathFrag.CurrentPathPointIndex = 1; // Start moving towards the first point *after* the start location (index 0 is start)
                         NavPathFrag.PathTargetLocation = FinalDestination; // Store the target this path was generated for
                     }
                     else
                     {
                         // Pathfinding failed or partial path not allowed
                         // UE_LOG(LogTemp, Warning, TEXT("Entity [%s]: Pathfinding failed or partial path. Result: %s"),
                         //      *CurrentEntity.DebugGetDescription(), *UEnum::GetValueAsString(PathResult.Result));
                         NavPathFrag.ResetPath();
                         // TODO: Decide fallback behavior - Stop? Try direct move? Signal failure?
                         // For now, it will likely just stop moving as bFollowingPath will be false.
                     }
                 }
                 else
                 {
                     // UE_LOG(LogTemp, Error, TEXT("Entity [%s]: Could not get NavData for pathfinding."), *CurrentEntity.DebugGetDescription());
                     NavPathFrag.ResetPath(); // Ensure path is invalid
                 }
            }


            // --- Path Following Logic ---
            FVector CurrentTargetPoint = FinalDestination; // Default to final destination (direct move if no path)
            bool bFollowingPath = false;
            const float PathWaypointAcceptanceRadiusSq = FMath::Square(50.0f); // How close to get to intermediate waypoints

            if (NavPathFrag.CurrentPath.IsValid() && NavPathFrag.CurrentPath->GetPathPoints().IsValidIndex(NavPathFrag.CurrentPathPointIndex))
            {
                bFollowingPath = true;
                CurrentTargetPoint = NavPathFrag.CurrentPath->GetPathPoints()[NavPathFrag.CurrentPathPointIndex].Location;

                // Check if we are close enough to the current waypoint
                const float DistToWaypointSq = FVector::DistSquared(CurrentLocation, CurrentTargetPoint);

                if (DistToWaypointSq <= PathWaypointAcceptanceRadiusSq)
                {
                    // Move to the next waypoint
                    NavPathFrag.CurrentPathPointIndex++;
                   // UE_LOG(LogTemp, Log, TEXT("Entity [%s]: Reached waypoint. Moving to index %d."), *CurrentEntity.DebugGetDescription(), NavPathFrag.CurrentPathPointIndex);


                    // Check if we've run out of waypoints (reached the end of the path)
                    if (!NavPathFrag.CurrentPath->GetPathPoints().IsValidIndex(NavPathFrag.CurrentPathPointIndex))
                    {
                         // UE_LOG(LogTemp, Log, TEXT("Entity [%s]: Reached end of path."), *CurrentEntity.DebugGetDescription());
                         NavPathFrag.ResetPath(); // Clear the path, we are done with it.
                         bFollowingPath = false;
                         CurrentTargetPoint = FinalDestination; // Target the final destination precisely now.

                         // Optional: Signal arrival or change state via deferred command
                         // Context.Defer().PushCommand<FMassDeferredSetCommand>(CurrentEntity, [](FMassMoveTargetFragment& TargetFragment) {
                         //     TargetFragment.CreateNewAction(EMassMovementAction::Stand, *TargetFragment.GetWorld()); // Assuming GetWorld is valid
                         //     TargetFragment.DesiredSpeed.Set(0.f);
                         // });
                         // Or: Context.Defer().AddTag<FMassReachedDestinationTag>(CurrentEntity);
                    }
                    else
                    {
                        // Get the location of the *new* current waypoint
                        CurrentTargetPoint = NavPathFrag.CurrentPath->GetPathPoints()[NavPathFrag.CurrentPathPointIndex].Location;
                    }
                }
            } // End if(following path)


            // --- Velocity Calculation (Based on CurrentTargetPoint) ---
            const FVector DirectionToTargetRaw = CurrentTargetPoint - CurrentLocation;
            const float DistToCurrentTargetSq = DirectionToTargetRaw.SizeSquared();

            // Determine the acceptance radius: Use the final slack radius ONLY if not following a path AND close to the *final* destination.
            // Otherwise, we need to keep moving towards the waypoint or the final point if the path just finished.
            const float FinalAcceptanceRadiusSq = FMath::Square(SlackRadius);
            bool bWithinFinalAcceptance = (FVector::DistSquared(CurrentLocation, FinalDestination) <= FinalAcceptanceRadiusSq);

            if (!bWithinFinalAcceptance || bFollowingPath) // Keep moving if following a path OR if outside final acceptance radius
            {
                FVector MoveDirection = DirectionToTargetRaw;
                if (MoveDirection.Normalize()) // Normalize safely
                {
                    CalculatedVelocity = MoveDirection * DesiredSpeed;
                }
                else {
                    // Already at the CurrentTargetPoint (or very close) - if it was a waypoint, logic above should handle advancing.
                    // If it was the final destination, bWithinFinalAcceptance should be true, stopping us.
                    CalculatedVelocity = FVector::ZeroVector;
                }
            }
            else
            {
                 // Within final acceptance radius AND not following a path anymore -> Stop.
                 CalculatedVelocity = FVector::ZeroVector;
                 // If the path wasn't properly cleared upon arrival, ensure it is now.
                 if(NavPathFrag.CurrentPath.IsValid()) NavPathFrag.ResetPath();
            }


            // --- Apply Movement ---
            if (!CalculatedVelocity.IsNearlyZero() && DeltaTime > 0.f)
            {
                FVector TranslationDelta = CalculatedVelocity * DeltaTime;
                Transform.AddToTranslation(TranslationDelta);
            }

            // Update velocity fragment
            if (VelocityFrag.Value != CalculatedVelocity)
            {
                VelocityFrag.Value = CalculatedVelocity;
            }

             // --- Debug Visualization ---
             if (World)
             {
                 // Draw Current Location (Blue Sphere)
                 DrawDebugSphere(World, CurrentLocation, 20.f, 12, FColor::Blue, false, -1.f, 0, 1.f);
                 // Draw Final Destination + Slack Radius (Green Sphere)
                 DrawDebugSphere(World, FinalDestination, SlackRadius, 12, FColor::Green, false, -1.f, 0, 1.f);

                 if (bFollowingPath && NavPathFrag.CurrentPath.IsValid())
                 {
                     // Draw line to current waypoint (Yellow Line)
                     DrawDebugLine(World, CurrentLocation, CurrentTargetPoint, FColor::Yellow, false, -1.f, 0, 2.f);
                     // Draw the current target waypoint (Yellow Sphere)
                     DrawDebugSphere(World, CurrentTargetPoint, 30.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);
                     // Draw the full path (Cyan Lines/Spheres)
                     const auto& PathPoints = NavPathFrag.CurrentPath->GetPathPoints();
                     for (int32 p = 0; p < PathPoints.Num(); ++p)
                     {
                         DrawDebugSphere(World, PathPoints[p].Location, 15.f, 6, FColor::Cyan, false, -1.f, 0, 1.f);
                         if (p > 0)
                         {
                             DrawDebugLine(World, PathPoints[p-1].Location, PathPoints[p].Location, FColor::Cyan, false, -1.f, 0, 1.f);
                         }
                     }
                 } else {
                     // Draw line direct to final destination if not pathing (Red Line)
                     DrawDebugLine(World, CurrentLocation, FinalDestination, FColor::Red, false, -1.f, 0, 1.f);
                 }
             }


        } // End for loop (per entity)
    }); // End ForEachEntityChunk
}
/*
void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//
	
    // Log at the start to confirm execution - Use 'Log' level so it always shows up
    UE_LOG(LogTemp, Log, TEXT("Executing UnitMovementProcessor"));


    EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        // Log chunk processing - Use 'Log' level
        UE_LOG(LogTemp, Log, TEXT("UnitMovementProcessor: Processing chunk with %d entities."), NumEntities);
        if (NumEntities == 0)
        {
            return; // Skip this chunk if no entities match
        }

        // Get fragment views for this chunk
        const auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        const auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>(); // ReadOnly recommended

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        const UWorld* World = EntityManager.GetWorld(); // Get World context for drawing

        // Process each entity in the chunk
        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Get references/data for the current entity
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
            FTransform& Transform = TransformList[i].GetMutableTransform(); // Mutable for updating position
            FMassVelocityFragment& VelocityFrag = VelocityList[i];           // Mutable for updating velocity
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];       // Const reference (ReadOnly)

            // --- Extract Data for Logging and Logic ---
            const FVector CurrentLocation = Transform.GetLocation();
            const FVector TargetCenter = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get(); // Use .Get() for FMassNumeric types
            const float SlackRadius = MoveTarget.SlackRadius;
            // Access CurrentAction via the provided getter based on the header file structure
            const EMassMovementAction Intent = MoveTarget.GetCurrentAction();

            const FVector DirectionToTargetRaw = TargetCenter - CurrentLocation;
            const float DistSq = DirectionToTargetRaw.SizeSquared();
            const float AcceptanceRadiusSq = FMath::Square(SlackRadius);

            // --- Debug Visualization (Only if World is valid) ---
            if (World)
            {
                 // Draw Current Location (e.g., small blue sphere)
                 DrawDebugSphere(World, CurrentLocation, 20.f, 12, FColor::Blue, false, -1.f, 0, 1.f);
                 // Draw Target Location + Acceptance Radius (e.g., green wire sphere)
                 DrawDebugSphere(World, TargetCenter, SlackRadius, 12, FColor::Green, false, -1.f, 0, 1.f);
                 // Draw Line from Current to Target (e.g., red line)
                 DrawDebugLine(World, CurrentLocation, TargetCenter, FColor::Red, false, -1.f, 0, 1.f);
            }

            // --- Movement Logic ---
            FVector CalculatedVelocity = FVector::ZeroVector; // Start with zero velocity

            // Check if the entity's intent is to move AND it has a target speed > 0
            if (Intent == EMassMovementAction::Move && DesiredSpeed > KINDA_SMALL_NUMBER)
            {
                //UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Intent is Move and Speed > 0."), *CurrentEntity.DebugGetDescription());

                // Only calculate movement if we are further than the acceptance radius (squared comparison)
                if (DistSq > AcceptanceRadiusSq)
                {
                    //UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Outside acceptance radius (DistSq %.2f > AcceptSq %.2f). Calculating movement."), *CurrentEntity.DebugGetDescription(), DistSq, AcceptanceRadiusSq);

                    // Normalize the direction vector safely
                    FVector MoveDirection = DirectionToTargetRaw; // Use the raw direction
                    if (MoveDirection.Normalize()) // Modifies MoveDirection in place, returns false if near zero length
                    {
                        // Calculate velocity using the normalized direction and the desired speed
                        CalculatedVelocity = MoveDirection * DesiredSpeed;
    
                    }
                    else
                    {
                        // Direction vector was zero or very small (already at target)
                        CalculatedVelocity = FVector::ZeroVector;
                      //  UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Direction normalization failed (already at target?). Setting Velocity Zero."), *CurrentEntity.DebugGetDescription());
                    }
                }
                else
                {
                    // We are within the acceptance radius - stop moving.
                    CalculatedVelocity = FVector::ZeroVector;
                   // UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Within acceptance radius (DistSq %.2f <= AcceptSq %.2f). Setting Velocity Zero."), *CurrentEntity.DebugGetDescription(), DistSq, AcceptanceRadiusSq);

                    // --- Optional: Add logic here if needed upon arrival (e.g., deferred command to change intent) ---
                    // Example (requires Write access to FMassMoveTargetFragment & careful state checking):
                    // if (MoveTarget.DesiredSpeed.Get() != 0.f) // Avoid spamming commands if already stopped
                    // {
                    //     Context.Defer().PushCommand<FMassDeferredSetCommand>(CurrentEntity, [](FMassMoveTargetFragment& TargetFragment)
                    //     {
                    //         UWorld* TargetWorld = TargetFragment.GetWorld(); // Need World context for CreateNewAction
                    //         if(TargetWorld) TargetFragment.CreateNewAction(EMassMovementAction::Stand, *TargetWorld);
                    //         TargetFragment.DesiredSpeed.Set(0.f);
                    //         // UE_LOG(LogTemp, Log, TEXT("Deferred Command: Entity arrived, setting Intent=Stand, Speed=0.")); // Need entity handle if logging here
                    //     });
                    // }
                }
            }
            else
            {
                // Intent is not Move, or DesiredSpeed is effectively zero. Velocity remains ZeroVector.
                CalculatedVelocity = FVector::ZeroVector; // Ensure it's zero
            }


            // --- Apply Movement ---
            // Only apply translation if velocity is significant and DeltaTime is positive
            if (!CalculatedVelocity.IsNearlyZero() && DeltaTime > 0.f)
            {
                 FVector TranslationDelta = CalculatedVelocity * DeltaTime;
                 Transform.AddToTranslation(TranslationDelta);
               //  UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Applying Translation Delta: %s (Vel: %s)"), *CurrentEntity.DebugGetDescription(), *TranslationDelta.ToString(), *CalculatedVelocity.ToString());
            }
            else
            {
                // UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Calculated Velocity is zero or DeltaTime is zero. No translation applied."), *CurrentEntity.DebugGetDescription());
            }

            // Update the velocity fragment - other systems might read this
            // Only update if the value has actually changed to potentially save some processing
            if (VelocityFrag.Value != CalculatedVelocity)
            {
                 VelocityFrag.Value = CalculatedVelocity;
               //  UE_LOG(LogTemp, Warning, TEXT("  -> Entity [%s]: Updated VelocityFragment to %s."), *CurrentEntity.DebugGetDescription(), *CalculatedVelocity.ToString());
            }

        } // End of for loop (per entity)
    }); // End of ForEachEntityChunk
	
}
*/

/*
void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Log, TEXT("UUnitMovementProcessor::Execute!"));

	
	// Process each chunk of entities that meet the query's requirements.
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& ChunkContext)
	{
		// Number of entities in this chunk.
		const int32 NumEntities = ChunkContext.GetNumEntities();

		// Get the mutable views for the fragments for this chunk.
		// Note: Do not use the ampersand (&) because the GetMutableFragmentView() returns a temporary view.
		auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

		// Iterate over each entity within the chunk.
		for (int32 i = 0; i < NumEntities; ++i)
		{
			// Access the transform for the current entity.
			FTransform& Transform = TransformList[i].GetMutableTransform();

			// Access the velocity fragment for the current entity.
			FMassVelocityFragment& VelocityFrag = VelocityList[i];

			// Define a fixed desired velocity (for example, moving along the X-axis at a speed of 100 units).
			FVector DesiredVelocity(100.f, 0.f, 0.f);

			// Update the entity's transform: add the translation for this frame.
			Transform.AddToTranslation(DesiredVelocity * ChunkContext.GetDeltaTimeSeconds());

			// Update the movement fragment; assign the velocity.
			// Here, we use the "Value" member which is defined in FMassVelocityFragment.
			VelocityFrag.Value = DesiredVelocity;
		}
	});
}*/