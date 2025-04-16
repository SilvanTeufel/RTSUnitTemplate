// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/UnitMovementProcessor.h"


#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
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
/*
 *
 *
 *#include "Mass/UnitMovementProcessor.h" // Passe Pfad an
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "MassNavigationTypes.h" // Für ENavigationQueryResult::Error etc.

// Includes für unsere Fragmente und Tags
#include "Mass/UnitNavigationFragments.h" // Für FMassNavigationPathFragment (Passe Pfad an)
#include "Mass/UnitMassTag.h"         // Für FUnitMassTag (Passe Pfad an)
#include "Mass/UnitAIStateTags.h"     // Für FMassReachedDestinationTag (Passe Pfad an)


UUnitMovementProcessor::UUnitMovementProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
    ProcessingPhase = EMassProcessingPhase::PrePhysics; // Typische Phase für Bewegungsupdates
    bAutoRegisterWithProcessingPhases = true;
}

void UUnitMovementProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::WriteOnly); // Nur Velocity schreiben
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FMassNavigationPathFragment>(EMassFragmentAccess::ReadWrite); // Pfad lesen/schreiben

    // Nur Einheiten bewegen, die auch dein Unit-Tag haben
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
    // Optional: Nur Einheiten bewegen, die nicht z.B. "Dead" oder "Rooted" sind
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRootedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None); // Auch während Casten nicht bewegen?

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Hole NavSystem einmal pro Execute-Aufruf
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(EntityManager.GetWorld());
    if (!NavSystem)
    {
        // Optional: Logge diesen Fehler nur einmal pro Sitzung
        static bool bNavSystemLogged = false;
        if (!bNavSystemLogged)
        {
             UE_LOG(LogTemp, Error, TEXT("UUnitMovementProcessor: NavigationSystemV1 not found!"));
             bNavSystemLogged = true;
        }
        return;
    }
    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(); // Hole Default NavData
    if (!NavData)
    {
         static bool bNavDataLogged = false;
        if (!bNavDataLogged)
        {
             UE_LOG(LogTemp, Error, TEXT("UUnitMovementProcessor: Default NavigationData not found!"));
             bNavDataLogged = true;
        }
        return;
    }

    // Hole den Standard Query Filter einmal
    FSharedConstNavQueryFilter DefaultQueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());


    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        auto PathList = ChunkContext.GetMutableFragmentView<FMassNavigationPathFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        const float PathWaypointAcceptanceRadiusSq = FMath::Square(PathWaypointAcceptanceRadius);

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FTransform& Transform = TransformList[i].GetMutableTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            FMassNavigationPathFragment& PathFrag = PathList[i];

            const FVector CurrentLocation = Transform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius; // Akzeptanzradius am Endziel
             const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);
            const EMassMovementAction CurrentAction = MoveTarget.GetCurrentAction();

            Velocity.Value = FVector::ZeroVector; // Standardmäßig anhalten

            // --- Prüfen, ob Bewegung überhaupt stattfinden soll ---
            if (CurrentAction != EMassMovementAction::Move || DesiredSpeed <= KINDA_SMALL_NUMBER)
            {
                if (PathFrag.HasValidPath()) PathFrag.ResetPath(); // Alten Pfad löschen
                continue; // Nächste Entität
            }

             // --- Prüfen, ob wir schon am Endziel sind ---
             const float DistToFinalDestSq = FVector::DistSquared(CurrentLocation, FinalDestination);
             if (DistToFinalDestSq <= AcceptanceRadiusSq)
             {
                 if (PathFrag.HasValidPath()) PathFrag.ResetPath(); // Alten Pfad löschen
                 // Signal für Ankunft setzen (könnte von State Machine Prozessoren gelesen werden)
                 if (!ChunkContext.DoesEntityHaveTag<FMassReachedDestinationTag>(ChunkContext.GetEntity(i)))
                 {
                      ChunkContext.Defer().AddTag<FMassReachedDestinationTag>(ChunkContext.GetEntity(i));
                 }
                 continue; // Ziel erreicht, nächste Entität
             }
             else
             {
                  // Signal entfernen, falls wir uns wieder vom Ziel wegbewegt haben
                  if (ChunkContext.DoesEntityHaveTag<FMassReachedDestinationTag>(ChunkContext.GetEntity(i)))
                  {
                       ChunkContext.Defer().RemoveTag<FMassReachedDestinationTag>(ChunkContext.GetEntity(i));
                  }
             }


            // --- Pfadfindung / Pfadvalidierung ---
            bool bNeedsNewPath = !PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination;

            if (bNeedsNewPath)
            {
                // UE_LOG(LogTemp, Log, TEXT("Entity %d needs new path to %s"), ChunkContext.GetEntity(i).Index, *FinalDestination.ToString());
                PathFrag.ResetPath(); // Alten Pfad sicherheitshalber löschen

                // Nur Pfad suchen, wenn Ziel weiter entfernt ist als nur ein kleiner Schritt
                if (DistToFinalDestSq > FMath::Square(DesiredSpeed * DeltaTime * 2.0f)) // Verhindert unnötige Pfadsuche bei kleinen Distanzen
                {
                    FPathFindingQuery Query(nullptr, *NavData, CurrentLocation, FinalDestination, DefaultQueryFilter);
                    FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);

                    if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                    {
                        PathFrag.CurrentPath = PathResult.Path;
                        PathFrag.CurrentPathPointIndex = 1; // Index 0 ist Startpunkt, bewege zu Index 1
                        PathFrag.PathTargetLocation = FinalDestination;
                         // UE_LOG(LogTemp, Log, TEXT("Entity %d path found. Points: %d"), ChunkContext.GetEntity(i).Index, PathFrag.CurrentPath->GetPathPoints().Num());
                    }
                    else
                    {
                        // Pfad ungültig oder nicht gefunden, ResetPath wurde schon gemacht
                        // UE_LOG(LogTemp, Warning, TEXT("Entity %d pathfinding failed or path too short. Target: %s. Reason: %s"),
                        //        ChunkContext.GetEntity(i).Index, *FinalDestination.ToString(), *UEnum::GetValueAsString(PathResult.Result));
                        // Fallback: Direkte Bewegung (wird unten behandelt, da kein Pfad existiert)
                    }
                }
                 else {
                      // Ziel sehr nah, keine Pfadsuche nötig, direkte Bewegung
                 }
            }

            // --- Pfadverfolgung / Zielpunkt bestimmen ---
            FVector MoveDirection = FVector::ZeroVector;
            FVector CurrentTargetPoint = FinalDestination; // Standard: direktes Ziel

            if (PathFrag.HasValidPath())
            {
                const auto& PathPoints = PathFrag.CurrentPath->GetPathPoints();
                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;

                    // Prüfen, ob aktueller Wegpunkt erreicht wurde
                    const float DistToWaypointSq = FVector::DistSquared(CurrentLocation, CurrentTargetPoint);
                    if (DistToWaypointSq <= PathWaypointAcceptanceRadiusSq)
                    {
                        PathFrag.CurrentPathPointIndex++; // Zum nächsten Wegpunkt wechseln
                        // UE_LOG(LogTemp, Verbose, TEXT("Entity %d reached waypoint, advancing to index %d"), ChunkContext.GetEntity(i).Index, PathFrag.CurrentPathPointIndex);

                        // Neuen Wegpunkt setzen oder Pfad beenden
                        if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                        {
                            CurrentTargetPoint = PathPoints[PathFrag.CurrentPathPointIndex].Location;
                        }
                        else
                        {
                            // Ende des Pfades erreicht
                            // UE_LOG(LogTemp, Log, TEXT("Entity %d reached end of path."), ChunkContext.GetEntity(i).Index);
                            PathFrag.ResetPath();
                            CurrentTargetPoint = FinalDestination; // Jetzt direkt zum Endziel
                            // Ankunftssignal wird in der nächsten Iteration oben gesetzt, wenn Distanz passt
                        }
                    }
                    MoveDirection = (CurrentTargetPoint - CurrentLocation);
                }
                else
                {
                    // Index war ungültig, Pfad beenden
                     // UE_LOG(LogTemp, Warning, TEXT("Entity %d had invalid path index %d. Resetting path."), ChunkContext.GetEntity(i).Index, PathFrag.CurrentPathPointIndex);
                    PathFrag.ResetPath();
                    CurrentTargetPoint = FinalDestination;
                    MoveDirection = (CurrentTargetPoint - CurrentLocation);
                }
            }
            else
            {
                // Kein Pfad vorhanden -> Direkte Bewegung zum Ziel
                MoveDirection = (CurrentTargetPoint - CurrentLocation);
            }

            // --- Geschwindigkeit berechnen und anwenden ---
            if (MoveDirection.Normalize()) // Normalisiert und prüft auf Länge > 0
            {
                Velocity.Value = MoveDirection * DesiredSpeed;
                Transform.AddToTranslation(Velocity.Value * DeltaTime);

                 // Optional: Rotation anpassen (besser in eigenem Prozessor)
                 // FRotator TargetRotation = MoveDirection.Rotation();
                 // Transform.SetRotation(FQuat::Slerp(Transform.GetRotation(), TargetRotation.Quaternion(), DeltaTime * RotationSpeed));
            }
             else {
                 // Steht schon am CurrentTargetPoint oder sehr nah dran
                 Velocity.Value = FVector::ZeroVector;
             }
        } // Ende der Entity-Schleife
    }); // Ende ForEachEntityChunk
}
 *
 * 
 */