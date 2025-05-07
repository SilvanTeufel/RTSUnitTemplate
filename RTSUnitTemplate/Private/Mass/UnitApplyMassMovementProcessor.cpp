// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/UnitApplyMassMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

UUnitApplyMassMovementProcessor::UUnitApplyMassMovementProcessor()
{

	//ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	//ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UUnitApplyMassMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	
	// Input from the steering/pathfinding processor
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly); // <<< ADDED (ReadOnly)

	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
	EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);

	EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
	// Tag requirements
	//EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None); // <<< Only Moves on Screen
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);   // ...OR if this tag is present.
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

    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
    {
        const int32 NumEntities = Context.GetNumEntities();
        if (NumEntities == 0) return;
    	
        // --- Get required data ---
        const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = Context.GetFragmentView<FMassSteeringFragment>(); // Get Steering
        const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();

        // --- Process each entity ---
    	//UE_LOG(LogTemp, Log, TEXT("UUnitApplyMassMovementProcessor::Execute started NumEntities: %d"), NumEntities);
        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex]; // Read desired velocity
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();

            // --- Core Movement Logic ---
            const FVector DesiredVelocity = Steering.DesiredVelocity;
        	const FVector AvoidanceForce = Force.Value; 
            const float MaxSpeed = MovementParams.MaxSpeed; // Get parameters
            const float Acceleration = MovementParams.MaxAcceleration;

            // Calculate acceleration input towards desired velocity
            // (Simplified: directly accelerate towards target velocity, clamped by max acceleration)
            // More complex steering would use forces differently.
            FVector AccelInput = (DesiredVelocity - Velocity.Value); // How much velocity needs to change
            // Clamp the acceleration magnitude
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);

            // Calculate change in velocity applying acceleration and external forces
            FVector VelocityDelta = (AccelInput + AvoidanceForce) * DeltaTime * 4.f;

            // Store previous velocity for logging if needed
            FVector PrevVelocity = Velocity.Value;

            // Update velocity
            Velocity.Value += VelocityDelta;

            // Clamp final speed
            Velocity.Value = Velocity.Value.GetClampedToMaxSize(MaxSpeed);

        	/*
        	// Log inputs and outputs for the selected entity
				 UE_LOG(LogTemp, Log, TEXT("Entity [%d] ApplyMovement: DesiredVel=%s | AvoidForce=%s | AccelInput=%s | VelocityDelta=%s | FinalVel=%s"),
					 Context.GetEntity(EntityIndex).Index,
					 *DesiredVelocity.ToString(),
					 *AvoidanceForce.ToString(), // Log the force coming FROM avoidance
					 *AccelInput.ToString(),
					 *VelocityDelta.ToString(),
					 *Velocity.Value.ToString());

				 // Optional: Log only if AvoidanceForce is significant
				 if (!AvoidanceForce.IsNearlyZero(0.1f))
				 {
					 UE_LOG(LogTemp, Warning, TEXT("Entity [%d] ApplyMovement: SIGNIFICANT AvoidanceForce Detected: %s"),
						 Context.GetEntity(EntityIndex).Index, *AvoidanceForce.ToString());
				 }
        	*/
            // --- Apply final velocity to position ---
            FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;
            CurrentTransform.SetTranslation(NewLocation);

            // Reset force for the next frame (usually done by force generating systems, but good practice here)
            Force.Value = FVector::ZeroVector;
        }
    });
}