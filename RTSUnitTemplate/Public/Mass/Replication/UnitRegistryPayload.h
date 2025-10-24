#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UnitRegistryPayload.generated.h"

USTRUCT()
struct RTSUNITTEMPLATE_API FUnitRegistryItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FName OwnerName = NAME_None;

	UPROPERTY()
	FMassNetworkID NetID;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FUnitRegistryArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUnitRegistryItem> Items;

	class AUnitRegistryReplicator* OwnerActor = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUnitRegistryItem, FUnitRegistryArray>(Items, DeltaParms, *this);
	}

	FUnitRegistryItem* FindByOwner(const FName InOwner)
	{
		return Items.FindByPredicate([&](const FUnitRegistryItem& It){ return It.OwnerName == InOwner; });
	}

	bool RemoveByOwner(const FName InOwner)
	{
		const int32 Removed = Items.RemoveAll([&](const FUnitRegistryItem& It){ return It.OwnerName == InOwner; });
		return Removed > 0;
	}
};

template<>
struct TStructOpsTypeTraits<FUnitRegistryArray> : public TStructOpsTypeTraitsBase2<FUnitRegistryArray>
{
	enum { WithNetDeltaSerializer = true };
};
