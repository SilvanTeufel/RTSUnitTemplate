// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Mass/UnitMovementProcessor.h"


#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "Characters/Mass/UnitMassTag.h"

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
	EntityQuery.RegisterWithProcessor(*this); // Important!
}
/*
void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Optional Log: Useful for debugging if Execute runs
    UE_LOG(LogTemp, Log, TEXT("Executing UnitMovementProcessor"));

    EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        // Get fragment views for this chunk
        auto TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        // Use the ENGINE's FMassMoveTargetFragment (ReadOnly access as configured in ConfigureQueries)
        auto TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Get references to the fragments for the current entity
            FTransform& Transform = TransformList[i].GetMutableTransform();
            FMassVelocityFragment& VelocityFrag = VelocityList[i];
            const FMassMoveTargetFragment& MoveTarget = TargetList[i]; // Engine standard fragment

            // Default velocity is zero (stopped)
            FVector DesiredVelocity = FVector::ZeroVector;

            // --- Movement Logic ---
            // Check if the entity's intent is to move and it has a target speed > 0
            // Note: DesiredSpeed is often FMassFloatValue, use .Get()
            if (MoveTarget.DesiredSpeed.Get() > KINDA_SMALL_NUMBER)
            {
                const FVector CurrentLocation = Transform.GetLocation();
                // Use MoveTarget.Center as the target location point
                const FVector TargetCenter = MoveTarget.Center;

                // Calculate direction and distance to the target
                FVector DirectionToTarget = TargetCenter - CurrentLocation;

                // Use MoveTarget.SlackRadius for the acceptance/stopping distance
                // Compare squared distances for efficiency (avoids sqrt)
                const float AcceptanceRadiusSq = FMath::Square(MoveTarget.SlackRadius);

                // Only calculate movement if we are further than the acceptance radius
                if (DirectionToTarget.SizeSquared() > AcceptanceRadiusSq)
                {
                    // Optional: If your movement should be purely horizontal (ignore Z differences)
                    // DirectionToTarget.Z = 0.0f;

                    // Normalize the direction vector safely (handles zero vector case)
                    if (DirectionToTarget.Normalize())
                    {
                         // Calculate velocity using the direction and the desired speed from the fragment
                        DesiredVelocity = DirectionToTarget * MoveTarget.DesiredSpeed.Get();
                    }
                    // else: Direction vector was zero (or very small), already at target XY. DesiredVelocity remains ZeroVector.
                }
                else
                {
                    // We are within the acceptance radius - stop moving.
                    DesiredVelocity = FVector::ZeroVector;

                    // --- Optional: Signal Arrival or Change Intent ---
                    // If you want the entity to stop seeking once it arrives, you would need
                    // Write access to FMassMoveTargetFragment in ConfigureQueries and use
                    // a deferred command here to change the Intent or speed.
                    // Example (requires Write access & more complex handling):
                   
                    //FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
                    //EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(CurrentEntity, [](FMassMoveTargetFragment& TargetFragment)
                    //{
                        //TargetFragment.Intent = EMassMovementAction::Stand; // Change intent to stop
                        //TargetFragment.DesiredSpeed.Set(0.f);
                    //});
                    // You could also signal arrival to other systems:
                    // UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSharedFragment<UMassSignalSubsystem>();
                    // if(SignalSubsystem) { SignalSubsystem->SignalEntity(UE::Mass::Signals::FollowPointPathDone, CurrentEntity); }
                   
                }
            }
            // Else: Intent is not Move, or DesiredSpeed is effectively zero. DesiredVelocity remains ZeroVector.


            // --- Apply Movement ---
            // Update the entity's actual transform based on the calculated velocity for this frame
            Transform.AddToTranslation(DesiredVelocity * DeltaTime);

            // Update the velocity fragment - other systems might read this
            VelocityFrag.Value = DesiredVelocity;
        }
    });
}
*/

void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("UUnitMovementProcessor::Execute!"));
	
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
}