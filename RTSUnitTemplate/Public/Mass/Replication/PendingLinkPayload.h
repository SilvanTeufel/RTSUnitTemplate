#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PendingLinkPayload.generated.h"

USTRUCT()
struct RTSUNITTEMPLATE_API FPendingLinkItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FName OwnerName = NAME_None;

	UPROPERTY()
	FMassNetworkID NetID;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FPendingLinkArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPendingLinkItem> Items;

	class AUnitRegistryReplicator* Owner = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPendingLinkItem, FPendingLinkArray>(Items, DeltaParms, *this);
	}

	// utilities
	bool Contains(const FName InOwner, const FMassNetworkID& InID) const
	{
		for (const FPendingLinkItem& It : Items)
		{
			if (It.OwnerName == InOwner && It.NetID == InID) return true;
		}
		return false;
	}
};

template<>
struct TStructOpsTypeTraits<FPendingLinkArray> : public TStructOpsTypeTraitsBase2<FPendingLinkArray>
{
	enum { WithNetDeltaSerializer = true };
};