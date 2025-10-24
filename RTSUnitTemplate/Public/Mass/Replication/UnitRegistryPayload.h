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

	// Stable unique key for a Unit in your game (preferred over OwnerName)
	UPROPERTY()
	int32 UnitIndex = INDEX_NONE;

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

	FUnitRegistryItem* FindByUnitIndex(const int32 InUnitIndex)
	{
		return Items.FindByPredicate([&](const FUnitRegistryItem& It){ return It.UnitIndex == InUnitIndex; });
	}
	
	bool RemoveByOwner(const FName InOwner)
	{
		const int32 Removed = Items.RemoveAll([&](const FUnitRegistryItem& It){ return It.OwnerName == InOwner; });
		return Removed > 0;
	}

	bool RemoveByUnitIndex(const int32 InUnitIndex)
	{
		const int32 Removed = Items.RemoveAll([&](const FUnitRegistryItem& It){ return It.UnitIndex == InUnitIndex; });
		return Removed > 0;
	}
};

template<>
struct TStructOpsTypeTraits<FUnitRegistryArray> : public TStructOpsTypeTraitsBase2<FUnitRegistryArray>
{
	enum { WithNetDeltaSerializer = true };
};
