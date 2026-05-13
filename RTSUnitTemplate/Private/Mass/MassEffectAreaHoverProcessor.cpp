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

UMassEffectAreaHoverProcessor::UMassEffectAreaHoverProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
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
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassEffectAreaHoverProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World) return;

	AccumulatedTime += Context.GetDeltaTimeSeconds();
	if (AccumulatedTime < 0.2f) return;
	AccumulatedTime = 0.f;

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

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FMassAgentCharacteristicsFragment& CharFrag = CharFrags[i];

			FVector BaseLocation = EntityTransform.GetLocation();
			float Radius = CharFrag.CapsuleRadius > 0.f ? CharFrag.CapsuleRadius : 100.f;
			
			FVector OutP1, OutP2;
			FMath::SegmentDistToSegmentSafe(RayOrigin, RayEnd, BaseLocation - FVector(0,0,100), BaseLocation + FVector(0,0,100), OutP1, OutP2);
			float DistSq = FVector::DistSquared(OutP1, OutP2);

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
			static float LastLogTime = 0.f;
			if (BestEntity != LastHoveredEntity || World->GetTimeSeconds() - LastLogTime > 0.5f)
			{
				LastLogTime = World->GetTimeSeconds();
				LastHoveredEntity = BestEntity;

				const FMassVisibilityFragment& Vis = EntityManager.GetFragmentDataChecked<FMassVisibilityFragment>(BestEntity);
				const FEffectAreaVisualFragment& Visual = EntityManager.GetFragmentDataChecked<FEffectAreaVisualFragment>(BestEntity);
				const FEffectAreaImpactFragment& Impact = EntityManager.GetFragmentDataChecked<FEffectAreaImpactFragment>(BestEntity);
				const FMassActorFragment& ActorFrag = EntityManager.GetFragmentDataChecked<FMassActorFragment>(BestEntity);
				
				const AEffectArea* Area = Cast<AEffectArea>(ActorFrag.Get());
				FString ActorName = Area ? Area->GetName() : TEXT("None");

				UE_LOG(LogTemp, Warning, TEXT("[EffectAreaDebug] Entity: %d (SN:%d), Actor: %s"), BestEntity.Index, BestEntity.SerialNumber, *ActorName);
				UE_LOG(LogTemp, Warning, TEXT("  - Fragment Vis: bIsOnViewport=%d, bIsVisibleEnemy=%d, bAffectedByFogOfWar=%d, bIsMyTeam=%d"), 
					Vis.bIsOnViewport, Vis.bIsVisibleEnemy, Vis.bAffectedByFogOfWar, Vis.bIsMyTeam);
				
				if (Area)
				{
					UE_LOG(LogTemp, Warning, TEXT("  - Actor Vis: IsHidden=%d, RootVisible=%d, bIsVisibleByFog=%d, bIsOnViewport=%d, bIsInvisible=%d"), 
						Area->IsHidden(), Area->GetRootComponent() && Area->GetRootComponent()->IsVisible(), Area->bIsVisibleByFog, Area->bIsOnViewport, Area->bIsInvisible);
				}

				UE_LOG(LogTemp, Warning, TEXT("  - Visual: InstanceIndex=%d, BaseMeshRadius=%.2f"), Visual.InstanceIndex, Visual.BaseMeshRadius);
				UE_LOG(LogTemp, Warning, TEXT("  - Impact: CurrentRadius=%.2f, bScaleMesh=%d, bPendingDestruction=%d"), Impact.CurrentRadius, Impact.bScaleMesh, Impact.bPendingDestruction);

				if (Visual.ISMComponent.IsValid())
				{
					UInstancedStaticMeshComponent* ISM = Visual.ISMComponent.Get();
					FTransform InstanceTransform;
					if (ISM->GetInstanceTransform(Visual.InstanceIndex, InstanceTransform, true))
					{
						FVector Scale = InstanceTransform.GetScale3D();
						UE_LOG(LogTemp, Warning, TEXT("  - ISM Scale: (%s), Loc: (%s)"), *Scale.ToString(), *InstanceTransform.GetLocation().ToString());
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  - ISM Scale: Failed to get transform for index %d"), Visual.InstanceIndex);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("  - Visual: ISMComponent is NULL"));
				}
			}
		}
	}
	else
	{
		LastHoveredEntity = FMassEntityHandle();
	}
}
