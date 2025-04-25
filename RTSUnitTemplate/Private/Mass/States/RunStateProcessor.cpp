// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

URunStateProcessor::URunStateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void URunStateProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Nur Entities im Run-Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben

	// Optional: Wenn StopMovement direkt Velocity setzt oder Path zurückgesetzt werden soll
	// EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	// EntityQuery.AddRequirement<FMassPathFragment>(EMassFragmentAccess::ReadWrite);

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // UE_LOG(LogTemp, Log, TEXT("URunStateProcessor::Execute"));
	/*
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("URunStateProcessor: World ist null!"));
        return;
    }

    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!SignalSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("URunStateProcessor: SignalSubsystem ist null!"));
        return;
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // World wird für StopMovement benötigt, also auch erfassen
        [this, SignalSubsystem, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        // === KORREKTUR HIER: Mutable View holen ===
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        // ==========================================

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FTransformFragment& TransformFrag = TransformList[i];
            // === Jetzt ist MoveTargetFrag eine gültige nicht-konstante Referenz ===
            FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[i];
            // ===================================================================

            // --- Daten extrahieren ---
            const FVector CurrentLocation = TransformFrag.GetTransform().GetLocation();
            const FVector FinalDestination = MoveTargetFrag.Center;
            const float AcceptanceRadius = MoveTargetFrag.SlackRadius;
            const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);

            // --- Distanz prüfen ---
            const float DistSq = FVector::DistSquared(CurrentLocation, FinalDestination);

            if (DistSq <= AcceptanceRadiusSq)
            {
                // --- Ziel erreicht ---
                UE_LOG(LogTemp, Log, TEXT("Entity %d:%d hat Ziel erreicht. Signalisiere Idle und stoppe Bewegung."), Entity.Index, Entity.SerialNumber);

                SignalSubsystem->SignalEntity(UnitSignals::Idle, Entity);

                // StopMovement kann jetzt mit der nicht-konstanten Referenz aufgerufen werden
                StopMovement(MoveTargetFrag, World); // World wurde oben geholt und in Lambda erfasst

                continue;
            }

        } // Ende for each entity
    }); // Ende ForEachEntityChunk
	*/
}
