// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/MassUnitHoverProcessor.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassSignalSubsystem.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

UMassUnitHoverProcessor::UMassUnitHoverProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true; // Still required to access PlayerController and potentially Actor mesh bounds
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassUnitHoverProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassHoverFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);

	SignalSubsystem = EntityManager->GetWorld()->GetSubsystem<UMassSignalSubsystem>();
}

void UMassUnitHoverProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World || !SignalSubsystem) return;

	AccumulatedTime += Context.GetDeltaTimeSeconds();
	if (AccumulatedTime < 0.1f) // 10 FPS
	{
		return;
	}
	AccumulatedTime = 0.f;

	ACustomControllerBase* LocalPC = Cast<ACustomControllerBase>(World->GetFirstPlayerController());
	if (!LocalPC || !LocalPC->IsLocalController()) return;

	FVector RayOrigin, RayDirection;
	if (!LocalPC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return;
	FVector RayEnd = RayOrigin + RayDirection * 100000.f;

	FMassEntityHandle BestEntity;
	float ClosestDistanceSq = FLT_MAX;
	int32 BestInstanceIndex = INDEX_NONE;
	TWeakObjectPtr<USkeletalMeshComponent> BestMesh = nullptr;

	EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const auto VisualFrags = ChunkContext.GetFragmentView<FMassUnitVisualFragment>();
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto ActorFrags = ChunkContext.GetFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			bool bHit = false;
			int32 CurrentInstanceIndex = INDEX_NONE;
			TWeakObjectPtr<USkeletalMeshComponent> CurrentMesh = nullptr;

			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();

			if (VisualFrags[i].bUseSkeletalMovement)
			{
				if (const AActor* Actor = ActorFrags[i].Get())
				{
					if (const ACharacter* Char = Cast<ACharacter>(Actor))
					{
						if (USkeletalMeshComponent* Mesh = Char->GetMesh())
						{
							FBox WorldBox = Mesh->Bounds.GetBox();
							if (FMath::LineBoxIntersection(WorldBox, RayOrigin, RayEnd, RayDirection))
							{
								bHit = true;
								CurrentMesh = Mesh;
							}
						}
					}
				}
			}
			else
			{
				// Check Visual ISMs instances
				for (const FMassUnitVisualInstance& Instance : VisualFrags[i].VisualInstances)
				{
					if (Instance.TargetISM.IsValid())
					{
						UStaticMesh* SM = Instance.TargetISM->GetStaticMesh();
						if (SM)
						{
							FTransform WorldTransform = EntityTransform * Instance.CurrentRelativeTransform;
							FBox WorldBox = SM->GetBounds().GetBox().TransformBy(WorldTransform);

							if (FMath::LineBoxIntersection(WorldBox, RayOrigin, RayEnd, RayDirection))
							{
								bHit = true;
								CurrentInstanceIndex = Instance.InstanceIndex;
								break; 
							}
						}
					}
				}
			}

			if (bHit)
			{
				float DistToCamSq = FVector::DistSquared(RayOrigin, EntityLocation);
				if (DistToCamSq < ClosestDistanceSq)
				{
					ClosestDistanceSq = DistToCamSq;
					BestEntity = Entity;
					BestInstanceIndex = CurrentInstanceIndex;
					BestMesh = CurrentMesh;
				}
			}
		}
	}));

	// Update hover states and signal changes
	if (BestEntity != LastHoveredEntity)
	{
		// End hover for old entity
		if (EntityManager.IsEntityValid(LastHoveredEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(LastHoveredEntity);
			if (HoverFrag)
			{
				HoverFrag->bIsHovered = false;
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapEnd, LastHoveredEntity);
			}
		}

		// Start hover for new entity
		if (EntityManager.IsEntityValid(BestEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity);
			if (HoverFrag)
			{
				HoverFrag->bIsHovered = true;
				HoverFrag->HoveredInstanceIndex = BestInstanceIndex;
				HoverFrag->HoveredMesh = BestMesh;
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapStart, BestEntity);
			}
		}

		LastHoveredEntity = BestEntity;
		
		if (BestEntity.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[HoverProcessor] New Hovered Entity: %d:%d (InstanceIndex: %d)"), BestEntity.Index, BestEntity.SerialNumber, BestInstanceIndex);
		}
	}
}
