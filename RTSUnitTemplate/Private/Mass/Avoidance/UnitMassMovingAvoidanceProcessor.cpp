// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Avoidance/UnitMassMovingAvoidanceProcessor.h"

// Fill out your copyright notice in the Description page of Project Settings.
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Avoidance/MassAvoidanceFragments.h" // For FMassForceFragment if needed directly
#include "MassExecutionContext.h"
#include "MassDebugger.h"
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
	bAutoRegisterWithProcessingPhases = false;
}

void UUnitMassMovingAvoidanceProcessor::ConfigureQueries()
{
	// IMPORTANT: Call the base class ConfigureQueries first!
	Super::ConfigureQueries();

	// Now configure OUR DebugLogQuery for logging needs.
	DebugLogQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
	DebugLogQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // For location
	// Add shared params needed for logging neighbor check
	DebugLogQuery.AddConstSharedRequirement<FMassMovingAvoidanceParameters>(EMassFragmentPresence::All);

	// Add tags needed to ensure we query the same entities as the base processor
	DebugLogQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	DebugLogQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	DebugLogQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	// DebugLogQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All); // Add if needed

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
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture 'this' is not strictly needed anymore as we get NavSys from Context
        // Capture EntityManager by reference if needed inside lambda
        [&EntityManager, &Context](FMassExecutionContext& ChunkContext)
    {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		if (NumEntities == 0) return;

		// Get ReadOnly views based on DebugLogQuery requirements
		const TConstArrayView<FMassForceFragment> ForceList = ChunkContext.GetFragmentView<FMassForceFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetFragmentView<FMassSteeringFragment>();
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const FMassMovingAvoidanceParameters& MovingParams = ChunkContext.GetConstSharedFragment<FMassMovingAvoidanceParameters>();

        // Get Navigation Subsystem here using the context
        UWorld* World = ChunkContext.GetWorld();
        UMassNavigationSubsystem* NavSys = World ? World->GetSubsystem<UMassNavigationSubsystem>() : nullptr;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			// #if WITH_MASSGAMEPLAY_DEBUG // Keep commented for testing
			// if (UE::Mass::Debug::IsDebuggingEntity(Entity)) // Keep commented for testing
			{
				const FVector CurrentForce = ForceList[i].Value;
				const FVector CurrentVel = VelocityList[i].Value;
				const FVector CurrentDesiredVel = SteeringList[i].DesiredVelocity;
                const FVector AgentLocation = TransformList[i].GetTransform().GetLocation();

				UE_LOG(LogTemp, Log, TEXT("Entity [%d] PRE-MOVING-AVOIDANCE: Force=%s | Vel=%s | DesiredVel=%s"),
					Entity.Index, *CurrentForce.ToString(), *CurrentVel.ToString(), *CurrentDesiredVel.ToString());

                // --- Log Neighbor Check ---
                if (NavSys) // Check if NavSys is valid before using it
                {
                    // *** FIX: Use default allocator (TArray) instead of TFixedAllocator ***
                    TArray<FMassNavigationObstacleItem> TempCloseEntities;
                    const FNavigationObstacleHashGrid2D& ObstacleGrid = NavSys->GetObstacleGrid();

                    const float SearchRadius = MovingParams.ObstacleDetectionDistance;
                    const FVector Extent(SearchRadius, SearchRadius, 0.f);
                    const FBox QueryBox = FBox(AgentLocation - Extent, AgentLocation + Extent);
                    ObstacleGrid.Query(QueryBox, TempCloseEntities); // Query the grid

                    int32 NeighborCount = 0;
                    for(const FMassNavigationObstacleItem& Item : TempCloseEntities)
                    {
                        // Skip self when counting neighbors
                        if(Item.Entity != Entity)
                        {
                            NeighborCount++;
                            // Optional: Log details of each neighbor found
                            // UE_LOG(LogUnitAvoidanceDebug, Log, TEXT("   -> Found Neighbor: Entity [%d]"), Item.Entity.Index);
                        }
                    }

                    UE_LOG(LogTemp, Log, TEXT("Entity [%d] PRE-AVOIDANCE Neighbor Check: Found %d potential neighbors within %.1f units."),
                       Entity.Index, NeighborCount, SearchRadius);
                }
                else
                {
                     UE_LOG(LogTemp, Warning, TEXT("Entity [%d] PRE-AVOIDANCE Neighbor Check: NavigationSubsystem is NULL."), Entity.Index);
                }
                // --- End Log Neighbor Check ---
			}
			// #endif // WITH_MASSGAMEPLAY_DEBUG // Keep commented for testing
		}
	});

	// --- Execute the original engine avoidance logic ---
	// This will use its own internal (private) NavigationSubsystem pointer
	Super::Execute(EntityManager, Context);
	// ---------------------------------------------------

	// --- Logging AFTER Super::Execute using DebugLogQuery ---
	// Re-query just the force fragment to see the result.
	DebugLogQuery.ForEachEntityChunk(EntityManager, Context, [&Context](FMassExecutionContext& ChunkContext) {
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassForceFragment> ForceList = ChunkContext.GetFragmentView<FMassForceFragment>(); // Get force after execution

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			// #if WITH_MASSGAMEPLAY_DEBUG // Keep commented for testing
			// if (UE::Mass::Debug::IsDebuggingEntity(Entity)) // Keep commented for testing
			{
				const FVector ResultForce = ForceList[i].Value;
				UE_LOG(LogTemp, Log, TEXT("Entity [%d] POST-MOVING-AVOIDANCE: ResultForce=%s"),
					Entity.Index, *ResultForce.ToString());

				if(!ResultForce.IsNearlyZero(0.1f))
				{
					UE_LOG(LogTemp, Warning, TEXT("Entity [%d] POST-MOVING-AVOIDANCE: *** Non-Zero Force Detected: %s ***"),
						Entity.Index, *ResultForce.ToString());
				}
			}
			// #endif // WITH_MASSGAMEPLAY_DEBUG // Keep commented for testing
		}
	});
	
}

