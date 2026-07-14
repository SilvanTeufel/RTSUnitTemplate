// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassBeaconProcessor.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "System/RTSBeaconSubsystem.h"
#include "MassExecutionContext.h"

UMassBeaconProcessor::UMassBeaconProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // To access the World Subsystem and use GetWorld()
}

void UMassBeaconProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassBeaconFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassBeaconProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun = 0.f;

	TArray<FRTSBeaconInfo> CurrentBeacons;

	EntityQuery.ForEachEntityChunk(Context, [&CurrentBeacons](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassBeaconFragment> Beacons = ChunkContext.GetFragmentView<FMassBeaconFragment>();
		const int32 NumEntities = ChunkContext.GetNumEntities();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			if (Beacons[i].BeaconRange > 0.f)
			{
				FRTSBeaconInfo Info;
				Info.Location = Transforms[i].GetTransform().GetLocation();
				Info.Range = Beacons[i].BeaconRange;
				CurrentBeacons.Add(Info);
			}
		}
	});

	if (URTSBeaconSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<URTSBeaconSubsystem>())
	{
		Subsystem->UpdateBeacons(MoveTemp(CurrentBeacons));
	}
}
