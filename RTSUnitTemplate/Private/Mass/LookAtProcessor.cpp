// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass//LookAtProcessor.h"

#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "Mass/UnitMassTag.h"

ULookAtProcessor::ULookAtProcessor()
{
	// Sollte laufen, nachdem Ziele gesetzt wurden, aber bevor/während Bewegung/Angriff relevant wird
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement; // Oder ::Behavior, je nach Präferenz
	ProcessingPhase = EMassProcessingPhase::PrePhysics; // Rotation vor der Physik berechnen? Oder PostPhysics? Testen!
	bAutoRegisterWithProcessingPhases = true;
	// Normalerweise nicht auf GameThread beschränkt, es sei denn, es gibt spezifische Gründe
	// bRequiresGameThreadExecution = false;
}

void ULookAtProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
	// Schließe tote oder pausierte Einheiten aus (optional, je nach gewünschtem Verhalten)
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	// EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void ULookAtProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// --- Temporäres Throttling ---
	AccumulatedTimeA += DeltaTime;

	if (constexpr float TickInterval = 0.1f; AccumulatedTimeA < TickInterval)
	{
		return;
	}
	AccumulatedTimeA = 0.0f;
	
   


    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [DeltaTime, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            if (!TargetFrag.TargetEntity.IsSet())
            {
                continue;
            }

            // Ermitteln der Zielposition
            FVector TargetLocation = TargetFrag.LastKnownLocation;
            const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;
            if (EntityManager.IsEntityValid(TargetEntity))
            {
                if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity))
                {
                    TargetLocation = TargetXform->GetTransform().GetLocation();
                }
            }

            // Actor holen
            FMassActorFragment& ActorFrag = ActorList[i];
            AActor* Actor = ActorFrag.GetMutable();
            if (!IsValid(Actor))
            {
                continue;
            }

            // LookAt-Berechnung in XY-Ebene
            const FVector ActorLocation = Actor->GetActorLocation();
            FVector Dir = TargetLocation - ActorLocation;
            Dir.Z = 0.0f;
            if (!Dir.Normalize())
            {
                continue;
            }
            const FQuat DesiredQuat = Dir.ToOrientationQuat();

            // Interpolation mit Rotationsgeschwindigkeit
            const float RotationSpeedDeg = StatsList[i].RotationSpeed*15.f;
            const FQuat CurrentQuat = Actor->GetActorQuat();
            FQuat NewQuat;
            if (RotationSpeedDeg > KINDA_SMALL_NUMBER)
            {
                NewQuat = FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, DeltaTime, FMath::DegreesToRadians(RotationSpeedDeg));
            }
            else
            {
                NewQuat = DesiredQuat;
            }

            // Neuen Transform zusammenstellen
            FTransform FinalTransform = Actor->GetActorTransform();
            FinalTransform.SetRotation(NewQuat);

            // SetActorTransform wenn nötig
            if (!Actor->GetActorTransform().Equals(FinalTransform, 0.01f))
            {
                Actor->SetActorTransform(FinalTransform, false, nullptr, ETeleportType::TeleportPhysics);
            }
        }
    });
}