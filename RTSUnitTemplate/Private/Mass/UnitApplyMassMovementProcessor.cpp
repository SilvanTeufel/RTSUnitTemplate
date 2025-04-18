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

	// Tag requirements
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None); // <<< ADDED BACK

	// Shared fragment requirement
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All); // <<< ADDED BACK
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitApplyMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Warning, TEXT("UUnitApplyMassMovementProcessor!!!!!!! EXECUTE!!!!"));
    // Clamp max delta time
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());
    if (DeltaTime <= 0.0f) // Avoid division by zero or no-op
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
    {
        const int32 NumEntities = Context.GetNumEntities();
        if (NumEntities == 0) return;

        UE_LOG(LogTemp, Log, TEXT("UUnitApplyMassMovementProcessor: Processing Chunk with %d entities!"), NumEntities);

        // --- Get required data ---
        const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = Context.GetFragmentView<FMassSteeringFragment>(); // Get Steering
        const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();

        // --- Process each entity ---
        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex]; // Read desired velocity
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();

            // --- Core Movement Logic ---
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const float MaxSpeed = MovementParams.MaxSpeed; // Get parameters
            const float Acceleration = MovementParams.MaxAcceleration;

            // Calculate acceleration input towards desired velocity
            // (Simplified: directly accelerate towards target velocity, clamped by max acceleration)
            // More complex steering would use forces differently.
            FVector AccelInput = (DesiredVelocity - Velocity.Value); // How much velocity needs to change
            // Clamp the acceleration magnitude
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);

            // Calculate change in velocity applying acceleration and external forces
            FVector VelocityDelta = (AccelInput + Force.Value) * DeltaTime;

            // Store previous velocity for logging if needed
            FVector PrevVelocity = Velocity.Value;

            // Update velocity
            Velocity.Value += VelocityDelta;

            // Clamp final speed
            Velocity.Value = Velocity.Value.GetClampedToMaxSize(MaxSpeed);

            // --- Logging ---
            UE_LOG(LogTemp, Log, TEXT("EntityIdx %d | DesiredV: %s | PrevV: %s | Force: %s | AccelIn: %s | DeltaV: %s | NewV: %s | MaxSpd: %.1f | Accel: %.1f"),
                   EntityIndex, *DesiredVelocity.ToString(), *PrevVelocity.ToString(), *Force.Value.ToString(),
                   *AccelInput.ToString(), *VelocityDelta.ToString(), *Velocity.Value.ToString(), MaxSpeed, Acceleration);

            // --- Apply final velocity to position ---
            FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;
            CurrentTransform.SetTranslation(NewLocation);
            UE_LOG(LogTemp, Log, TEXT("EntityIdx %d | PrevLoc: %s | NewLoc: %s"),
                   EntityIndex, *CurrentLocation.ToString(), *NewLocation.ToString());

            // Reset force for the next frame (usually done by force generating systems, but good practice here)
            Force.Value = FVector::ZeroVector;
        }
    });
}