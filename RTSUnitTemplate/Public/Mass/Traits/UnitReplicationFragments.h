// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "UnitReplicationFragments.generated.h"

// This is the fragment that will actually be sent over the network.
// It's stored on the client and updated by the replication processor.
USTRUCT()
struct RTSUNITTEMPLATE_API FUnitReplicatedTransformFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FTransform Transform;

	/**
	 * Server velocity estimated from the last two FRESH updates (client only, written by the
	 * reconciler). The bubble arrives at only 5 Hz, so Transform is up to ~200 ms old; the client
	 * uses this to extrapolate where the server unit is NOW instead of widening its thresholds.
	 */
	UPROPERTY(Transient)
	FVector EstimatedVelocity = FVector::ZeroVector;

	/** World time (s) of the last update that actually moved Transform. <0 = none yet. */
	UPROPERTY(Transient)
	double LastFreshTime = -1.0;

	/**
	 * Best guess of the CURRENT server location: Transform + EstimatedVelocity * age, with the age
	 * capped at MaxSeconds. Beyond that no data has arrived for a while - the server unit most likely
	 * stopped, so we do not keep extrapolating into nowhere.
	 */
	/**
	 * True when no server MOVEMENT has arrived for longer than StillSeconds. Unlike a "no fresh data
	 * this tick" check this does not confuse a 5 Hz bubble gap with a standing unit: a gap is at most
	 * ~0.2 s, a standing unit stays still indefinitely.
	 */
	bool IsServerStill(const double Now, const float StillSeconds) const
	{
		return LastFreshTime >= 0.0 && StillSeconds > 0.f && (Now - LastFreshTime) > StillSeconds;
	}

	/** Shared console value RTS.ClientServerStillTime (seconds), defined in ClientReplicationProcessor.cpp. */
	static float GetServerStillTime();

	FVector GetExtrapolatedLocation(const double Now, const float MaxSeconds) const
	{
		const FVector Location = Transform.GetLocation();
		if (LastFreshTime < 0.0)
		{
			return Location;
		}
		const double Age = Now - LastFreshTime;
		if (Age < 0.0 || Age > MaxSeconds)
		{
			return Location;
		}
		return Location + EstimatedVelocity * static_cast<float>(Age);
	}
};