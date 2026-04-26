#pragma once
#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Engine/NetSerialization.h"
#include "Net/UnrealNetwork.h"
#include "UnitReplicationPayload.generated.h"

// NOTE: Replicated fragments/tags summary and where to find them:
// - Replicated fragments (payload fields below):
//   - FTransformFragment (Location/Rotation/Scale)
//   - FMassActorFragment (OwnerName as stable owner key)
//   - FMassCombatStatsFragment (subset: Health, MaxHealth, RunSpeed, TeamId)
//   - FMassAgentCharacteristicsFragment (subset: bIsFlying, bIsInvisible, FlyHeight)
//   - FMassAIStateFragment (subset: StateTimer, CanAttack, CanMove, HoldPosition)
//   - FMassAITargetFragment (Target flags/NetID/locations)
//   - FMassMoveTargetFragment (subset: bHasTarget, Center, SlackRadius, DesiredSpeed, IntentAtGoal, DistanceToGoal)
// - Replicated tags: packed into TagBits (see ApplyReplicatedTagBits in UnitMassTag.h)
// Writers: Mass/Replication/MassUnitReplicatorBase.cpp (server side; populates Move_* from FMassMoveTargetFragment)
// Readers: Mass/Replication/ClientReplicationProcessor.cpp (client side; applies Move_* back to FMassMoveTargetFragment)
// Transport: Mass/Replication/UnitClientBubbleInfo.* (FastArray Agents)

// Forward Declarations
struct FUnitReplicationArray;

namespace UnitReplicationBits
{
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

	static constexpr uint32 AIS_StyleIndexMask = 0xFE000000; // Bits 25-31
	static constexpr uint32 AIS_StyleIndexShift = 25;
}

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
	FVector_NetQuantize Location;

	// Rotation (quantisiert: 0-360° -> 0-65535)
	UPROPERTY()
	uint16 YawQuantized = 0;

	// Scale
	UPROPERTY()
	FVector_NetQuantize10 Scale;

	// Bitfield of replicated Mass state tags (subset used for client-side state/UI)
	UPROPERTY()
	uint32 TagBits = 0u;

	// Packed booleans for various fragments to save bandwidth
	UPROPERTY()
	uint32 ReplicationBits = 0u;

	// --- AI Target replication ---
	// NetID of the current AI target entity (0 if none/invalid)
	UPROPERTY()
	uint32 AITargetNetID = 0u;
	// Flags: bit0=HasValidTarget, bit1=IsFocusedOnTarget
	UPROPERTY()
	uint8 AITargetFlags = 0u;
	// Last known location of the target
	UPROPERTY()
	FVector_NetQuantize AITargetLastKnownLocation;
	// Ability target location (coarse precision is fine)
	UPROPERTY()
	FVector_NetQuantize10 AbilityTargetLocation;

	 // --- FMassCombatStatsFragment (subset) ---
	UPROPERTY() float CS_Health = 0.f;
	UPROPERTY() float CS_Shield = 0.f;
	UPROPERTY() uint8 CS_TeamId = 0;

	// --- FMassAgentCharacteristicsFragment (subset) ---
	UPROPERTY() float AC_FlyHeight = 0.f;

	// --- FMassRotateToMouseFragment ---
	UPROPERTY()
	FVector_NetQuantize10 RotateToMouse_TargetLocation = FVector::ZeroVector;
	UPROPERTY()
	int32 RotateToMouse_PlayerId = -1;

	// --- FRunAnimationFragment ---
	UPROPERTY()
	float RunAnimation_Duration = 1.0f;
	UPROPERTY()
	uint8 RunAnimation_AnimationState = 0; // Use uint8 for EState

	// --- FMassAIStateFragment (subset) ---
	UPROPERTY() float AIS_StateTimer = 0.f;
	UPROPERTY() uint8 AIS_ProjectileFireCounter = 0;
	
	UPROPERTY()
	uint32 AIS_LastTargetNetID = 0u;

	UPROPERTY()
	FVector_NetQuantize10 AIS_ProjectileTargetLocation = FVector::ZeroVector;

	// --- Worker and Friendly Target replication ---
	UPROPERTY()
	uint32 AIFriendlyTargetNetID = 0u;

	UPROPERTY()
	FVector_NetQuantize AIFriendlyTargetLastKnownLocation;

	UPROPERTY()
	FVector_NetQuantize Worker_BuildAreaPosition;

	// --- FMassMoveTargetFragment (subset + versioning) ---
	UPROPERTY() FVector_NetQuantize10 Move_Center = FVector::ZeroVector;
	UPROPERTY() float Move_SlackRadius = 0.f;
	UPROPERTY() float Move_DesiredSpeed = 0.f;
	UPROPERTY() uint8 Move_IntentAtGoal = 0; // EMassMovementAction
	UPROPERTY() float Move_DistanceToGoal = 0.f;
	UPROPERTY() uint16 Move_ActionID = 0;
	UPROPERTY() float Move_ServerStartTime = 0.f;

	// --- FMassVisualEffectFragment replication ---
	UPROPERTY() uint8 VE_ActiveEffects = 0; // Bitmask: bit0=Pulsate, bit1=Rotation, bit2=Oscillation

	// Pulsate
	UPROPERTY() FVector_NetQuantize10 VE_PulsateMinScale = FVector::OneVector;
	UPROPERTY() FVector_NetQuantize10 VE_PulsateMaxScale = FVector::OneVector;
	UPROPERTY() float VE_PulsateHalfPeriod = 0.f;

	// --- FEffectAreaImpactFragment replication ---
	UPROPERTY() float EA_StartScaleTime = 0.f;
	UPROPERTY() FQuat EA_VisualRotationOffset = FQuat::Identity;
	UPROPERTY() float EA_RadiusAtImpactStart = 0.f;

	// Client-side reconciliation (NOT replicated)
	uint8 LastServerProjectileFireCounter = 0;
	uint8 PredictedPendingShots = 0;
	float PredictionTimer = 0.f;
	bool bPredictedLatch = false;

	// Default Constructor
	FUnitReplicationItem()
		: NetID()
		, Location(FVector::ZeroVector)
		, YawQuantized(0)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, AITargetLastKnownLocation(FVector::ZeroVector)
		, AbilityTargetLocation(FVector::ZeroVector)
		, AIS_ProjectileFireCounter(0)
		, AIS_LastTargetNetID(0u)
		, EA_StartScaleTime(0.f)
		, EA_VisualRotationOffset(FQuat::Identity)
		, EA_RadiusAtImpactStart(0.f)
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
