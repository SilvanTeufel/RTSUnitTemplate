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
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Async/Async.h"
#include "MassEntitySubsystem.h"

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
	EntityQuery.AddTagRequirement<FMassHoverTag>(EMassFragmentPresence::All); // Nur Entities im Hover-Zustand
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassUnitHoverProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	
	if (UWorld* World = Owner.GetWorld())
	{
		SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	}

	if (SignalSubsystem)
	{
		CustomOverlapStartDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapStart)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UMassUnitHoverProcessor, HandleCustomOverlapStart));

		CustomOverlapEndDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapEnd)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UMassUnitHoverProcessor, HandleCustomOverlapEnd));
	}
}

void UMassUnitHoverProcessor::BeginDestroy()
{
	if (SignalSubsystem)
	{
		if (CustomOverlapStartDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapStart);
			Delegate.Remove(CustomOverlapStartDelegateHandle);
			CustomOverlapStartDelegateHandle.Reset();
		}

		if (CustomOverlapEndDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapEnd);
			Delegate.Remove(CustomOverlapEndDelegateHandle);
			CustomOverlapEndDelegateHandle.Reset();
		}
	}

	Super::BeginDestroy();
}

static float GetRadiusFromBounds(const FVector& LocalExtent, const FVector& LocalDir)
{
	float AbsX = FMath::Abs(LocalDir.X);
	float AbsY = FMath::Abs(LocalDir.Y);

	// Ensure Extent is valid to avoid div by zero
	float Ex = FMath::Max(1.0f, LocalExtent.X);
	float Ey = FMath::Max(1.0f, LocalExtent.Y);

	if (AbsX < 1e-4f && AbsY < 1e-4f) return FMath::Max(Ex, Ey);

	// Normalize LocalDir in 2D for radius calculation
	float L = FMath::Sqrt(AbsX * AbsX + AbsY * AbsY);
	float nX = AbsX / L;
	float nY = AbsY / L;

	return 1.0f / FMath::Max(nX / Ex, nY / Ey);
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

			// Project mouse to Entity's Z plane
			if (FMath::IsNearlyZero(RayDirection.Z)) continue;
			float t = (EntityLocation.Z - RayOrigin.Z) / RayDirection.Z;
			if (t < 0) continue;
			FVector MousePosAtZ = RayOrigin + t * RayDirection;

			FVector DirToMouse = MousePosAtZ - EntityLocation;
			DirToMouse.Z = 0.f;
			float DistSq = DirToMouse.SizeSquared();

			if (VisualFrags[i].bUseSkeletalMovement)
			{
				if (const AActor* Actor = ActorFrags[i].Get())
				{
					if (const ACharacter* Char = Cast<ACharacter>(Actor))
					{
						if (USkeletalMeshComponent* Mesh = Char->GetMesh())
						{
							FVector LocalExtent = Mesh->Bounds.BoxExtent; // Fallback
							if (Mesh->GetSkeletalMeshAsset())
							{
								LocalExtent = Mesh->GetSkeletalMeshAsset()->GetBounds().BoxExtent * EntityTransform.GetScale3D();
							}

							FVector LocalDir = EntityTransform.GetRotation().UnrotateVector(DirToMouse);
							LocalDir.Z = 0.f;

							if (DistSq < 1.0f) {
								bHit = true;
							} else {
								float Radius = GetRadiusFromBounds(LocalExtent, LocalDir.GetSafeNormal2D());
								if (DistSq <= FMath::Square(Radius)) {
									bHit = true;
								}
							}

							if (bHit) CurrentMesh = Mesh;
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
							FVector InstanceLocation = WorldTransform.GetLocation();
							
							FVector InstanceDirToMouse = MousePosAtZ - InstanceLocation;
							InstanceDirToMouse.Z = 0.f;
							float InstanceDistSq = InstanceDirToMouse.SizeSquared();

							FVector LocalExtent = SM->GetBounds().BoxExtent * WorldTransform.GetScale3D();
							FVector LocalDir = WorldTransform.GetRotation().UnrotateVector(InstanceDirToMouse);
							LocalDir.Z = 0.f;

							if (InstanceDistSq < 1.0f) {
								bHit = true;
							} else {
								float Radius = GetRadiusFromBounds(LocalExtent, LocalDir.GetSafeNormal2D());
								if (InstanceDistSq <= FMath::Square(Radius)) {
									bHit = true;
								}
							}

							if (bHit)
							{
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

	// Detect if we changed entity OR instance index/mesh for the same entity
	bool bInstanceChanged = false;
	if (BestEntity.IsValid() && BestEntity == LastHoveredEntity)
	{
		if (FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity))
		{
			if (HoverFrag->HoveredInstanceIndex != BestInstanceIndex || HoverFrag->HoveredMesh != BestMesh)
			{
				bInstanceChanged = true;
			}
		}
	}

	// Update hover states and signal changes
	if (BestEntity != LastHoveredEntity || bInstanceChanged)
	{
		// End hover for old entity (or old instance)
		if (EntityManager.IsEntityValid(LastHoveredEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(LastHoveredEntity);
			if (HoverFrag && HoverFrag->bIsHovered)
			{
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapEnd, LastHoveredEntity);
				HoverFrag->bIsHovered = false;
			}
		}

		// Start hover for new entity
		if (EntityManager.IsEntityValid(BestEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity);
			if (HoverFrag && !HoverFrag->bIsHovered)
			{
				HoverFrag->HoveredInstanceIndex = BestInstanceIndex;
				HoverFrag->HoveredMesh = BestMesh;
				HoverFrag->bIsHovered = true;
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapStart, BestEntity);
			}
		}

		LastHoveredEntity = BestEntity;
		
	}
}

void UMassUnitHoverProcessor::HandleCustomOverlapStart(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	UWorld* World = GetWorld();
	if (!SignalSubsystem || !World) return;

	TArray<FMassEntityHandle> EntitiesCopy = Entities;

	AsyncTask(ENamedThreads::GameThread, [this, World, EntitiesCopy]()
	{
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem) return;
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (FMassEntityHandle Entity : EntitiesCopy)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
				FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(Entity);
				
				if (ActorFrag && HoverFrag)
				{
					if (HoverFrag->LastStartSignalFrame == GFrameCounter) continue;
					HoverFrag->LastStartSignalFrame = GFrameCounter;

					if (AMassUnitBase* Unit = const_cast<AMassUnitBase*>(Cast<AMassUnitBase>(ActorFrag->Get())))
					{
						Unit->CustomOverlapStart(HoverFrag->HoveredInstanceIndex, HoverFrag->HoveredMesh.Get());

						if (this->bSetCustomDataValue && HoverFrag->HoveredInstanceIndex != INDEX_NONE)
						{
							const FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
							if (VisualFrag)
							{
								for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
								{
									if (Instance.InstanceIndex == HoverFrag->HoveredInstanceIndex && Instance.TargetISM.IsValid())
									{
										Instance.TargetISM->SetCustomDataValue(Instance.InstanceIndex, 0, 1.0f, true);
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	});
}

void UMassUnitHoverProcessor::HandleCustomOverlapEnd(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	UWorld* World = GetWorld();
	if (!SignalSubsystem || !World) return;

	TArray<FMassEntityHandle> EntitiesCopy = Entities;

	AsyncTask(ENamedThreads::GameThread, [this, World, EntitiesCopy]()
	{
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem) return;
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (FMassEntityHandle Entity : EntitiesCopy)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
				FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(Entity);
				
				if (ActorFrag && HoverFrag)
				{
					if (HoverFrag->LastEndSignalFrame == GFrameCounter) continue;
					HoverFrag->LastEndSignalFrame = GFrameCounter;

					if (AMassUnitBase* Unit = const_cast<AMassUnitBase*>(Cast<AMassUnitBase>(ActorFrag->Get())))
					{
						Unit->CustomOverlapEnd(HoverFrag->HoveredInstanceIndex, HoverFrag->HoveredMesh.Get());

						if (this->bSetCustomDataValue && HoverFrag->HoveredInstanceIndex != INDEX_NONE)
						{
							const FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
							if (VisualFrag)
							{
								for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
								{
									if (Instance.InstanceIndex == HoverFrag->HoveredInstanceIndex && Instance.TargetISM.IsValid())
									{
										Instance.TargetISM->SetCustomDataValue(Instance.InstanceIndex, 0, 0.0f, true);
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	});
}
