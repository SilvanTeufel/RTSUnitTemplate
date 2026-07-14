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
