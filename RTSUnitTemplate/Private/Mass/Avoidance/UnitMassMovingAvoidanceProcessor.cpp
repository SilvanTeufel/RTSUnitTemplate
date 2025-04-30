// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Avoidance/UnitMassMovingAvoidanceProcessor.h"

// Fill out your copyright notice in the Description page of Project Settings.
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Avoidance/MassAvoidanceFragments.h" // For FMassForceFragment if needed directly
#include "MassExecutionContext.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h" // Added for GetMutableEntityManager
#include "MassLODFragments.h"
#include "Steering/MassSteeringFragments.h" // Added for steering fragment

UUnitMassMovingAvoidanceProcessor::UUnitMassMovingAvoidanceProcessor()
{
	// Ensure this runs in the correct group and order, matching the engine default if possible
	// Or ensuring it runs AFTER your UnitMovementProcessor (Tasks group)
	// and BEFORE your UnitApplyMassMovementProcessor (Movement group).
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks); // Make sure it's after intent calculation
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UUnitMassMovingAvoidanceProcessor::ConfigureQueries()
{
	// IMPORTANT: Call the base class ConfigureQueries first!
	// This sets up the base class's private EntityQuery.
	Super::ConfigureQueries();

	// Now configure OUR DebugLogQuery for logging needs.
	// Add fragments you want to LOG (ReadOnly is sufficient).
	DebugLogQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // For location if needed

	// Add any tags needed to ensure we query the same entities as the base processor
	DebugLogQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	DebugLogQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	DebugLogQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	// Add your specific unit tags if the base query uses them implicitly or explicitly
	// DebugLogQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);

	// Register the debug query
	DebugLogQuery.RegisterWithProcessor(*this);
}

/*
// Optional: Override ConfigureQueries if needed, otherwise base class version is used.
void UUnitMassMovingAvoidanceProcessor::ConfigureQueries()
{
	Super::ConfigureQueries(); // Call base class query setup
	// Add any additional requirements specific to your debugging needs if necessary
	// EntityQuery.AddRequirement...
}
*/

void UUnitMassMovingAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// --- Logging BEFORE Super::Execute using DebugLogQuery ---
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context, [&Context](FMassExecutionContext& ChunkContext) {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		// Get ReadOnly views based on DebugLogQuery requirements
		const TConstArrayView<FMassForceFragment> ForceList = ChunkContext.GetFragmentView<FMassForceFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetFragmentView<FMassSteeringFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			//#if WITH_MASSGAMEPLAY_DEBUG
			//if (UE::Mass::Debug::IsDebuggingEntity(Entity))
			//{
				const FVector CurrentForce = ForceList[i].Value;
				const FVector CurrentVel = VelocityList[i].Value;
				const FVector CurrentDesiredVel = SteeringList[i].DesiredVelocity;

				UE_LOG(LogTemp, Log, TEXT("Entity [%d] PRE-MOVING-AVOIDANCE: Force=%s | Vel=%s | DesiredVel=%s"),
					Entity.Index, *CurrentForce.ToString(), *CurrentVel.ToString(), *CurrentDesiredVel.ToString());
			//}
			//#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});

	// --- Execute the original engine avoidance logic ---
	Super::Execute(EntityManager, Context);
	// ---------------------------------------------------

	// --- Logging AFTER Super::Execute using DebugLogQuery ---
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context, [&Context](FMassExecutionContext& ChunkContext) {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassForceFragment> ForceList = ChunkContext.GetFragmentView<FMassForceFragment>(); // Get force after execution

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			//#if WITH_MASSGAMEPLAY_DEBUG
			//if (UE::Mass::Debug::IsDebuggingEntity(Entity))
			//{
				const FVector ResultForce = ForceList[i].Value;
				UE_LOG(LogTemp, Log, TEXT("Entity [%d] POST-MOVING-AVOIDANCE: ResultForce=%s"),
					Entity.Index, *ResultForce.ToString());

				if(!ResultForce.IsNearlyZero(0.1f))
				{
					UE_LOG(LogTemp, Warning, TEXT("Entity [%d] POST-MOVING-AVOIDANCE: *** Non-Zero Force Detected: %s ***"),
						Entity.Index, *ResultForce.ToString());
				}
			//}
			//#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});
}