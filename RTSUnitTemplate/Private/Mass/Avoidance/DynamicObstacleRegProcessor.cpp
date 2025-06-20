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
	ObstacleQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);  // READ the target location/speed
	ObstacleQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite); // WRITE desired velocity
	ObstacleQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite);
	ObstacleQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);

	// Add a tag requirement if you only want to register specific entities
	// ObstacleQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	ObstacleQuery.RegisterWithProcessor(*this);
}

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
	//NavSys->GetObstacleGridMutable().Initialize(InitialCellSize);

	ObstacleQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const auto& Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto& Colliders = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();

		// UE_LOG(LogTemp, Warning, TEXT("UDynamicObstacleRegProcessor NumEntities: %d"), NumEntities);
		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			FVector Location = Transforms[i].GetTransform().GetLocation();
			float Radius = Colliders[i].GetCircleCollider().Radius;
			FVector Extent(Radius, Radius, Radius);
			FBox Bounds(Location - Extent, Location + Extent);

			FMassNavigationObstacleItem Item;
			Item.Entity = Entity;
			Item.ItemFlags = EMassNavigationObstacleFlags::HasColliderData;
			NavSys->GetObstacleGridMutable().Add(Item, Bounds);
		}
	});

	
}
