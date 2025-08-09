// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitApplyMassMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

UUnitApplyMassMovementProcessor::UUnitApplyMassMovementProcessor(): EntityQuery()
{

	//ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	//ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UUnitApplyMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	
	// Input from the steering/pathfinding processor
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly); // <<< ADDED (ReadOnly)
	//EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
	EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
	
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any); 
	//EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::Any);
	
	EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
	// Tag requirements
	//EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None); // <<< Only Moves on Screen
	EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
	//EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
	//EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);   // ...OR if this tag is present.
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	// Shared fragment requirement
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All); // <<< ADDED BACK
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitApplyMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());
    if (DeltaTime <= 0.0f) // Avoid division by zero or no-op
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& Context)
    {
        const int32 NumEntities = Context.GetNumEntities();
        if (NumEntities == 0) return;
        
        // --- Get required data ---
        const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = Context.GetFragmentView<FMassSteeringFragment>(); // Get Steering
        const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex]; 
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
            // --- NEU: Vertikale Geschwindigkeit sichern und alle Berechnungen in 2D durchführen ---

            // 1. Sichere die aktuelle Z-Geschwindigkeit. Diese wollen wir unberührt lassen.
            const float OriginalZVelocity = Velocity.Value.Z;

            // --- Core Movement Logic (modifiziert für 2D) ---
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const FVector AvoidanceForce = Force.Value; 
            const float MaxSpeed = MovementParams.MaxSpeed; 
            const float Acceleration = MovementParams.MaxAcceleration;

            // 2. Erstelle 2D-Versionen der Vektoren für eine saubere horizontale Berechnung.
            const FVector CurrentHorizontalVelocity = FVector(Velocity.Value.X, Velocity.Value.Y, 0.f);
            const FVector DesiredHorizontalVelocity = FVector(DesiredVelocity.X, DesiredVelocity.Y, 0.f);
            const FVector HorizontalAvoidanceForce = FVector(AvoidanceForce.X, AvoidanceForce.Y, 0.f);

            // 3. Führe die Beschleunigungs- und Kraftberechnung ausschließlich auf der X/Y-Ebene durch.
            FVector AccelInput = (DesiredHorizontalVelocity - CurrentHorizontalVelocity);
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);
            
            FVector HorizontalVelocityDelta = (AccelInput + HorizontalAvoidanceForce) * DeltaTime * 4.f;

            // Update der horizontalen Geschwindigkeit
            FVector NewHorizontalVelocity = CurrentHorizontalVelocity + HorizontalVelocityDelta;

            // Begrenze die horizontale Geschwindigkeit
            NewHorizontalVelocity = NewHorizontalVelocity.GetClampedToMaxSize(MaxSpeed);
            
            // --- NEU: FINALE GESCHWINDIGKEIT ZUSAMMENSETZEN ---
            
            // 4. Kombiniere die neue horizontale Geschwindigkeit mit der ursprünglichen vertikalen Geschwindigkeit.
            Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);

            // --- Apply final velocity to position (unverändert) ---
            FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;
            CurrentTransform.SetTranslation(NewLocation);

            // Reset force for the next frame
            Force.Value = FVector::ZeroVector;
        }
    });
}