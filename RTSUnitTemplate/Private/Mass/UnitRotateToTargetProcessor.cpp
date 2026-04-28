// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitRotateToTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "Async/Async.h"

UUnitRotateToTargetProcessor::UUnitRotateToTargetProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteBefore.Add(TEXT("ActorTransformSyncProcessor"));
	bRequiresGameThreadExecution = true;
}

void UUnitRotateToTargetProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassUnitYawFollowFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassUnitYawFollowTag>(EMassFragmentPresence::All);
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitRotateToTargetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	static int32 LogCounter = 0;

	EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

		TArrayView<FTransformFragment> TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		TConstArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
		TConstArrayView<FMassUnitYawFollowFragment> FollowList = ChunkContext.GetFragmentView<FMassUnitYawFollowFragment>();
		TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
		TConstArrayView<FMassRepresentationLODFragment> LODList = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();
		

		for (int32 i = 0; i < NumEntities; ++i)
		{
			if (LODList[i].LOD == EMassLOD::Off) continue;

			AActor* Actor = ActorList[i].GetMutable();
			AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
			if (!UnitBase) continue;

			FTransform& MassTransform = TransformList[i].GetMutableTransform();

			const FVector PreviousLocation = MassTransform.GetLocation();
			const FQuat PreviousQuat = MassTransform.GetRotation();
			
			// Authoritatively update Mass transform location from Actor ONLY if skeletal movement is used
			if (UnitBase->bUseSkeletalMovement)
			{
				const FVector CurrentActorLocation = UnitBase->GetMassActorLocation();
				MassTransform.SetLocation(CurrentActorLocation);
			}

			const FVector CurrentLocation = MassTransform.GetLocation();
			const FQuat CurrentQuat = MassTransform.GetRotation();

			const FMassAITargetFragment& TargetFrag = TargetList[i];
			const FMassUnitYawFollowFragment& FollowFrag = FollowList[i];
			FString ActorName = Actor->GetName();

			FVector TargetLocation = FVector::ZeroVector;
			bool bHasTarget = false;

			if (EntityManager.IsEntityValid(TargetFrag.TargetEntity))
			{
				if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity))
				{
					TargetLocation = TargetXform->GetTransform().GetLocation();
					bHasTarget = true;
				}
			}
			else if (TargetFrag.bHasValidTarget && !TargetFrag.LastKnownLocation.IsNearlyZero())
			{
				TargetLocation = TargetFrag.LastKnownLocation;
				bHasTarget = true;
			}

			FQuat TargetQuat = CurrentQuat;

			if (bHasTarget)
			{
				bool bIsDead = false;
				if (EntityManager.IsEntityValid(TargetFrag.TargetEntity))
				{
					bIsDead = DoesEntityHaveTag(EntityManager, TargetFrag.TargetEntity, FMassStateDeadTag::StaticStruct());
				}

					const float Distance = FVector::Dist(CurrentLocation, TargetLocation);
					float MaxRange = 2500.f;
					if (const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(UnitBase))
					{
						if (MassUnit->MassActorBindingComponent)
						{
							MaxRange = MassUnit->MassActorBindingComponent->LoseSightRadius;
						}
					}

					if (!bIsDead && Distance <= MaxRange)
					{
						FVector Dir = TargetLocation - CurrentLocation;
					Dir.Z = 0.f;
					if (Dir.Normalize())
					{
						FQuat DesiredQuat = Dir.ToOrientationQuat();
						float TargetYaw = DesiredQuat.Rotator().Yaw + FollowFrag.OffsetDegrees;
						TargetQuat = FRotator(0.f, TargetYaw, 0.f).Quaternion();
					}
				}
			}

			// Smooth interpolation using QInterpTo for better frame-rate independence and smoothness
			float InterpSpeed = (FollowFrag.Duration > 0.f) ? (1.0f / FollowFrag.Duration) : 10.0f;
			FQuat NewQuat = FMath::QInterpTo(CurrentQuat, TargetQuat, DeltaTime, InterpSpeed);
			MassTransform.SetRotation(NewQuat);

			const bool bLocationChanged = !PreviousLocation.Equals(MassTransform.GetLocation(), 0.01f);
			const bool bRotationChanged = !PreviousQuat.Equals(MassTransform.GetRotation(), 0.0001f);

			if (UnitBase->bUseSkeletalMovement)
			{
				if (bLocationChanged || bRotationChanged)
				{
					CharList[i].PositionedTransform = MassTransform;
					CharList[i].bTransformDirty = true;
					Actor->SetActorTransform(MassTransform, false, nullptr, ETeleportType::None);
				}
			}
			else
			{
				// For normal units and buildings: Only update rotation part to avoid losing Z-offset managed by SyncProcessor
				if (bRotationChanged)
				{
					CharList[i].PositionedTransform.SetRotation(MassTransform.GetRotation());
					CharList[i].bTransformDirty = true;
				}
				
				// If location changed externally (e.g. by movement processor), 
				// we rely on ActorTransformSyncProcessor to update PositionedTransform.Z correctly.
			}
		}
	});
}
