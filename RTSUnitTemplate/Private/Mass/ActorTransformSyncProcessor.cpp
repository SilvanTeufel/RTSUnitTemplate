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
        const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODFragments = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();
        // const TConstArrayView<FMassVelocityFragment> VelocityFragments = ChunkContext.GetFragmentView<FMassVelocityFragment>(); // <-- Entfernt
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
            // Wenn keine signifikante Bewegung stattfand, bleibt TargetRotation ebenfalls unverändert

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
                //UE_LOG(LogTemp, Warning, TEXT("END FinalLocation! %s"), *FinalLocation.ToString());
                //Actor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::None);
                Actor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
                //Actor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
            }
        } // Ende for each entity
    }); // Ende ForEachEntityChunk
}