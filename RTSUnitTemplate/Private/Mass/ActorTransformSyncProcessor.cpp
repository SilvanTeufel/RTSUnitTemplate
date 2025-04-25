// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/ActorTransformSyncProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/Actor.h"

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Editor);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    // Optional ExecutionOrder settings...
}
/*
void UActorTransformSyncProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    // You don't necessarily need the RepresentationSubsystem pointer directly
    // if you use FMassActorFragment for the actor link.
}
*/
void UActorTransformSyncProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    // Still need LOD info to know if the actor should be visible/updated
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    //EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // <-- HINZUGEFÜGT: Geschwindigkeit lesen für Rotation
    EntityQuery.RegisterWithProcessor(*this); // Important!
    // Shared fragment defining LOD distances, etc. Might not be strictly needed
    // in Execute if just checking LOD level, but good practice to include.
    //EntityQuery.AddConstSharedRequirement<FMassRepresentationLODParams>(EMassFragmentPresence::All);

    // Optional but recommended: Add a tag requirement if you only want to sync
    // entities specifically marked as having an Actor representation.
    // The MassActor subsystem usually adds this tag automatically.
     //EntityQuery.AddTagRequirement<FMassHasActorRepresentationTag>(EMassFragmentPresence::All);

    // Process entities that are visible (LOD > Off)
    //EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All); // Ensure LOD fragment exists
}

// --- Modified Execute Function ---
void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Ensure thresholds are logical
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f); // Ensure High > Low
    MinTickInterval = FMath::Max(0.001f, MinTickInterval); // Ensure MinTickInterval is small but positive
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval); // Ensure Max >= Min

    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();

    // Avoid division by zero or extreme values if ActualDeltaTime is somehow zero or negative
    if (ActualDeltaTime <= 0.0f)
    {
        // Cannot calculate interval based on invalid delta time, skip or use default?
        // Skipping might be safer if delta time is invalid.
        // Alternatively, use a default interval:
        // AccumulatedTimeA += SOME_SMALL_DEFAULT_DT; // Avoid infinite loop if return is hit constantly
        // if (AccumulatedTimeA < DefaultTickInterval) return;
        // AccumulatedTimeA = 0;
        // For now, just log and potentially return to avoid bad calculations.
        //UE_LOG(LogTemp, Warning, TEXT("UActorTransformSyncProcessor: Invalid ActualDeltaTime (%.4f), skipping update."), ActualDeltaTime);
        return;
    }

    // --- Dynamic Throttling Calculation using ActualDeltaTime ---

    // Convert FPS thresholds to corresponding DeltaTime thresholds
    // High FPS corresponds to Low DeltaTime
    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    // Low FPS corresponds to High DeltaTime
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;

    // Map the ActualDeltaTime to the desired tick interval range.
    // High DeltaTime (Low FPS) should map to MaxTickInterval
    // Low DeltaTime (High FPS) should map to MinTickInterval
    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold); // Input: Delta time values
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);          // Output: Corresponding interval

    // Calculate the interval, clamping ActualDeltaTime to the derived thresholds
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, ActualDeltaTime);

    // Optional: Log the dynamic interval for debugging
    // UE_LOG(LogTemp, Verbose, TEXT("ActualDeltaTime: %.4f, Dynamic Tick Interval: %.4f"), ActualDeltaTime, CurrentDynamicTickInterval);

    //UE_LOG(LogTemp, Warning, TEXT("CurrentDynamicTickInterval: (%.4f)"), CurrentDynamicTickInterval);
    // --- Primary Throttling Check ---
    AccumulatedTimeA += ActualDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval) // Use the dynamically calculated interval
    {
        return; // Skip execution for this frame
    }
    AccumulatedTimeA = 0.0f; // Reset accumulator for the next interval


    // --- Visibility Throttling Accumulator ---
    AccumulatedTimeB += ActualDeltaTime;
    bool bResetVisibilityTimer = false; // Flag to reset timer AFTER the loop


    // --- Main Processing Logic ---
    //UE_LOG(LogTemp, Warning, TEXT("!!!!??????UActorTransformSyncProcessor Executing (Interval: %.4f)!!!!!"), CurrentDynamicTickInterval);
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, ActualDeltaTime, &bResetVisibilityTimer](FMassExecutionContext& ChunkContext) // Capture necessary variables
    {
        // ... (Rest of your ForEachEntityChunk logic remains exactly the same as before) ...
        // ... including visibility check, transform calculation, Slerp, SetActorTransform etc. ...

        const int32 NumEntities = ChunkContext.GetNumEntities();
        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const float MinMovementDistanceSq = FMath::Square(MinMovementDistanceForRotation);

        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            bool UnitIsVisible = UnitBase->IsOnViewport && (!UnitBase->EnableFog || UnitBase->IsVisibleEnemy || UnitBase->IsMyTeam);
            if (!UnitIsVisible && AccumulatedTimeB <= 0.5f)
            {
                continue;
            }
            if (UnitIsVisible || AccumulatedTimeB > 0.5f)
            {
                 bResetVisibilityTimer = true;
            }

            const FTransform& MassTransform = TransformFragments[i].GetTransform();
            const FVector CurrentActorLocation = Actor->GetActorLocation();
            const FQuat CurrentActorRotation = Actor->GetActorQuat();
            FVector FinalLocation = MassTransform.GetLocation();
            FinalLocation.Z = CurrentActorLocation.Z;
            FQuat TargetRotation = CurrentActorRotation;
            const FVector MoveDirection = FinalLocation - CurrentActorLocation;
            if (MoveDirection.SizeSquared() > MinMovementDistanceSq)
            {
                FVector MoveDirection2D = MoveDirection;
                MoveDirection2D.Z = 0.0f;
                if (MoveDirection2D.Normalize())
                {
                    TargetRotation = MoveDirection2D.ToOrientationQuat();
                }
            }
            // Simplified Slerp alpha based on speed and delta time (adjust RotationInterpolationSpeed as needed)
            const float RotationInterpolationSpeed = 5.0f; // Example speed factor
            const FQuat SmoothedRotation = FQuat::Slerp(CurrentActorRotation, TargetRotation, FMath::Clamp(ActualDeltaTime * RotationInterpolationSpeed, 0.0f, 1.0f));
            FTransform FinalActorTransform(SmoothedRotation, FinalLocation, MassTransform.GetScale3D());
            if (!Actor->GetActorTransform().Equals(FinalActorTransform, 0.1f))
            {
                Actor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
            }
        }

    }); // End ForEachEntityChunk

    // --- Reset Visibility Timer ---
    if (bResetVisibilityTimer)
    {
        AccumulatedTimeB = 0.0f;
    }
}
/*
void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();

    // --- Temporäres Throttling ---
    AccumulatedTimeA += ActualDeltaTime;
    AccumulatedTimeB += ActualDeltaTime;

    if (constexpr float TickInterval = 0.05f; AccumulatedTimeA < TickInterval)
    {
        return;
    }
    AccumulatedTimeA = 0.0f;
    // --- Ende Throttling ---

    //UE_LOG(LogTemp, Warning, TEXT("!!!!??????UActorTransformSyncProcessor!!!!!"));
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // WICHTIG: ActualDeltaTime für die Interpolation einfangen
        [this, ActualDeltaTime](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();

        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        const float MinMovementDistanceSq = FMath::Square(MinMovementDistanceForRotation); // Quadrat für Vergleich

        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);

            if (!IsValid(UnitBase)) continue; // Gültigkeitsprüfung

            // Sichtbarkeitsprüfung (wie vorher)
            bool UnitIsVisible = UnitBase->IsOnViewport && (!UnitBase->EnableFog || UnitBase->IsVisibleEnemy || UnitBase->IsMyTeam);
            if (!UnitIsVisible && AccumulatedTimeB <= 0.5f) continue;
            if(UnitIsVisible || AccumulatedTimeB > 0.5f) AccumulatedTimeB = 0.0f; // Reset für Sichtbarkeitstimer

            // --- Transform Synchronisation ---
            const FTransform& MassTransform = TransformFragments[i].GetTransform();
            const FVector CurrentActorLocation = Actor->GetActorLocation(); // Aktuelle (Alte) Actor-Position
            const FQuat CurrentActorRotation = Actor->GetActorQuat();   // Aktuelle (Alte) Actor-Rotation

            // --- Neue Zielposition (Z-angepasst) ---
            FVector FinalLocation = MassTransform.GetLocation();
            FinalLocation.Z = CurrentActorLocation.Z; // Höhe des Actors beibehalten
            //UE_LOG(LogTemp, Warning, TEXT("FinalLocation! %s"), *FinalLocation.ToString());
            // --- Ziel-Rotation basierend auf Positionsänderung berechnen ---
            FQuat TargetRotation = CurrentActorRotation; // Standard: Aktuelle Rotation beibehalten
            const FVector MoveDirection = FinalLocation - CurrentActorLocation; // Richtung von Alt nach Neu

            // Nur rotieren, wenn eine signifikante Bewegung stattgefunden hat
            if (MoveDirection.SizeSquared() > MinMovementDistanceSq) // Prüfe gegen Mindestdistanz
            {
                FVector MoveDirection2D = MoveDirection;
                MoveDirection2D.Z = 0.0f; // Nur horizontale Bewegung für Rotation berücksichtigen

                if (MoveDirection2D.Normalize()) // Sicherstellen, dass Vektor nicht Null ist nach Z-Entfernung
                {
                    TargetRotation = MoveDirection2D.ToOrientationQuat();
                }
                // Wenn MoveDirection2D Null war (nur vertikale Bewegung), bleibt TargetRotation unverändert (CurrentActorRotation)
            }

            // --- Rotation interpolieren ---
            const FQuat SmoothedRotation = FQuat::Slerp(CurrentActorRotation, TargetRotation, ActualDeltaTime * (ActorRotationSpeed / 90.f)); // Rotationsgeschwindigkeit ggf. anpassen

            // --- Finale Actor Transform zusammenbauen ---
            FTransform FinalActorTransform;
            FinalActorTransform.SetLocation(FinalLocation);
            FinalActorTransform.SetRotation(SmoothedRotation);
            FinalActorTransform.SetScale3D(MassTransform.GetScale3D()); // Skalierung von Mass übernehmen

            // --- Actor Transform setzen (nur wenn nötig) ---
            if (!Actor->GetActorTransform().Equals(FinalActorTransform, 0.1f))
            {
                Actor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
            }
        } // Ende for each entity
    }); // Ende ForEachEntityChunk
}*/