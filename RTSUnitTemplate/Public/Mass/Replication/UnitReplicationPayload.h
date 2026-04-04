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
	uint16 PitchQuantized = 0;
	UPROPERTY()
	uint16 YawQuantized = 0;
	UPROPERTY()
	uint16 RollQuantized = 0;

	// Scale
	UPROPERTY()
	FVector_NetQuantize10 Scale;

	// Bitfield of replicated Mass state tags (subset used for client-side state/UI)
	UPROPERTY()
	uint32 TagBits = 0u;

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
	// Replicate seen sets as NetID arrays (bounded on server)
	UPROPERTY()
	TArray<uint32> AITargetPrevSeenIDs;
	
	UPROPERTY()
	TArray<uint32> AITargetCurrSeenIDs;

	 // --- FMassCombatStatsFragment (full) ---
	UPROPERTY() float CS_Health = 0.f;
	UPROPERTY() float CS_MaxHealth = 0.f;
	UPROPERTY() float CS_RunSpeed = 0.f;
	UPROPERTY() int32 CS_TeamId = 0;
	UPROPERTY() float CS_AttackRange = 0.f;
	UPROPERTY() float CS_AttackDamage = 0.f;
	UPROPERTY() float CS_AttackDuration = 0.f;
	UPROPERTY() float CS_IsAttackedDuration = 0.f;
	UPROPERTY() float CS_CastTime = 0.f;
	UPROPERTY() bool CS_IsInitialized = true;
	UPROPERTY() float CS_RotationSpeed = 0.f;
	UPROPERTY() float CS_Armor = 0.f;
	UPROPERTY() float CS_MagicResistance = 0.f;
	UPROPERTY() float CS_Shield = 0.f;
	UPROPERTY() float CS_MaxShield = 0.f;
	UPROPERTY() float CS_SightRadius = 0.f;
	UPROPERTY() float CS_LoseSightRadius = 0.f;
	UPROPERTY() float CS_PauseDuration = 0.f;
	UPROPERTY() bool CS_bUseProjectile = false;

	// --- FMassAgentCharacteristicsFragment (subset) ---
	UPROPERTY() bool AC_bIsFlying = false;
	UPROPERTY() bool AC_bIsInvisible = false;
	UPROPERTY() float AC_FlyHeight = 0.f;
	// Extended AgentCharacteristics (full)
	UPROPERTY() bool AC_bCanOnlyAttackFlying = true;
	UPROPERTY() bool AC_bCanOnlyAttackGround = true;
	UPROPERTY() bool AC_bCanBeInvisible = false;
	UPROPERTY() bool AC_bCanDetectInvisible = false;
	UPROPERTY() float AC_LastGroundLocation = 0.f;
	UPROPERTY() float AC_DespawnTime = 0.f;

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

	UPROPERTY() bool AC_RotatesToMovement = true;
	UPROPERTY() bool AC_RotatesToEnemy = true;
	UPROPERTY() float AC_RotationSpeed = 0.f;
	// REMOVED redundant PositionedTransform fields to save bandwidth
	UPROPERTY() float AC_CapsuleHeight = 0.f;
	UPROPERTY() float AC_CapsuleRadius = 0.f;

	// --- FMassAIStateFragment (full) ---
	UPROPERTY() float AIS_StateTimer = 0.f;
	UPROPERTY() bool AIS_CanAttack = true;
	UPROPERTY() bool AIS_CanMove = true;
	UPROPERTY() bool AIS_HoldPosition = false;
	UPROPERTY() bool AIS_HasAttacked = false;
	UPROPERTY() FName AIS_PlaceholderSignal = NAME_None;
	UPROPERTY() FVector_NetQuantize10 AIS_StoredLocation = FVector::ZeroVector;
	UPROPERTY() bool AIS_SwitchingState = false;
	UPROPERTY() float AIS_BirthTime = 0.f;
	UPROPERTY() float AIS_DeathTime = 0.f;
	UPROPERTY() bool AIS_IsInitialized = true;
	UPROPERTY() uint8 AIS_ProjectileFireCounter = 0;
	UPROPERTY() TSubclassOf<class AProjectile> AIS_ProjectileClass;
	UPROPERTY() float AIS_ProjectileSpeed = 0.f;
	UPROPERTY() float AIS_HomingInitialAngle = 0.f;
	UPROPERTY() float AIS_HomingRotationSpeed = 0.f;
	UPROPERTY() float AIS_HomingMaxSpiralRadius = 0.f;
	UPROPERTY() float AIS_HomingInterpSpeed = 2.f;
	UPROPERTY()
	bool AIS_bFollowTarget = false;
	
	UPROPERTY()
	uint32 AIS_LastTargetNetID = 0u;

	UPROPERTY()
	FVector_NetQuantize10 AIS_ProjectileTargetLocation = FVector::ZeroVector;

	UPROPERTY()
	float AIS_ProjectileSpread = 0.f;

	UPROPERTY()
	FVector_NetQuantize10 AIS_ProjectileScale = FVector::OneVector;

	UPROPERTY()
	float AIS_ProjectileDamage = 0.f;

	UPROPERTY()
	int32 AIS_ProjectileMaxPiercedTargets = 1;

	uint8 LastProjectileFireCounter = 0; // Legacy / will remove later if unused
	
	// Client-side reconciliation
	uint8 LastServerProjectileFireCounter = 0;
	uint8 PredictedPendingShots = 0;
	float PredictionTimer = 0.f;
	bool bPredictedLatch = false;

	// --- FMassMoveTargetFragment (subset + versioning) ---
	UPROPERTY() bool Move_bHasTarget = false;
	UPROPERTY() FVector_NetQuantize10 Move_Center = FVector::ZeroVector;
	UPROPERTY() float Move_SlackRadius = 0.f;
	UPROPERTY() float Move_DesiredSpeed = 0.f;
	UPROPERTY() uint8 Move_IntentAtGoal = 0; // EMassMovementAction
	UPROPERTY() float Move_DistanceToGoal = 0.f;
	// Versioning/ordering to resolve race conditions on client apply
	// Current movement action ID (from FMassMoveTargetFragment::GetCurrentActionID). 0 means unknown/legacy
	UPROPERTY() uint16 Move_ActionID = 0;
	// Server time when the action started (quantized to float). Used as tiebreaker when IDs are equal
	UPROPERTY() float Move_ServerStartTime = 0.f;
	// Current movement action enum (from GetCurrentAction). Informational; can help client-side decision making
	UPROPERTY() uint8 Move_CurrentAction = 0; // EMassMovementAction

	// --- FMassVisualEffectFragment replication ---
	UPROPERTY() uint8 VE_ActiveEffects = 0; // Bitmask: bit0=Pulsate, bit1=Rotation, bit2=Oscillation
	UPROPERTY() bool VE_bForceHidden = false;

	// Pulsate
	UPROPERTY() FVector_NetQuantize10 VE_PulsateMinScale = FVector::OneVector;
	UPROPERTY() FVector_NetQuantize10 VE_PulsateMaxScale = FVector::OneVector;
	UPROPERTY() float VE_PulsateHalfPeriod = 0.f;

	// Rotation
	UPROPERTY() FVector_NetQuantize10 VE_RotationAxis = FVector::UpVector;
	UPROPERTY() float VE_RotationDegreesPerSecond = 0.f;

	// Oscillation
	UPROPERTY() FVector_NetQuantize10 VE_OscillationOffsetA = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 VE_OscillationOffsetB = FVector::ZeroVector;
	UPROPERTY() float VE_OscillationCyclesPerSecond = 0.f;

	// Default Constructor
	FUnitReplicationItem()
		: NetID()
		, Location(FVector::ZeroVector)
		, PitchQuantized(0)
		, YawQuantized(0)
		, RollQuantized(0)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, AITargetLastKnownLocation(FVector::ZeroVector)
		, AbilityTargetLocation(FVector::ZeroVector)
		, AIS_ProjectileFireCounter(0)
		, AIS_ProjectileSpeed(0.f)
		, AIS_LastTargetNetID(0u)
		, AIS_ProjectileScale(FVector::OneVector)
		, AIS_ProjectileDamage(0.f)
		, AIS_ProjectileMaxPiercedTargets(1)
		, LastProjectileFireCounter(0)
		, LastServerProjectileFireCounter(0)
		, PredictedPendingShots(0)
		, PredictionTimer(0.f)
		, bPredictedLatch(false)
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
