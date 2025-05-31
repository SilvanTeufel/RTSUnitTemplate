// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/UnitMassStandingAvoidanceProcess.h"
#include "Avoidance/MassAvoidanceFragments.h"      // For FMassGhostLocationFragment etc.
#include "MassExecutionContext.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassCommonFragments.h" // Include for FTransformFragment if logging location
#include "MassMovementFragments.h" // Include for FMassMoveTargetFragment if logging target info
#include "MassSimulationLOD.h"     // Include for LOD Tags

// Include the header for your custom moving avoidance processor if needed for ExecuteAfter
#include "MassNavigationFragments.h"
#include "Mass/Avoidance/UnitMassMovingAvoidanceProcessor.h"

UUnitMassStandingAvoidanceProcess::UUnitMassStandingAvoidanceProcess()
 : DebugLogQuery(*this) // Initialize the debug query
{
	// Ensure this runs in the correct group and order
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	// Standing usually runs after Moving within the same group
	// Use the engine's moving processor name by default
	ExecutionOrder.ExecuteAfter.Add(UMassMovingAvoidanceProcessor::StaticClass()->GetFName());
	// Add your custom moving processor if you are using that instead, ensuring it also runs after the base one
	// ExecutionOrder.ExecuteAfter.Add(UUnitMassMovingAvoidanceProcessor::StaticClass()->GetFName());

	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = false;
}

void UUnitMassStandingAvoidanceProcess::ConfigureQueries()
{
	// IMPORTANT: Call the base class ConfigureQueries first!
	// This sets up the base class's private EntityQuery.
	Super::ConfigureQueries();

	// Now configure OUR DebugLogQuery for logging needs.
	// Add fragments you want to LOG (ReadOnly is sufficient).
	// Standing avoidance primarily reads/writes FMassGhostLocationFragment
	DebugLogQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // To log actual location
	DebugLogQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly); // To check action state

	DebugLogQuery.AddConstSharedRequirement<FMassStandingAvoidanceParameters>(EMassFragmentPresence::All);

	// Add any tags needed to ensure we query the same entities as the base processor
	DebugLogQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
 	DebugLogQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
 	DebugLogQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	// Add your specific unit tags if the base query uses them implicitly or explicitly
	// DebugLogQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);

	// Register the debug query
	DebugLogQuery.RegisterWithProcessor(*this);
}


void UUnitMassStandingAvoidanceProcess::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// --- Logging BEFORE Super::Execute using DebugLogQuery ---
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context, [&Context](FMassExecutionContext& ChunkContext) {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassGhostLocationFragment> GhostList = ChunkContext.GetFragmentView<FMassGhostLocationFragment>();
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();


		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

			// Only log if the entity should actually be processed by standing avoidance logic
			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Stand && GhostList[i].IsValid(MoveTarget.GetCurrentActionID()))
            {
				//#if WITH_MASSGAMEPLAY_DEBUG
				//if (UE::Mass::Debug::IsDebuggingEntity(Entity))
				//{
					const FVector CurrentGhostLocation = GhostList[i].Location;
					const FVector CurrentGhostVelocity = GhostList[i].Velocity;
					const FVector CurrentActualLocation = TransformList[i].GetTransform().GetLocation();

					UE_LOG(LogTemp, Log, TEXT("Entity [%d] PRE-STANDING: GhostLoc=%s | GhostVel=%s | ActualLoc=%s"),
						Entity.Index, *CurrentGhostLocation.ToString(), *CurrentGhostVelocity.ToString(), *CurrentActualLocation.ToString());
				//}
				//#endif // WITH_MASSGAMEPLAY_DEBUG
			}
		}
	});


	// --- Execute the original engine avoidance logic ---
	Super::Execute(EntityManager, Context);
	// ---------------------------------------------------


	// --- Logging AFTER Super::Execute using DebugLogQuery ---
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context, [&Context](FMassExecutionContext& ChunkContext) {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassGhostLocationFragment> GhostList = ChunkContext.GetFragmentView<FMassGhostLocationFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

			// Only log if the entity should actually be processed by standing avoidance logic
			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Stand && GhostList[i].IsValid(MoveTarget.GetCurrentActionID()))
            {
					const FVector ResultGhostLocation = GhostList[i].Location;
					const FVector ResultGhostVelocity = GhostList[i].Velocity;
					UE_LOG(LogTemp, Log, TEXT("Entity [%d] POST-STANDING: ResultGhostLoc=%s | ResultGhostVel=%s"),
						Entity.Index, *ResultGhostLocation.ToString(), *ResultGhostVelocity.ToString());
			}
		}
	});
}
