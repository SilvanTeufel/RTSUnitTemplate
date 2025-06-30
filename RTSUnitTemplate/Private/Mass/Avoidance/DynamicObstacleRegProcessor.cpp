// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/DynamicObstacleRegProcessor.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationTypes.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

UDynamicObstacleRegProcessor::UDynamicObstacleRegProcessor(): ObstacleQuery()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UDynamicObstacleRegProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObstacleQuery.Initialize(EntityManager);
	ObstacleQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObstacleQuery.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly);
	ObstacleQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);

	//ObstacleQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);  // READ the target location/speed
	//ObstacleQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite); // WRITE desired velocity
	//ObstacleQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite);
	//ObstacleQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);

	// Add a tag requirement if you only want to register specific entities
	// ObstacleQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	ObstacleQuery.RegisterWithProcessor(*this);
}

void UDynamicObstacleRegProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return; 
	}
	TimeSinceLastRun -= ExecutionInterval;
	
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    UMassNavigationSubsystem* NavSys = World->GetSubsystem<UMassNavigationSubsystem>();
    if (!ensure(NavSys != nullptr))
    {
        return;
    }

    static constexpr float InitialCellSize = 100.f;
    
    // Re-initialize (clear + configure) the hash‐grid
    NavSys->GetObstacleGridMutable().Initialize(InitialCellSize);
    
    const FColor BoxColor = FColor::Red;
    constexpr float LifeTime = 3.0f; // Set a longer lifetime for debugging if needed

    // --- NEW: Constants for the subdivision logic ---
    // If an obstacle's radius is larger than this, we'll subdivide it.
    const float LargeRadiusThreshold = InitialCellSize; 
    // The radius of the small obstacles we'll create along the perimeter.
    const float SubObstacleRadius = InitialCellSize / 2.0f; 

    ObstacleQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto& Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto& Colliders = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FVector Location = CharList[i].PositionedTransform.GetLocation();
        	const float Height = CharList[i].CapsuleHeight;
            const float Radius = Colliders[i].GetCircleCollider().Radius;

        	if (CharList[i].bIsFlying) continue;
            // --- MODIFIED LOGIC START ---
			// LOG HEIGHT HERE PLEASE
            if (Radius > LargeRadiusThreshold)
            {
                // This is a large obstacle. Subdivide it into a ring of smaller obstacles.
                const FColor SubBoxColor = FColor::Orange;
                const float SubObstacleDiameter = SubObstacleRadius * 2.0f;

                // Calculate how many smaller obstacles we need to reasonably cover the circumference.
                const int32 NumSubObstacles = FMath::CeilToInt((2.0f * PI * Radius) / SubObstacleDiameter);

                for (int32 j = 0; j < NumSubObstacles; ++j)
                {
                    // Calculate the angle for this sub-obstacle.
                    const float Angle = (static_cast<float>(j) / NumSubObstacles) * 2.0f * PI;

                    // Calculate the position of the sub-obstacle on the circle's edge.
                    // Assuming 2D navigation on the XY plane.
                    const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
                    const FVector SubLocation = Location + Offset;
                    
                    const FVector SubExtent(SubObstacleRadius, SubObstacleRadius, SubObstacleRadius);
                    const FBox SubBounds(SubLocation - SubExtent, SubLocation + SubExtent);

                    // Draw the bounding box for each sub-obstacle
                    if (Debug)DrawDebugBox(World, SubBounds.GetCenter(), SubBounds.GetExtent(), SubBoxColor, false, LifeTime, 0, 2.0f);

                    // We use the same main entity handle for all sub-obstacles.
                    // The navigation system will treat them as distinct obstacles at different locations.
                    FMassNavigationObstacleItem Item;
                    Item.Entity = Entity;
                    Item.ItemFlags = EMassNavigationObstacleFlags::HasColliderData;
                    NavSys->GetObstacleGridMutable().Add(Item, SubBounds);
                }
            }
            else
            {
                // This is a small obstacle. Use the original, single-box method.
                const FVector Extent(Radius, Radius, Radius);
                const FBox Bounds(Location - Extent, Location + Extent);

                // Draw the bounding box for the obstacle
                if (Debug)DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxColor, false, LifeTime, 0, 2.0f);

                FMassNavigationObstacleItem Item;
                Item.Entity = Entity;
                Item.ItemFlags = EMassNavigationObstacleFlags::HasColliderData;
                NavSys->GetObstacleGridMutable().Add(Item, Bounds);
            }
            // --- MODIFIED LOGIC END ---
        }
    });
}
/*
void UDynamicObstacleRegProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	UMassNavigationSubsystem* NavSys = World->GetSubsystem<UMassNavigationSubsystem>();
	if (!ensure(NavSys!= nullptr))
	{
		return;
	}

	static constexpr float InitialCellSize = 100.f;

	// Re-initialize (clear + configure) the hash‐grid
	NavSys->GetObstacleGridMutable().Initialize(InitialCellSize);
	const FColor BoxColor = FColor::Red;
	constexpr float LifeTime = 3.0f; // Draw for one frame

	ObstacleQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const auto& Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto& Colliders = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();
		TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
		// UE_LOG(LogTemp, Warning, TEXT("UDynamicObstacleRegProcessor NumEntities: %d"), NumEntities);
		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			//FVector Location = Transforms[i].GetTransform().GetLocation();
			FVector Location = CharList[i].PositionedTransform.GetLocation();
			float Radius = Colliders[i].GetCircleCollider().Radius;
			FVector Extent(Radius, Radius, Radius);
			FBox Bounds(Location - Extent, Location + Extent);

			// Draw the bounding box for each obstacle
			DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxColor, false, LifeTime, 0, 2.0f);

			
			FMassNavigationObstacleItem Item;
			Item.Entity = Entity;
			Item.ItemFlags = EMassNavigationObstacleFlags::HasColliderData;
			NavSys->GetObstacleGridMutable().Add(Item, Bounds);
		}
	});

	
}
*/