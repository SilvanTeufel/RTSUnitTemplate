// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/MassEffectAreaHoverProcessor.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h" // FMassActorFragment
#include "Mass/UnitMassTag.h"
#include "Actors/EffectArea.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/Abilitys/EffectAreaVisualManager.h"
#include "MassReplicationFragments.h"

UMassEffectAreaHoverProcessor::UMassEffectAreaHoverProcessor()
{
	bAutoRegisterWithProcessingPhases = false;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UMassEffectAreaHoverProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEffectAreaVisualFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassHoverFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassEffectAreaHoverProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World) return;


	ACustomControllerBase* LocalPC = Cast<ACustomControllerBase>(World->GetFirstPlayerController());
	if (!LocalPC || !LocalPC->IsLocalController()) return;

	FVector RayOrigin, RayDirection;
	if (!LocalPC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return;
	FVector RayEnd = RayOrigin + RayDirection * 100000.f;

	FMassEntityHandle BestEntity;
	float ClosestDistanceSq = FLT_MAX;

	EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto CharFrags = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		const auto ActorFrags = ChunkContext.GetFragmentView<FMassActorFragment>();
		const auto ImpactFrags = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FMassAgentCharacteristicsFragment& CharFrag = CharFrags[i];
			const FEffectAreaImpactFragment& ImpactFrag = ImpactFrags[i];

			FVector BaseLocation = EntityTransform.GetLocation();
			const AActor* Actor = ActorFrags[i].Get();

			// Always prefer Actor location for debugging if available, as it's the "truth" for projectiles etc.
			if (Actor)
			{
				BaseLocation = Actor->GetActorLocation();
			}

			// Prefer CurrentRadius from ImpactFragment, fallback to CapsuleRadius or default 100
			float Radius = ImpactFrag.CurrentRadius > 0.f ? ImpactFrag.CurrentRadius : (CharFrag.CapsuleRadius > 0.f ? CharFrag.CapsuleRadius : 100.f);
			
			FVector OutP1, OutP2;
			// Use a very long vertical segment to detect the area even if Z is wrong (2D Hover)
			FMath::SegmentDistToSegmentSafe(RayOrigin, RayEnd, BaseLocation - FVector(0,0,100000.f), BaseLocation + FVector(0,0,100000.f), OutP1, OutP2);
			float DistSq = FVector::DistSquared2D(OutP1, OutP2);

			if (DistSq <= FMath::Square(Radius))
			{
				float DistToCamSq = FVector::DistSquared(RayOrigin, BaseLocation);
				if (DistToCamSq < ClosestDistanceSq)
				{
					ClosestDistanceSq = DistToCamSq;
					BestEntity = ChunkContext.GetEntity(i);
				}
			}
		}
	}));

	if (BestEntity.IsValid())
	{
		FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity);
		if (HoverFrag)
		{
			HoverFrag->bIsHovered = true;
			
			// Logging only when hovered entity changes or every 0.5s while hovering
			if (BestEntity != LastHoveredEntity || World->GetTimeSeconds() - LastLogTime > 0.5f)
			{
				LastLogTime = World->GetTimeSeconds();
				LastHoveredEntity = BestEntity;

				const FMassVisibilityFragment& Vis = EntityManager.GetFragmentDataChecked<FMassVisibilityFragment>(BestEntity);
				const FEffectAreaVisualFragment& Visual = EntityManager.GetFragmentDataChecked<FEffectAreaVisualFragment>(BestEntity);
				const FEffectAreaImpactFragment& Impact = EntityManager.GetFragmentDataChecked<FEffectAreaImpactFragment>(BestEntity);
				const FMassActorFragment& ActorFrag = EntityManager.GetFragmentDataChecked<FMassActorFragment>(BestEntity);
				const FTransformFragment& TransFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(BestEntity);
				
				const AEffectArea* Area = Cast<AEffectArea>(ActorFrag.Get());
				FString ActorName = Area ? Area->GetName() : TEXT("None");

			}
		}
	}
	else
	{
		LastHoveredEntity = FMassEntityHandle();
	}
}
