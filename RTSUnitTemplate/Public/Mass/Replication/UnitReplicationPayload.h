#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Engine/NetSerialization.h"
#include "Net/UnrealNetwork.h"
#include "UnitReplicationPayload.generated.h"

struct FUnitReplicationArray;

namespace UnitReplicationBits
{
	// Replication Bits (Used in UpdateReplicationBits)
	static constexpr uint32 CS_IsInitialized = 1u << 0;
	static constexpr uint32 CS_bUseProjectile = 1u << 1;
	static constexpr uint32 CS_bCanMoveWhileAttacking = 1u << 2;
	static constexpr uint32 CS_bRotatesToMovementIfMoveWhileAttacking = 1u << 3;
    
	static constexpr uint32 AC_bIsFlying = 1u << 4;
	static constexpr uint32 AC_bIsInvisible = 1u << 5;
	static constexpr uint32 AC_bCanOnlyAttackFlying = 1u << 6;
	static constexpr uint32 AC_bCanOnlyAttackGround = 1u << 7;
	static constexpr uint32 AC_bCanBeInvisible = 1u << 8;
	static constexpr uint32 AC_bCanDetectInvisible = 1u << 9;
	static constexpr uint32 AC_RotatesToMovement = 1u << 10;
	static constexpr uint32 AC_RotatesToEnemy = 1u << 11;
    
	static constexpr uint32 AIS_CanAttack = 1u << 12;
	static constexpr uint32 AIS_CanMove = 1u << 13;
	static constexpr uint32 AIS_HoldPosition = 1u << 14;
	static constexpr uint32 AIS_HasAttacked = 1u << 15;
	static constexpr uint32 AIS_SwitchingState = 1u << 16;
	static constexpr uint32 AIS_IsInitialized = 1u << 17;
	static constexpr uint32 AIS_bFollowTarget = 1u << 18;
    
	static constexpr uint32 Move_bHasTarget = 1u << 19;
	static constexpr uint32 VE_bForceHidden = 1u << 20;
    
	static constexpr uint32 EA_bImpactVFXTriggered = 1u << 21;
	static constexpr uint32 EA_bIsScalingAfterImpact = 1u << 22;
	static constexpr uint32 EA_bImpactScaleTriggered = 1u << 23;
	static constexpr uint32 EA_bPendingDestruction = 1u << 24;

	static constexpr uint32 AIS_StyleIndexShift = 25;
	static constexpr uint32 AIS_StyleIndexMask = 0x7F000000;

	// PackedEnums Bits (16-bit)
	static constexpr uint16 Packed_HasValidTarget = 1u << 0;
	static constexpr uint16 Packed_IsFocusedOnTarget = 1u << 1;
    
	static constexpr uint16 Packed_AnimStateShift = 2; // 4 bits (2-5)
	static constexpr uint16 Packed_AnimStateMask = 0x003C;
    
	static constexpr uint16 Packed_MoveIntentShift = 6; // 4 bits (6-9)
	static constexpr uint16 Packed_MoveIntentMask = 0x03C0;
    
	static constexpr uint16 Packed_ActiveEffectsShift = 10; // 3 bits (10-12)
	static constexpr uint16 Packed_ActiveEffectsMask = 0x1C00;

	// Slot Indicators in ReplicationBits (Starting from Bit 25)
	static constexpr uint32 Slot_TargetIsMove = 1u << 25;
	static constexpr uint32 Slot_ActionIsProjectile = 1u << 26;
	static constexpr uint32 Slot_ActionIsFriendly = 1u << 27;
	static constexpr uint32 Slot_ActionIsAbility = 1u << 28;
}

USTRUCT()
struct FUnitReplicationItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY() FMassNetworkID NetID;                // Prop 1
	UPROPERTY() FVector_NetQuantize Location;        // Prop 2
	
	// Bits 0-15: Yaw (quantized), Bits 16-31: PackedEnums
	UPROPERTY() uint32 PackedBits = 0u;              // Prop 3
	UPROPERTY() uint32 TagBits = 0u;                 // Prop 4
	UPROPERTY() uint32 ReplicationBits = 0u;         // Prop 5
	
	// Shared Slot 1: Target (Move-Ziel ODER AI-Ziel)
	UPROPERTY() uint32 TargetID = 0u;                // Prop 6
	UPROPERTY() FVector_NetQuantize TargetLoc;       // Prop 7
	
	// Shared Slot 2: Action (Projectile, Ability ODER Friendly-Ziel)
	UPROPERTY() uint32 ActionID = 0u;                // Prop 8
	UPROPERTY() FVector_NetQuantize ActionLoc;       // Prop 9
	
	// Bits 0-7: Slack(quant), Bits 8-19: Speed(quant), Bits 20-31: ActionID(12 bit)
	UPROPERTY() uint32 MoveData = 0u;                // Prop 10
	UPROPERTY() float Move_ServerStartTime = 0.f;    // Prop 11

	// Bits 0-15: RunAnimDuration (quant 0.01s), Bits 16-23: ProjectileFireCounter, Bits 24-31: AIS_LastTargetNetID (partial or other)
	UPROPERTY() uint32 AuxData = 0u;                 // Prop 12

	// Client-side prediction state (not replicated)
	uint8 LastServerProjectileFireCounter = 0;
	uint8 PredictedPendingShots = 0;
	float PredictionTimer = 0.f;
	bool bPredictedLatch = false;

	FUnitReplicationItem() {}
	void PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer);
	void PostReplicatedChange(const FUnitReplicationArray& InArraySerializer);
	void PreReplicatedRemove(const FUnitReplicationArray& InArraySerializer);
};

USTRUCT()
struct FUnitReplicationArray : public FFastArraySerializer
{
	GENERATED_BODY()
	UPROPERTY() TArray<FUnitReplicationItem> Items;
	class AUnitClientBubbleInfo* OwnerBubble = nullptr;

	FUnitReplicationItem* FindItemByNetID(const FMassNetworkID& NetID)
	{
		for (FUnitReplicationItem& Item : Items)
		{
			if (Item.NetID == NetID) return &Item;
		}
		return nullptr;
	}

	const FUnitReplicationItem* FindItemByNetID(const FMassNetworkID& NetID) const
	{
		for (const FUnitReplicationItem& Item : Items)
		{
			if (Item.NetID == NetID) return &Item;
		}
		return nullptr;
	}

	bool RemoveItemByNetID(const FMassNetworkID& NetID)
	{
		const int32 Removed = Items.RemoveAll([&NetID](const FUnitReplicationItem& Item) { return Item.NetID == NetID; });
		return Removed > 0;
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUnitReplicationItem, FUnitReplicationArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FUnitReplicationArray> : public TStructOpsTypeTraitsBase2<FUnitReplicationArray>
{
	enum { WithNetDeltaSerializer = true };
};
