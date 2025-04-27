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

struct FActorTransformUpdatePayload
{
	TWeakObjectPtr<AActor> ActorPtr;
	FTransform NewTransform;

	FActorTransformUpdatePayload(AActor* InActor, const FTransform& InTransform)
		: ActorPtr(InActor), NewTransform(InTransform)
	{}
};

void ULookAtProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    // --- Throttling ---
    AccumulatedTimeA += DeltaTime;
    if (constexpr float TickInterval = 0.1f; AccumulatedTimeA < TickInterval)
    {
       return;
    }
    AccumulatedTimeA = 0.0f;

    // --- List for Game Thread Updates ---
    TArray<FActorTransformUpdatePayload> PendingLookAtUpdates;
    // PendingLookAtUpdates.Reserve(ExpectedUpdates); // Optional optimization

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingLookAtUpdates by reference
        [DeltaTime, &EntityManager, &PendingLookAtUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        // We likely only need read access to fragments within the lambda now
       // const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>(); // ReadOnly access is sufficient

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            if (!TargetFrag.TargetEntity.IsSet())
            {
                continue;
            }

            // --- Determine Target Location ---
            FVector TargetLocation = TargetFrag.LastKnownLocation;
            const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;
            // EntityManager access is generally thread-safe within Mass
            if (EntityManager.IsEntityValid(TargetEntity))
            {
                if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity))
                {
                    TargetLocation = TargetXform->GetTransform().GetLocation();
                }
            }

            // --- Get Actor (ReadOnly pointer) ---
            AActor* Actor = ActorList[i].GetMutable(); // Use Get() for const view
            if (!IsValid(Actor)) // Still need IsValid check
            {
                continue;
            }

            // --- LookAt Calculation ---
            // Reading actor state might have thread-safety implications, but often acceptable.
            const FVector ActorLocation = Actor->GetActorLocation();
            FVector Dir = TargetLocation - ActorLocation;
            Dir.Z = 0.0f; // LookAt in XY plane
            if (!Dir.Normalize())
            {
                continue; // Avoid issues with zero direction
            }
            const FQuat DesiredQuat = Dir.ToOrientationQuat();

            // --- Interpolation ---
            const float RotationSpeedDeg = StatsList[i].RotationSpeed * 15.f; // Consider renaming Stat or clarifying multiplier
            const FQuat CurrentQuat = Actor->GetActorQuat(); // Reading actor state
            FQuat NewQuat;
            if (RotationSpeedDeg > KINDA_SMALL_NUMBER)
            {
                NewQuat = FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, DeltaTime, FMath::DegreesToRadians(RotationSpeedDeg));
            }
            else // Instant rotation if speed is zero/negligible
            {
                NewQuat = DesiredQuat;
            }

            // --- Prepare Final Transform ---
            FTransform FinalTransform = Actor->GetActorTransform(); // Reading actor state
            FinalTransform.SetRotation(NewQuat);

            // --- Check if update is needed and add to list ---
            if (!Actor->GetActorTransform().GetRotation().Equals(NewQuat, 0.01f)) // Compare only rotation if that's all we change
            // Or use: if (!Actor->GetActorTransform().Equals(FinalTransform, 0.01f))
            {
                // Add to the list instead of setting transform directly
                PendingLookAtUpdates.Emplace(Actor, FinalTransform);
            }
        }
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task ---
    if (!PendingLookAtUpdates.IsEmpty())
    {
        // Capture the list of updates by value (moving it)
        AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingLookAtUpdates)]()
        {
            for (const FActorTransformUpdatePayload& Update : Updates)
            {
                // Check if the actor is still valid on the Game Thread
                if (AActor* Actor = Update.ActorPtr.Get())
                {
                    // Apply the transform safely on the Game Thread
                    Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
            }
        });
    }
}

/*
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
}*/