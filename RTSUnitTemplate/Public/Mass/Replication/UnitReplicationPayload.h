#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UnitReplicationPayload.generated.h"

// Forward Declarations
struct FUnitReplicationArray;

// Fast Array Item für Mass Entity Replikation
USTRUCT()
struct RTSUNITTEMPLATE_API FUnitReplicationItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	// Network ID of the Mass Entity
	UPROPERTY()
	FMassNetworkID NetID;

	// Stable owner identifier (authoritative key computed on server, e.g., Actor Name or NetGUID)
	UPROPERTY()
	FName OwnerName = NAME_None;

	// Position (komprimiert für Bandbreite)
	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	// Rotation (quantisiert: 0-360° -> 0-65535)
	UPROPERTY()
	uint16 PitchQuantized = 0;
	UPROPERTY()
	uint16 YawQuantized = 0;
	UPROPERTY()
	uint16 RollQuantized = 0;

	// Scale
	UPROPERTY()
	FVector_NetQuantize10 Scale = FVector(1.0f, 1.0f, 1.0f);

	// Default Constructor
	FUnitReplicationItem()
		: NetID()
		, Location(FVector::ZeroVector)
		, PitchQuantized(0)
		, YawQuantized(0)
		, RollQuantized(0)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
	{
	}

	// Callback wenn Item hinzugefügt wird (Client)
	void PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer);

	// Callback wenn Item geändert wird (Client)
	void PostReplicatedChange(const FUnitReplicationArray& InArraySerializer);

	// Callback wenn Item entfernt wird (Client)
	void PreReplicatedRemove(const FUnitReplicationArray& InArraySerializer);
};

// Fast Array für die Replikation
USTRUCT()
struct RTSUNITTEMPLATE_API FUnitReplicationArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUnitReplicationItem> Items;

	// Pointer zum Owner Actor (für Callbacks)
	class AUnitClientBubbleInfo* OwnerBubble = nullptr;

	// Fast Array Serialization
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUnitReplicationItem, FUnitReplicationArray>(Items, DeltaParms, *this);
	}

	// Hilfsfunktion: Finde Item by NetID
	FUnitReplicationItem* FindItemByNetID(const FMassNetworkID& InNetID)
	{
		return Items.FindByPredicate([&InNetID](const FUnitReplicationItem& Item)
		{
			return Item.NetID == InNetID;
		});
	}
	
	// Hilfsfunktion: Entferne Item by NetID
	bool RemoveItemByNetID(const FMassNetworkID& InNetID)
	{
		const int32 RemovedCount = Items.RemoveAll([&InNetID](const FUnitReplicationItem& Item)
		{
			return Item.NetID == InNetID;
		});
		return RemovedCount > 0;
	}
};

// Template Specialization für Net Delta Serialization
template<>
struct TStructOpsTypeTraits<FUnitReplicationArray> : public TStructOpsTypeTraitsBase2<FUnitReplicationArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
