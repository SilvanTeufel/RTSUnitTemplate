#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"

// Lightweight global cache to pass replicated transforms from AUnitClientBubbleInfo
// to Mass processors without requiring engine bubble handler access.
namespace UnitReplicationCache
{
	inline TMap<FMassNetworkID, FTransform>& Map()
	{
		static TMap<FMassNetworkID, FTransform> GLatestByID;
		return GLatestByID;
	}

	inline void SetLatest(const FMassNetworkID& NetID, const FTransform& Transform)
	{
		Map().Add(NetID, Transform);
	}

	inline bool GetLatest(const FMassNetworkID& NetID, FTransform& OutTransform)
	{
		if (const FTransform* Found = Map().Find(NetID))
		{
			OutTransform = *Found;
			return true;
		}
		return false;
	}

	inline void Remove(const FMassNetworkID& NetID)
	{
		Map().Remove(NetID);
	}

	inline void Clear()
	{
		Map().Reset();
	}
}
