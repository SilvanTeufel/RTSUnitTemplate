// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitRotateToTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h"
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
			
			// Authoritatively update Mass transform location from Actor to preserve it during rotation sync
			const FVector CurrentActorLocation = UnitBase->GetMassActorLocation();
			MassTransform.SetLocation(CurrentActorLocation);

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

			if (bHasTarget)
			{
				FVector Dir = TargetLocation - CurrentActorLocation;
				Dir.Z = 0.f;
				if (Dir.Normalize())
				{
					FQuat DesiredQuat = Dir.ToOrientationQuat();
					float TargetYaw = DesiredQuat.Rotator().Yaw + FollowFrag.OffsetDegrees;
					FQuat TargetQuat = FRotator(0.f, TargetYaw, 0.f).Quaternion();

					// Smooth interpolation
					float Alpha = (FollowFrag.Duration > 0.f) ? FMath::Clamp(DeltaTime / FollowFrag.Duration, 0.f, 1.f) : 1.f;
					if (FollowFrag.EaseExp != 1.0f && FollowFrag.Duration > 0.f)
					{
						Alpha = FMath::Pow(Alpha, FollowFrag.EaseExp);
					}
					

					FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, Alpha);
					MassTransform.SetRotation(NewQuat);
				}
			}

			// Always update PositionedTransform
			CharList[i].PositionedTransform = MassTransform;

			// Sync to Actor (since we are on Game Thread)
			const FRotator ActorRot = UnitBase->GetActorRotation();

			const bool bLocationChanged = !CurrentActorLocation.Equals(MassTransform.GetLocation(), 0.025f);
			const bool bRotationChanged = !ActorRot.Quaternion().Equals(MassTransform.GetRotation(), 0.0001f);

			if (bLocationChanged || bRotationChanged)
			{
				if (UnitBase->bUseSkeletalMovement || UnitBase->bUseIsmWithActorMovement)
				{
					Actor->SetActorTransform(MassTransform, false, nullptr, ETeleportType::None);
				}
				else
				{
					UnitBase->Multicast_UpdateISMInstanceTransform_Implementation(UnitBase->InstanceIndex, MassTransform);
				}
			}
		}
	});
}
