// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "System/RTSBeaconSubsystem.h"

void URTSBeaconSubsystem::UpdateBeacons(TArray<FRTSBeaconInfo>&& InBeacons)
{
	ActiveBeacons = MoveTemp(InBeacons);
}

bool URTSBeaconSubsystem::IsLocationInBeaconRange(const FVector& Location) const
{
	for (const FRTSBeaconInfo& Beacon : ActiveBeacons)
	{
		if (FVector::Dist2D(Beacon.Location, Location) <= Beacon.Range)
		{
			return true;
		}
	}
	return false;
}

bool URTSBeaconSubsystem::GetNearestBeacon(const FVector& Location, FVector& OutBeaconLocation, float& OutRange) const
{
	const FRTSBeaconInfo* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FRTSBeaconInfo& Beacon : ActiveBeacons)
	{
		if (Beacon.Range <= 0.f)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(Beacon.Location, Location);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = &Beacon;
		}
	}

	if (!Best)
	{
		return false;
	}

	OutBeaconLocation = Best->Location;
	OutRange = Best->Range;
	return true;
}
