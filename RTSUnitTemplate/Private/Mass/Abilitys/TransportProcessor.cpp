// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Abilitys/TransportProcessor.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassSignalSubsystem.h"
#include "MassExecutionContext.h"
#include "Mass/Signals/MySignals.h"

UTransportProcessor::UTransportProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UTransportProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	TransporterQuery.Initialize(EntityManager);
	TransporterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	TransporterQuery.AddRequirement<FMassTransportFragment>(EMassFragmentAccess::ReadOnly);
	TransporterQuery.AddTagRequirement<FMassTransportTag>(EMassFragmentPresence::All);
	TransporterQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	TransporterQuery.RegisterWithProcessor(*this);

	FollowerQuery.Initialize(EntityManager);
	FollowerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FollowerQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	FollowerQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	FollowerQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	FollowerQuery.RegisterWithProcessor(*this);
}

void UTransportProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UTransportProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TMap<FMassEntityHandle, FVector> TransporterLocations;
	TMap<FMassEntityHandle, float> TransporterLoadRangesSq;

	TransporterQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& TransportContext)
	{
		const int32 NumTransporters = TransportContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = TransportContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassTransportFragment> TransportList = TransportContext.GetFragmentView<FMassTransportFragment>();

		for (int32 i = 0; i < NumTransporters; ++i)
		{
			FMassEntityHandle TransporterEntity = TransportContext.GetEntity(i);
			TransporterLocations.Add(TransporterEntity, TransformList[i].GetTransform().GetLocation());
			TransporterLoadRangesSq.Add(TransporterEntity, FMath::Square(TransportList[i].InstantLoadRange));
			UE_LOG(LogTemp, Verbose, TEXT("[TransportProcessor] Registered Transporter [%d:%d] Range=%.1f Pos=%s"), TransporterEntity.Index, TransporterEntity.SerialNumber, TransportList[i].InstantLoadRange, *TransformList[i].GetTransform().GetLocation().ToString());
		}
	});

	UE_LOG(LogTemp, Log, TEXT("[TransportProcessor] Found %d transporter(s) this tick"), TransporterLocations.Num());
	if (TransporterLocations.Num() == 0)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[TransportProcessor] No transporters present; skipping."));
		return;
	}

	FollowerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& FollowerContext)
	{
		const int32 NumFollowers = FollowerContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> FollowerTransformList = FollowerContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassAITargetFragment> FollowerAIList = FollowerContext.GetFragmentView<FMassAITargetFragment>();

		for (int32 i = 0; i < NumFollowers; ++i)
		{
			FMassEntityHandle TargetEntity = FollowerAIList[i].FriendlyTargetEntity;
			if (TargetEntity.IsSet() && TransporterLocations.Contains(TargetEntity))
			{
				const FVector& TransporterLoc = TransporterLocations[TargetEntity];
				const float RangeSq = TransporterLoadRangesSq[TargetEntity];
				const float DistSq = FVector::DistSquared2D(FollowerTransformList[i].GetTransform().GetLocation(), TransporterLoc);

				if (DistSq <= RangeSq)
				{
					// Signal the follower to load
					if (SignalSubsystem)
					{
						UE_LOG(LogTemp, Log, TEXT("[TransportProcessor] Signalling LoadUnit for Entity [%d:%d] to Transporter [%d:%d] (Dist: %.2f, Range: %.2f)"), 
							FollowerContext.GetEntity(i).Index, FollowerContext.GetEntity(i).SerialNumber, TargetEntity.Index, TargetEntity.SerialNumber, FMath::Sqrt(DistSq), FMath::Sqrt(RangeSq));
						SignalSubsystem->SignalEntityDeferred(FollowerContext, UnitSignals::LoadUnit, FollowerContext.GetEntity(i));
					}
				}
			}
		}
	});
}
