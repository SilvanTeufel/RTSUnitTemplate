// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Replication/MassUnitReplicatorBase.h"

#include "MassCommonFragments.h"

// Optional slice control to limit how many entities of a chunk are processed this call.
namespace ReplicationSliceControl
{
	static int32 GStartIndex = -1;
	static int32 GCount = -1;
	static bool IsActive()
	{
		return GStartIndex >= 0 && GCount >= 0;
	}
	void SetSlice(int32 StartIndex, int32 Count)
	{
		GStartIndex = FMath::Max(0, StartIndex);
		GCount = Count;
	}
	void ClearSlice()
	{
		GStartIndex = -1; GCount = -1;
	}
	void GetSlice(int32& OutStartIndex, int32& OutCount)
	{
		OutStartIndex = GStartIndex;
		OutCount = GCount;
	}
}
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassReplicationFragments.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "MassNavigationFragments.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "EngineUtils.h"
#include "MassEntitySubsystem.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationFragments.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/UnitRegistryPayload.h"
#include "MassRepresentationActorManagement.h"
#include "HAL/IConsoleManager.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameFramework/PlayerState.h"

// CVAR to control server-side MassUnitReplicatorBase logging
static TAutoConsoleVariable<int32> CVarRTS_ServerReplicator_LogLevel(
	TEXT("net.RTS.ServerReplicator.LogLevel"),
	0,
	TEXT("Logging level for MassUnitReplicatorBase: 0=Off, 1=Warn, 2=Verbose."),
	ECVF_Default);

// CVARs to reduce replication bandwidth by throttling dirty detection thresholds
static TAutoConsoleVariable<float> CVarRTS_ServerRep_LocThresholdCm(
	TEXT("net.RTS.ServerRep.LocThresholdCm"),
	50.0f,
	TEXT("Minimum location delta (cm) before marking replicated item dirty. Default 10cm."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ServerRep_AngleThresholdDeg(
	TEXT("net.RTS.ServerRep.AngleThresholdDeg"),
	15.0f,
	TEXT("Minimum rotation delta (deg) before marking replicated item dirty. Default 5deg."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ServerRep_ScaleThreshold(
	TEXT("net.RTS.ServerRep.ScaleThreshold"),
	0.1f,
	TEXT("Minimum scale component delta before marking replicated item dirty. Default 0.1."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ServerRep_HealthThreshold(
	TEXT("net.RTS.ServerRep.HealthThreshold"),
	1.0f,
	TEXT("Minimum health delta before marking replicated item dirty. Default 1.0."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ServerRep_SightRadiusThreshold(
	TEXT("net.RTS.ServerRep.SightRadiusThreshold"),
	50.0f,
	TEXT("Minimum sight radius delta before marking replicated item dirty. Default 50.0cm."),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerRep_ReplicateSeenIDs(
	TEXT("net.RTS.ServerRep.ReplicateSeenIDs"),
	0,
	TEXT("When 1, replicate the list of seen unit IDs for Fog of War. Default 0 to save bandwidth."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRTS_ServerRep_NewUnitsBudgetPerFrame(
	TEXT("net.RTS.ServerRep.NewUnitsBudgetPerFrame"),
	50,
	TEXT("Maximum number of new units to add to the replication bubble per frame to avoid oversized packets."),
	ECVF_Default);

static int32 GNewUnitsAddedThisFrame = 0;

namespace { inline int32 RepLogLevel(){ return CVarRTS_ServerReplicator_LogLevel.GetValueOnGameThread(); } }

// Helper: read bubble replication frequency (Hz) from CVAR
static float GetBubbleNetUpdateHz()
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.Bubble.NetUpdateHz")))
	{
		return FMath::Max(0.1f, Var->GetFloat());
	}
	return 10.0f;
}

// Helper: find or spawn a UnitClientBubbleInfo on the server
static AUnitClientBubbleInfo* GetOrSpawnBubble(UWorld& World)
{
	AUnitClientBubbleInfo* Bubble = nullptr;
	for (TActorIterator<AUnitClientBubbleInfo> It(&World); It; ++It)
	{
		Bubble = *It;
		break;
	}
	if (!Bubble && World.GetNetMode() != NM_Client)
	{
		FActorSpawnParameters Params;
		// Do NOT mark transient; we want this actor to replicate to clients.
		Bubble = World.SpawnActor<AUnitClientBubbleInfo>(AUnitClientBubbleInfo::StaticClass(), FTransform::Identity, Params);
		if (Bubble)
		{
			Bubble->SetReplicates(true);
			Bubble->SetNetUpdateFrequency(GetBubbleNetUpdateHz());
			Bubble->ForceNetUpdate();
		}
	}
    return Bubble;
}

static bool UpdateReplicationBits(FUnitReplicationItem& Item, FMassEntityManager& EM, FMassEntityHandle EH, AUnitClientBubbleInfo* BubbleInfo)
{
    uint32 NewBits = 0u;
    auto SetBit = [&NewBits](uint32 Bit, bool bValue) { if (bValue) NewBits |= Bit; };

    if (const FMassCombatStatsFragment* CS = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
    {
        SetBit(UnitReplicationBits::CS_IsInitialized, CS->IsInitialized);
        SetBit(UnitReplicationBits::CS_bUseProjectile, CS->bUseProjectile);
        SetBit(UnitReplicationBits::CS_bCanMoveWhileAttacking, CS->bCanMoveWhileAttacking);
        SetBit(UnitReplicationBits::CS_bRotatesToMovementIfMoveWhileAttacking, CS->bRotatesToMovementIfMoveWhileAttacking);
    }
    if (const FMassAgentCharacteristicsFragment* AC = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
    {
        SetBit(UnitReplicationBits::AC_bIsFlying, AC->bIsFlying);
        SetBit(UnitReplicationBits::AC_bIsInvisible, AC->bIsInvisible);
        SetBit(UnitReplicationBits::AC_bCanOnlyAttackFlying, AC->bCanOnlyAttackFlying);
        SetBit(UnitReplicationBits::AC_bCanOnlyAttackGround, AC->bCanOnlyAttackGround);
        SetBit(UnitReplicationBits::AC_bCanBeInvisible, AC->bCanBeInvisible);
        SetBit(UnitReplicationBits::AC_bCanDetectInvisible, AC->bCanDetectInvisible);
        SetBit(UnitReplicationBits::AC_RotatesToMovement, AC->RotatesToMovement);
        SetBit(UnitReplicationBits::AC_RotatesToEnemy, AC->RotatesToEnemy);
    }
    if (const FMassAIStateFragment* AIS = EM.GetFragmentDataPtr<FMassAIStateFragment>(EH))
    {
        SetBit(UnitReplicationBits::AIS_CanAttack, AIS->CanAttack);
        SetBit(UnitReplicationBits::AIS_CanMove, AIS->CanMove);
        SetBit(UnitReplicationBits::AIS_HoldPosition, AIS->HoldPosition);
        SetBit(UnitReplicationBits::AIS_HasAttacked, AIS->HasAttacked);
        SetBit(UnitReplicationBits::AIS_SwitchingState, AIS->SwitchingState);
        SetBit(UnitReplicationBits::AIS_IsInitialized, AIS->IsInitialized);
        SetBit(UnitReplicationBits::AIS_bFollowTarget, AIS->LastbFollowTarget);

		// --- Style Index Logic ---
		if (BubbleInfo && AIS->LastProjectileClass)
		{
			if (const FMassActorFragment* ActorFrag = EM.GetFragmentDataPtr<FMassActorFragment>(EH))
			{
				if (const AUnitBase* Unit = Cast<AUnitBase>(ActorFrag->Get()))
				{
					// Resolve unit's standard values for comparison
					float StandardSpeed = Unit->Attributes ? Unit->Attributes->GetProjectileSpeed() : 0.f;
					if (StandardSpeed <= 0.f)
					{
						if (const AProjectile* ProjCDO = Cast<AProjectile>(Unit->ProjectileBaseClass->GetDefaultObject()))
							StandardSpeed = ProjCDO->MovementSpeed;
					}

					const AProjectile* UnitProjCDO = Cast<AProjectile>(Unit->ProjectileBaseClass->GetDefaultObject());

					// Determine if the current shot is "Standard" or "Special"
					bool bIsStandard = (AIS->LastProjectileClass == Unit->ProjectileBaseClass) &&
									   FMath::IsNearlyEqual(AIS->LastProjectileSpeed, StandardSpeed, 1.0f) &&
									   AIS->LastProjectileScale.Equals(Unit->ProjectileScale, 0.05f) &&
									   FMath::IsNearlyEqual(AIS->LastProjectileSpread, 0.f, 0.1f) &&
									   (AIS->LastProjectileCount == (UnitProjCDO ? (UnitProjCDO->HomingMissleCount > 0 ? UnitProjCDO->HomingMissleCount : 1) : 1)) &&
									   (AIS->LastIsBouncingNext == false) &&
									   (AIS->LastIsBouncingBack == false) &&
									   FMath::IsNearlyEqual(AIS->LastZOffset, 0.f, 0.1f) &&
									   AIS->LastProjectileSpawnOffset.IsNearlyZero(0.1f) &&
									   (AIS->LastDisableAutoZOffset == false) &&
									   FMath::IsNearlyEqual(AIS->LastTwinProjectileDistance, UnitProjCDO ? UnitProjCDO->TwinProjectileDistance : 0.f, 0.1f);

					if (!bIsStandard)
					{
						FProjectileStyle Style;
						Style.ProjectileClass = AIS->LastProjectileClass;
						Style.Speed = AIS->LastProjectileSpeed;
						Style.Scale = AIS->LastProjectileScale;
						Style.Spread = AIS->LastProjectileSpread;
						Style.ProjectileCount = AIS->LastProjectileCount;
						Style.MaxPiercedTargets = AIS->LastProjectileMaxPiercedTargets;
						Style.IsBouncingNext = AIS->LastIsBouncingNext;
						Style.IsBouncingBack = AIS->LastIsBouncingBack;
						Style.ZOffset = AIS->LastZOffset;
						Style.SpawnOffset = AIS->LastProjectileSpawnOffset;
						Style.DisableAutoZOffset = AIS->LastDisableAutoZOffset;
						Style.TwinProjectileDistance = AIS->LastTwinProjectileDistance;
						Style.HomingInitialAngle = AIS->LastHomingInitialAngle;
						Style.HomingRotationSpeed = AIS->LastHomingRotationSpeed;
						Style.HomingMaxSpiralRadius = AIS->LastHomingMaxSpiralRadius;
						Style.HomingInterpSpeed = AIS->LastHomingInterpSpeed;
						Style.bFollowTarget = AIS->LastbFollowTarget;

						uint8 StyleIdx = BubbleInfo->GetOrCreateStyleIndex(Style);
						NewBits |= (static_cast<uint32>(StyleIdx) << UnitReplicationBits::AIS_StyleIndexShift) & UnitReplicationBits::AIS_StyleIndexMask;
					}
				}
			}
		}
    }
    if (const FMassMoveTargetFragment* MT = EM.GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
    {
        SetBit(UnitReplicationBits::Move_bHasTarget, true); 
    }
    if (const FMassVisualEffectFragment* VE = EM.GetFragmentDataPtr<FMassVisualEffectFragment>(EH))
    {
        SetBit(UnitReplicationBits::VE_bForceHidden, VE->bForceHidden);
    }
    if (const FEffectAreaImpactFragment* Impact = EM.GetFragmentDataPtr<FEffectAreaImpactFragment>(EH))
    {
        SetBit(UnitReplicationBits::EA_bImpactVFXTriggered, Impact->bImpactVFXTriggered);
        SetBit(UnitReplicationBits::EA_bIsScalingAfterImpact, Impact->bIsScalingAfterImpact);
        SetBit(UnitReplicationBits::EA_bImpactScaleTriggered, Impact->bImpactScaleTriggered);
        SetBit(UnitReplicationBits::EA_bPendingDestruction, Impact->bPendingDestruction);
    }

    const uint32 ProtectedMask = 0x1F000000; // Bits 24-28 (Slots + EA_bPendingDestruction is 24)
    // EA_bPendingDestruction is Bit 24. Slots are 25, 26, 27, 28.
    // So 0x1F000000 covers bits 24, 25, 26, 27, 28.
    
    const uint32 CombinedBits = (Item.ReplicationBits & ProtectedMask) | (NewBits & ~ProtectedMask);

    if (Item.ReplicationBits != CombinedBits)
    {
        Item.ReplicationBits = CombinedBits;
        return true;
    }
    return false;
}

void UMassUnitReplicatorBase::AddRequirements(FMassEntityQuery& EntityQuery)
{
    // Keep requirements minimal to avoid excluding entities that lack optional fragments.
    // UMassReplicatorBase already requires FMassNetworkIDFragment; only add Transform here.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    
    // Add AgentCharacteristics as optional to read visual position when available
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

    // Do NOT add optional fragments as hard requirements; we read them conditionally during serialization.
}

void UMassUnitReplicatorBase::AddEntity(FMassEntityHandle Entity, FMassReplicationContext& ReplicationContext)
{
    constexpr bool bEnableImportantLogs = false; // set to true temporarily when diagnosing replication issues
    UWorld* World = &ReplicationContext.World;
    if (!World || World->GetNetMode() == NM_Client)
    {
        return; // Only the server registers entities for replication
    }

    AUnitClientBubbleInfo* BubbleInfo = GetOrSpawnBubble(*World);
    if (!BubbleInfo)
    {
        if (RepLogLevel() >= 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - No AUnitClientBubbleInfo available in world %s"), *World->GetName());
        }
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        if (bEnableImportantLogs)
        {
            UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - No MassEntitySubsystem available in world %s"), *World->GetName());
        }
        return;
    }

    // Read fragments directly from the entity (safely)
    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
    if (!EntityManager.IsEntityActive(Entity)) return;

    // Note: We now replicate dead entities too. Do NOT early-return when FMassStateDeadTag is present.

    const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity);
    const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
    FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!NetIDFrag || !TransformFrag)
    {
        if (RepLogLevel() >= 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - Entity missing required fragments (NetID or Transform). Skipping registration."));
        }
        return;
    }

    const FMassNetworkID& NetID = NetIDFrag->NetID;
    const FTransform& Xf = TransformFrag->GetTransform();

    // Prefer visual position from AgentCharacteristics if available and valid
    const FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
    const FTransform& VisualXf = (CharFrag && !CharFrag->PositionedTransform.Equals(FTransform::Identity)) ? CharFrag->PositionedTransform : Xf;

    // 1) Ensure presence/update in replicated bubble array for clients
    FUnitReplicationItem* Item = BubbleInfo->Agents.FindItemByNetID(NetID);
    if (!Item)
    {
        // Batching: Limit new units per frame to avoid LogNetPartialBunch (64KB limit)
        if (GNewUnitsAddedThisFrame >= CVarRTS_ServerRep_NewUnitsBudgetPerFrame.GetValueOnGameThread())
        {
            return;
        }

        FUnitReplicationItem NewItem;
        NewItem.NetID = NetID;
        // Fill stable owner key if available
        if (ActorFrag)
        {
            if (AActor* Ow = ActorFrag->GetMutable())
            {
                if (AUnitBase* UnitActor = Cast<AUnitBase>(Ow))
                {
                    // Projectile data is now handled locally on client via UnitBase
                }
            }
        }
        NewItem.Location = VisualXf.GetLocation();

        auto QuantizeAngle = [](float AngleDeg)->uint16
        {
            const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
            return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
        };
        const FRotator Rot = VisualXf.Rotator();
        const uint16 YQ = QuantizeAngle(Rot.Yaw);
        NewItem.TagBits = BuildReplicatedTagBits(EntityManager, Entity);
        UpdateReplicationBits(NewItem, EntityManager, Entity, BubbleInfo);

        uint16 NewPackedEnums = 0;
        
        if (const FMassMoveTargetFragment* MT = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
        {
            // Slot 1: Target (Move)
            NewItem.TargetLoc = FVector(MT->Center);
            NewItem.ReplicationBits |= UnitReplicationBits::Slot_TargetIsMove;

            // MoveData bundling
            const uint8 QSlack = (uint8)FMath::Clamp(MT->SlackRadius, 0.f, 255.f);
            const uint16 QSpeed = (uint16)FMath::Clamp(MT->DesiredSpeed.Get(), 0.f, 4095.f);
            const uint16 QActionID = (uint16)(MT->GetCurrentActionID() & 0xFFF);
            NewItem.MoveData = (uint32)QSlack | ((uint32)QSpeed << 8) | ((uint32)QActionID << 20);

            NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();

            NewPackedEnums |= (static_cast<uint16>(MT->IntentAtGoal) << UnitReplicationBits::Packed_MoveIntentShift) & UnitReplicationBits::Packed_MoveIntentMask;

            // Distance to Goal in AuxData (Bits 24-31)
            const uint8 QDist = (uint8)FMath::Clamp(MT->DistanceToGoal / 4.f, 0.f, 255.f);
            NewItem.AuxData |= ((uint32)QDist << 24);
        }

        // Fill AI target replication fields if available
        if (const FMassAITargetFragment* AIT = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity))
        {
            if (AIT->bHasValidTarget) NewPackedEnums |= UnitReplicationBits::Packed_HasValidTarget;
            if (AIT->IsFocusedOnTarget) NewPackedEnums |= UnitReplicationBits::Packed_IsFocusedOnTarget;
            
            // If Slot 1 is NOT taken by Move, use it for AI Target
            if (!(NewItem.ReplicationBits & UnitReplicationBits::Slot_TargetIsMove))
            {
                NewItem.TargetLoc = FVector(AIT->LastKnownLocation);
                uint32 TargetNetIDVal = 0u;
                if (AIT->TargetEntity.IsSet() && EntityManager.IsEntityActive(AIT->TargetEntity))
                {
                    if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                    {
                        TargetNetIDVal = TgtNet->NetID.GetValue();
                    }
                }
                NewItem.TargetID = TargetNetIDVal;
            }

            // Ability Target in Slot 2 (Action)
            if (AIT->AbilityTargetLocation.SizeSquared() > 0.1f)
            {
                NewItem.ActionLoc = FVector(AIT->AbilityTargetLocation);
                NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsAbility;
            }

            // Sync friendly target (Alternative for Slot 2 if no Ability)
            if (!(NewItem.ReplicationBits & UnitReplicationBits::Slot_ActionIsAbility))
            {
                uint32 FriendlyTargetNetIDVal = 0u;
                if (AIT->FriendlyTargetEntity.IsSet() && EntityManager.IsEntityActive(AIT->FriendlyTargetEntity))
                {
                    if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                    {
                        FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                    }
                }
                if (FriendlyTargetNetIDVal != 0)
                {
                    NewItem.ActionID = FriendlyTargetNetIDVal;
                    NewItem.ActionLoc = FVector(AIT->LastKnownFriendlyLocation);
                    NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsFriendly;
                }
            }
        }
        
        if (const FMassAIStateFragment* AIS = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
        {
            // Projectile Target in Slot 2 (Action) - High Priority
            if (AIS->ProjectileFireCounter != 0)
            {
                NewItem.ActionLoc = FVector(AIS->LastProjectileTargetLocation);
                NewItem.ActionID = AIS->LastTargetNetID;
                NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsProjectile;

                // AuxData: ProjectileFireCounter (Bits 16-23)
                NewItem.AuxData |= ((uint32)AIS->ProjectileFireCounter << 16);
            }
        }

        if (const FRunAnimationFragment* RAF = EntityManager.GetFragmentDataPtr<FRunAnimationFragment>(Entity))
        {
            // AuxData: Duration (Bits 0-15, quant 0.01s)
            const uint16 QDur = (uint16)FMath::Clamp(RAF->Duration * 100.f, 0.f, 65535.f);
            NewItem.AuxData |= (uint32)QDur;

            NewPackedEnums |= (static_cast<uint16>(RAF->AnimationState.GetValue()) << UnitReplicationBits::Packed_AnimStateShift) & UnitReplicationBits::Packed_AnimStateMask;
        }

        if (const FMassVisualEffectFragment* VE = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity))
        {
            uint16 ActiveBits = 0;
            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);
            NewPackedEnums |= (ActiveBits << UnitReplicationBits::Packed_ActiveEffectsShift) & UnitReplicationBits::Packed_ActiveEffectsMask;
        }

        NewItem.PackedBits = (uint32)YQ | ((uint32)NewPackedEnums << 16);

        // Fill EffectArea Impact Fragment
        if (const FEffectAreaImpactFragment* Impact = EntityManager.GetFragmentDataPtr<FEffectAreaImpactFragment>(Entity))
        {
            // EA data now handled locally
        }

        if (RepLogLevel() >= 2)
        {
            //UE_LOG(LogTemp, Log, TEXT("ServerReplicate (Add): NetID=%u Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
            //    NetID.GetValue(), NewItem.CS_Health, NewItem.CS_MaxHealth, NewItem.CS_RunSpeed, NewItem.CS_TeamId,
            //    NewItem.AC_bIsFlying?1:0, NewItem.AC_bIsInvisible?1:0, NewItem.AC_FlyHeight,
            //    NewItem.AIS_StateTimer, NewItem.AIS_CanAttack?1:0, NewItem.AIS_CanMove?1:0, NewItem.AIS_HoldPosition?1:0);
        }

        const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
        GNewUnitsAddedThisFrame++;
        BubbleInfo->Agents.MarkItemDirty(BubbleInfo->Agents.Items[NewIdx]);
        BubbleInfo->Agents.MarkArrayDirty();
        BubbleInfo->ForceNetUpdate();
    }
    else
    {
        // Update initial values just in case and mark dirty using configurable thresholds
        const float LocThresh = FMath::Max(0.01f, CVarRTS_ServerRep_LocThresholdCm.GetValueOnGameThread());
        const float AngleThresh = FMath::Clamp(CVarRTS_ServerRep_AngleThresholdDeg.GetValueOnGameThread(), 0.0f, 180.0f);
        const float ScaleThresh = FMath::Max(0.0f, CVarRTS_ServerRep_ScaleThreshold.GetValueOnGameThread());
        bool bDirty = false;
        const FVector NewLoc = Xf.GetLocation();
        
        // Use Equals on Location (FVector_NetQuantize will handle quantization during serialization)
        if (!Item->Location.Equals(NewLoc, LocThresh)) { Item->Location = NewLoc; bDirty = true; }
        // Angle snapping helper based on threshold to reduce churn
        auto QuantizeAngleWithThreshold = [AngleThresh](float AngleDeg)->uint16
        {
            const float ClampedStep = FMath::Max(0.1f, AngleThresh); // avoid zero
            const float Snapped = FMath::RoundToFloat(AngleDeg / ClampedStep) * ClampedStep;
            const float Norm = FMath::Fmod(Snapped + 360.0f, 360.0f);
            return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
        };
        const FRotator Rot = Xf.Rotator();
        const uint16 NewY = QuantizeAngleWithThreshold(Rot.Yaw);
        
        // Yaw is in lower 16 bits of PackedBits
        const uint16 CurrentY = (uint16)(Item->PackedBits & 0xFFFF);
        if (CurrentY != NewY)
        {
            Item->PackedBits = (Item->PackedBits & 0xFFFF0000) | (uint32)NewY;
            bDirty = true;
        }
        if (bDirty)
        {
            BubbleInfo->Agents.MarkItemDirty(*Item);
            BubbleInfo->Agents.MarkArrayDirty();
            BubbleInfo->ForceNetUpdate();
        }
    }

    // 2) Ensure presence/update in authoritative Unit Registry on the server
    if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
    {
        // Gather owner name and unit index if available
        FName OwnerName = NAME_None;
        int32 UnitIndex = INDEX_NONE;
        if (ActorFrag)
        {
            if (AActor* Ow = ActorFrag->GetMutable())
            {
                OwnerName = Ow->GetFName();
                if (const AUnitBase* UnitActor = Cast<AUnitBase>(Ow))
                {
                    UnitIndex = UnitActor->UnitIndex;
                }
            }
        }

        // Find by NetID; update if exists, otherwise add
        bool bFound = false;
        for (FUnitRegistryItem& It : Reg->Registry.Items)
        {
            if (It.NetID == NetID)
            {
                // Update fields if changed
                bool bDirty = false;
                if (OwnerName != NAME_None && It.OwnerName != OwnerName) { It.OwnerName = OwnerName; bDirty = true; }
                if (UnitIndex != INDEX_NONE && It.UnitIndex != UnitIndex) { It.UnitIndex = UnitIndex; bDirty = true; }
                if (bDirty) { Reg->Registry.MarkItemDirty(It); }
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            const int32 NewRegIdx = Reg->Registry.Items.AddDefaulted();
            FUnitRegistryItem& NewRegItem = Reg->Registry.Items[NewRegIdx];
            NewRegItem.NetID = NetID;
            NewRegItem.OwnerName = OwnerName;
            NewRegItem.UnitIndex = UnitIndex;
            Reg->Registry.MarkItemDirty(NewRegItem);
            Reg->Registry.MarkArrayDirty();
            Reg->ForceNetUpdate();
        }
    }
}

void UMassUnitReplicatorBase::RemoveEntity(FMassEntityHandle Entity, FMassReplicationContext& ReplicationContext)
{
    UWorld* World = &ReplicationContext.World;
    if (!World || World->GetNetMode() == NM_Client)
    {
        return; // Only the server unregisters entities for replication
    }

    AUnitClientBubbleInfo* BubbleInfo = GetOrSpawnBubble(*World);
    if (!BubbleInfo)
    {
        if (RepLogLevel() >= 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::RemoveEntity - No AUnitClientBubbleInfo available in world %s"), *World->GetName());
        }
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        if (RepLogLevel() >= 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::RemoveEntity - No MassEntitySubsystem available in world %s"), *World->GetName());
        }
        return;
    }

    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
    const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity);
    if (!NetIDFrag)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::RemoveEntity - Entity missing FMassNetworkIDFragment. Skipping."));
        return;
    }
    const FMassNetworkID& NetID = NetIDFrag->NetID;

    // Remove from bubble replication array
    if (BubbleInfo->Agents.RemoveItemByNetID(NetID))
    {
        BubbleInfo->Agents.MarkArrayDirty();
        BubbleInfo->ForceNetUpdate();
    }

    // Remove from authoritative Unit Registry as well
    if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
    {
        // Only quarantine if the unit is actually dead or being destroyed
        bool bIsDead = false;
        if (const FUnitReplicationItem* Item = BubbleInfo->Agents.FindItemByNetID(NetID))
        {
            bIsDead = (Item->TagBits & UnitTagBits::Dead) != 0;
        }
        
        if (bIsDead)
        {
            Reg->QuarantineNetID(NetID.GetValue());
        }
        
        int32 Removed = 0;
        for (int32 i = Reg->Registry.Items.Num() - 1; i >= 0; --i)
        {
            if (Reg->Registry.Items[i].NetID == NetID)
            {
                Reg->Registry.Items.RemoveAt(i);
                ++Removed;
            }
        }
        if (Removed > 0)
        {
            Reg->Registry.MarkArrayDirty();
            Reg->ForceNetUpdate();
        }
    }
}

void UMassUnitReplicatorBase::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
    GNewUnitsAddedThisFrame = 0; // Reset batch budget counter
    const int32 NumEntities = Context.GetNumEntities();

    int32 SliceStart = -1, SliceCount = -1;
    ReplicationSliceControl::GetSlice(SliceStart, SliceCount);
    const bool bUseSlice = (SliceStart >= 0 && SliceCount >= 0);
    const int32 LoopStart = bUseSlice ? FMath::Clamp(SliceStart, 0, FMath::Max(0, NumEntities-1)) : 0;
    const int32 LoopEnd = bUseSlice ? FMath::Clamp(SliceStart + SliceCount, LoopStart, NumEntities) : NumEntities;

    // Use World from replication context to branch server/client
    if (ReplicationContext.World.GetNetMode() != NM_Client)
    {
        UWorld* World = &ReplicationContext.World;
        
        // Collect all existing bubbles in this world (subsystem may have created per-connection bubbles)
        TArray<AUnitClientBubbleInfo*> Bubbles;
        for (TActorIterator<AUnitClientBubbleInfo> It(World); It; ++It)
        {
            Bubbles.Add(*It);
        }
        if (Bubbles.Num() == 0)
        {
            // Fallback to a single replicated bubble
            if (AUnitClientBubbleInfo* Fallback = GetOrSpawnBubble(*World))
            {
                Bubbles.Add(Fallback);
            }
        }
        if (Bubbles.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("MassUnitReplicatorBase: No AUnitClientBubbleInfo available in world to populate Agents."));
            return;
        }

        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassNetworkIDFragment> NetIDList = Context.GetFragmentView<FMassNetworkIDFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();

        // Build a quick lookup from NetID->(OwnerName,UnitIndex) using the authoritative registry for logging
        TMap<uint32, TPair<FName,int32>> ByID;
        if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
        {
            for (const FUnitRegistryItem& It : Reg->Registry.Items)
            {
                ByID.Add(It.NetID.GetValue(), TPair<FName,int32>(It.OwnerName, It.UnitIndex));
            }
        }

        // Acquire EntityManager for tag checks
        UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        FMassEntityManager* EM = EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;

        // Collect current mouse locations for all players (for shared rotation replication)
        TArray<FPlayerMouseData> CurrentMouseDatas;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (AExtendedControllerBase* PC = Cast<AExtendedControllerBase>(It->Get()))
            {
                if (PC->PlayerState)
                {
                    CurrentMouseDatas.Add(FPlayerMouseData(PC->PlayerState->GetPlayerId(), PC->ReplicatedMouseLocation));
                }
            }
        }

        // Check if any unit in this chunk needs mouse data for rotation
        bool bChunkNeedsMouse = false;
        if (EM)
        {
            for (int32 Idx = LoopStart; Idx < LoopEnd; ++Idx)
            {
                if (DoesEntityHaveTag(*EM, Context.GetEntity(Idx), FMassRotateToMouseTag::StaticStruct()))
                {
                    bChunkNeedsMouse = true;
                    break;
                }
            }
        }

        // Update every bubble we found so clients receive replicated items
        for (AUnitClientBubbleInfo* BubbleInfo : Bubbles)
        {
            // Sync mouse data once per bubble, but only if actually needed by units in this chunk
            if (bChunkNeedsMouse)
            {
                if (BubbleInfo->PlayerMouseDatas != CurrentMouseDatas)
                {
                    BubbleInfo->PlayerMouseDatas = CurrentMouseDatas;
                }
            }

            bool bAnyDirty = false;
            for (int32 Idx = LoopStart; Idx < LoopEnd; ++Idx)
            {
                const FMassNetworkID& NetID = NetIDList[Idx].NetID;

                // We now replicate dead entities as well. Do not remove them from the bubble here.
                // Dead state will be carried in TagBits and consumers can react accordingly.

                const FTransform& Xf = TransformList[Idx].GetTransform();

                // Prefer visual position from AgentCharacteristics if available and valid
                const bool bHasChar = CharList.Num() > 0;
                const FTransform& VisualXf = (bHasChar && !CharList[Idx].PositionedTransform.Equals(FTransform::Identity)) ? CharList[Idx].PositionedTransform : Xf;

                const FVector Loc = VisualXf.GetLocation();
                const FRotator Rot = VisualXf.Rotator();
                const FVector Sca = VisualXf.GetScale3D();

                // Detailed server log for diagnostics: which NetID/transform we are replicating with identity from registry
                FName OwnerName = NAME_None;
                int32 OwnerUnitIndex = INDEX_NONE;
                if (const TPair<FName,int32>* FoundPair = ByID.Find(NetID.GetValue()))
                {
                    OwnerName = FoundPair->Key;
                    OwnerUnitIndex = FoundPair->Value;
                }
                if (RepLogLevel() >= 2)
                {
                    //UE_LOG(LogTemp, Log, TEXT("ServerReplicate: NetID=%u Owner=%s UnitIndex=%d Loc=(%.1f,%.1f,%.1f) Rot=(P%.1f Y%.1f R%.1f) Scale=(%.2f,%.2f,%.2f)"),
                    //    NetID.GetValue(), *OwnerName.ToString(), OwnerUnitIndex,
                    //    Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll, Sca.X, Sca.Y, Sca.Z);
                }

                FUnitReplicationItem* Item = BubbleInfo->Agents.FindItemByNetID(NetID);
                if (!Item)
                {
                    FUnitReplicationItem NewItem;
                    NewItem.NetID = NetID;
                    NewItem.Location = Loc;
                    auto QuantizeAngle = [](float AngleDeg)->uint16
                    {
                        const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
                        return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
                    };
                    const uint16 YQ = QuantizeAngle(Rot.Yaw);
                    const FMassEntityHandle EH = Context.GetEntity(Idx);
                    
                    if (EM)
                    {
                        NewItem.TagBits = BuildReplicatedTagBits(*EM, EH);
                        UpdateReplicationBits(NewItem, *EM, EH, BubbleInfo);

                        uint16 NewPackedEnums = 0;
                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            // Slot 1: Target (Move)
                            NewItem.TargetLoc = FVector(MT->Center);
                            NewItem.ReplicationBits |= UnitReplicationBits::Slot_TargetIsMove;
                            
                            // MoveData bundling
                            const uint8 QSlack = (uint8)FMath::Clamp(MT->SlackRadius, 0.f, 255.f);
                            const uint16 QSpeed = (uint16)FMath::Clamp(MT->DesiredSpeed.Get(), 0.f, 4095.f);
                            const uint16 QActionID = (uint16)(MT->GetCurrentActionID() & 0xFFF);
                            NewItem.MoveData = (uint32)QSlack | ((uint32)QSpeed << 8) | ((uint32)QActionID << 20);
                            
                            NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();

                            NewPackedEnums |= (static_cast<uint16>(MT->IntentAtGoal) << UnitReplicationBits::Packed_MoveIntentShift) & UnitReplicationBits::Packed_MoveIntentMask;
                            
                            // Distance to Goal in AuxData (Bits 24-31)
                            const uint8 QDist = (uint8)FMath::Clamp(MT->DistanceToGoal / 4.f, 0.f, 255.f); // 0..1020cm
                            NewItem.AuxData |= ((uint32)QDist << 24);
                        }
                        
                        // Fill AI target fields if fragment exists
                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            if (AIT->bHasValidTarget) NewPackedEnums |= UnitReplicationBits::Packed_HasValidTarget;
                            if (AIT->IsFocusedOnTarget) NewPackedEnums |= UnitReplicationBits::Packed_IsFocusedOnTarget;
                            
                            // If Slot 1 is NOT taken by Move, use it for AI Target
                            if (!(NewItem.ReplicationBits & UnitReplicationBits::Slot_TargetIsMove))
                            {
                                NewItem.TargetLoc = FVector(AIT->LastKnownLocation);
                                uint32 TargetNetIDVal = 0u;
                                if (AIT->TargetEntity.IsSet() && EM->IsEntityValid(AIT->TargetEntity))
                                {
                                    if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                                    {
                                        TargetNetIDVal = TgtNet->NetID.GetValue();
                                    }
                                }
                                NewItem.TargetID = TargetNetIDVal;
                            }
                            
                            // Ability Target in Slot 2 (Action)
                            if (AIT->AbilityTargetLocation.SizeSquared() > 0.1f)
                            {
                                NewItem.ActionLoc = FVector(AIT->AbilityTargetLocation);
                                NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsAbility;
                            }

                            // Sync friendly target (Alternative for Slot 2 if no Ability)
                            if (!(NewItem.ReplicationBits & UnitReplicationBits::Slot_ActionIsAbility))
                            {
                                uint32 FriendlyTargetNetIDVal = 0u;
                                if (AIT->FriendlyTargetEntity.IsSet() && EM->IsEntityValid(AIT->FriendlyTargetEntity))
                                {
                                    if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                                    {
                                        FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                                    }
                                }
                                if (FriendlyTargetNetIDVal != 0)
                                {
                                    NewItem.ActionID = FriendlyTargetNetIDVal;
                                    NewItem.ActionLoc = FVector(AIT->LastKnownFriendlyLocation);
                                    NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsFriendly;
                                }
                            }
                        }

                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            // Projectile Target in Slot 2 (Action) - High Priority
                            if (AIS->ProjectileFireCounter != 0) // Simple check if we ever fired
                            {
                                // We always sync Projectile data if it's active
                                NewItem.ActionLoc = FVector(AIS->LastProjectileTargetLocation);
                                NewItem.ActionID = AIS->LastTargetNetID;
                                NewItem.ReplicationBits |= UnitReplicationBits::Slot_ActionIsProjectile;
                                
                                // AuxData: ProjectileFireCounter (Bits 16-23)
                                NewItem.AuxData |= ((uint32)AIS->ProjectileFireCounter << 16);
                            }
                        }

                        // Fill RunAnimation fragment data
                        if (const FRunAnimationFragment* RAF = EM->GetFragmentDataPtr<FRunAnimationFragment>(EH))
                        {
                            // AuxData: Duration (Bits 0-15, quant 0.01s)
                            const uint16 QDur = (uint16)FMath::Clamp(RAF->Duration * 100.f, 0.f, 65535.f);
                            NewItem.AuxData |= (uint32)QDur;
                            
                            NewPackedEnums |= (static_cast<uint16>(RAF->AnimationState.GetValue()) << UnitReplicationBits::Packed_AnimStateShift) & UnitReplicationBits::Packed_AnimStateMask;
                        }

                        // Fill Visual Effect Fragment
                        if (const FMassVisualEffectFragment* VE = EM->GetFragmentDataPtr<FMassVisualEffectFragment>(EH))
                        {
                            uint16 ActiveBits = 0;
                            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
                            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
                            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);
                            NewPackedEnums |= (ActiveBits << UnitReplicationBits::Packed_ActiveEffectsShift) & UnitReplicationBits::Packed_ActiveEffectsMask;
                        }

                        NewItem.PackedBits = (uint32)YQ | ((uint32)NewPackedEnums << 16);

                        // Fill EffectArea Impact Fragment
                        if (const FEffectAreaImpactFragment* Impact = EM->GetFragmentDataPtr<FEffectAreaImpactFragment>(EH))
                        {
                            // EA data now handled locally
                        }
                    }
                    
                    const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
                    GNewUnitsAddedThisFrame++;
                    BubbleInfo->Agents.MarkItemDirty(BubbleInfo->Agents.Items[NewIdx]);
                    bAnyDirty = true;
                }
                else
                {
                    // Update logic for existing units
                    const float LocThresh = FMath::Max(0.0f, CVarRTS_ServerRep_LocThresholdCm.GetValueOnGameThread());
                    const float AngleThresh = FMath::Clamp(CVarRTS_ServerRep_AngleThresholdDeg.GetValueOnGameThread(), 0.0f, 180.0f);
                    
                    const FMassEntityHandle EH = Context.GetEntity(Idx);
                    const uint32 NewTagBits = EM ? BuildReplicatedTagBits(*EM, EH) : Item->TagBits;
                    const bool bIsDead = (NewTagBits & UnitTagBits::Dead) != 0;

                    bool bDirty = false;

                    if (!bIsDead)
                    {
                        if (!Item->Location.Equals(Loc, LocThresh)) { Item->Location = Loc; bDirty = true; }
                        
                        auto QuantizeAngleWithThreshold = [AngleThresh](float AngleDeg)->uint16
                        {
                            const float ClampedStep = FMath::Max(0.1f, AngleThresh);
                            const float Snapped = FMath::RoundToFloat(AngleDeg / ClampedStep) * ClampedStep;
                            const float Norm = FMath::Fmod(Snapped + 360.0f, 360.0f);
                            return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
                        };
                        const uint16 NewY = QuantizeAngleWithThreshold(Rot.Yaw);
                        const uint16 CurrentY = (uint16)(Item->PackedBits & 0xFFFF);
                        if (CurrentY != NewY)
                        {
                            Item->PackedBits = (Item->PackedBits & 0xFFFF0000) | (uint32)NewY;
                            bDirty = true;
                        }
                    }

                    if (EM)
                    {
                        if (Item->TagBits != NewTagBits) { Item->TagBits = NewTagBits; bDirty = true; }
                        if (UpdateReplicationBits(*Item, *EM, EH, BubbleInfo)) { bDirty = true; }

                        uint16 NewPackedEnums = (uint16)(Item->PackedBits >> 16);
                        uint32 NewMoveData = 0u; 
                        uint32 NewAuxData = 0u;
                        uint32 NewRepBits = Item->ReplicationBits & ~UnitReplicationBits::Slot_TargetIsMove & ~UnitReplicationBits::Slot_ActionIsAbility & ~UnitReplicationBits::Slot_ActionIsProjectile & ~UnitReplicationBits::Slot_ActionIsFriendly;
                        
                        // Clear slots for rebuild
                        Item->TargetLoc = FVector_NetQuantize();
                        Item->TargetID = 0;
                        Item->ActionLoc = FVector_NetQuantize();
                        Item->ActionID = 0;

                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            Item->TargetLoc = FVector(MT->Center);
                            NewRepBits |= UnitReplicationBits::Slot_TargetIsMove;
                            
                            const uint8 QSlack = (uint8)FMath::Clamp(MT->SlackRadius, 0.f, 255.f);
                            const uint16 QSpeed = (uint16)FMath::Clamp(MT->DesiredSpeed.Get(), 0.f, 4095.f);
                            const uint16 QActionID = (uint16)(MT->GetCurrentActionID() & 0xFFF);
                            NewMoveData = (uint32)QSlack | ((uint32)QSpeed << 8) | ((uint32)QActionID << 20);

                            if (Item->Move_ServerStartTime != (float)MT->GetCurrentActionServerStartTime())
                            {
                                Item->Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();
                                bDirty = true;
                            }

                            const uint16 Intent = (static_cast<uint16>(MT->IntentAtGoal) << UnitReplicationBits::Packed_MoveIntentShift) & UnitReplicationBits::Packed_MoveIntentMask;
                            NewPackedEnums = (NewPackedEnums & ~UnitReplicationBits::Packed_MoveIntentMask) | Intent;

                            const uint8 QDist = (uint8)FMath::Clamp(MT->DistanceToGoal / 4.f, 0.f, 255.f);
                            NewAuxData |= ((uint32)QDist << 24);
                        }

                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            const uint16 FlagMask = UnitReplicationBits::Packed_HasValidTarget | UnitReplicationBits::Packed_IsFocusedOnTarget;
                            uint16 Flags = 0u;
                            if (AIT->bHasValidTarget) Flags |= UnitReplicationBits::Packed_HasValidTarget;
                            if (AIT->IsFocusedOnTarget) Flags |= UnitReplicationBits::Packed_IsFocusedOnTarget;
                            NewPackedEnums = (NewPackedEnums & ~FlagMask) | Flags;

                            if (!(NewRepBits & UnitReplicationBits::Slot_TargetIsMove))
                            {
                                Item->TargetLoc = FVector(AIT->LastKnownLocation);
                                uint32 TargetNetIDVal = 0u;
                                if (AIT->TargetEntity.IsSet() && EM->IsEntityValid(AIT->TargetEntity))
                                {
                                    if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                                    {
                                        TargetNetIDVal = TgtNet->NetID.GetValue();
                                    }
                                }
                                Item->TargetID = TargetNetIDVal;
                            }

                            if (AIT->AbilityTargetLocation.SizeSquared() > 0.1f)
                            {
                                Item->ActionLoc = FVector(AIT->AbilityTargetLocation);
                                NewRepBits |= UnitReplicationBits::Slot_ActionIsAbility;
                            }
                            else
                            {
                                uint32 FriendlyTargetNetIDVal = 0u;
                                if (AIT->FriendlyTargetEntity.IsSet() && EM->IsEntityValid(AIT->FriendlyTargetEntity))
                                {
                                    if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                                    {
                                        FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                                    }
                                }
                                if (FriendlyTargetNetIDVal != 0)
                                {
                                    Item->ActionID = FriendlyTargetNetIDVal;
                                    Item->ActionLoc = FVector(AIT->LastKnownFriendlyLocation);
                                    NewRepBits |= UnitReplicationBits::Slot_ActionIsFriendly;
                                }
                            }
                        }

                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            if (AIS->ProjectileFireCounter != 0)
                            {
                                Item->ActionLoc = FVector(AIS->LastProjectileTargetLocation);
                                Item->ActionID = AIS->LastTargetNetID;
                                NewRepBits |= UnitReplicationBits::Slot_ActionIsProjectile;
                                NewAuxData |= ((uint32)AIS->ProjectileFireCounter << 16);
                            }
                        }

                        if (const FRunAnimationFragment* RAF = EM->GetFragmentDataPtr<FRunAnimationFragment>(EH))
                        {
                            const uint16 QDur = (uint16)FMath::Clamp(RAF->Duration * 100.f, 0.f, 65535.f);
                            NewAuxData |= (uint32)QDur;
                            const uint16 State = (static_cast<uint16>(RAF->AnimationState.GetValue()) << UnitReplicationBits::Packed_AnimStateShift) & UnitReplicationBits::Packed_AnimStateMask;
                            NewPackedEnums = (NewPackedEnums & ~UnitReplicationBits::Packed_AnimStateMask) | State;
                        }

                        if (const FMassVisualEffectFragment* VE = EM->GetFragmentDataPtr<FMassVisualEffectFragment>(EH))
                        {
                            uint16 ActiveBits = 0;
                            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
                            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
                            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);
                            NewPackedEnums = (NewPackedEnums & ~UnitReplicationBits::Packed_ActiveEffectsMask) | ((ActiveBits << UnitReplicationBits::Packed_ActiveEffectsShift) & UnitReplicationBits::Packed_ActiveEffectsMask);
                        }

                        const uint32 FinalPackedBits = (Item->PackedBits & 0xFFFF) | ((uint32)NewPackedEnums << 16);
                        if (Item->PackedBits != FinalPackedBits) { Item->PackedBits = FinalPackedBits; bDirty = true; }
                        if (Item->MoveData != NewMoveData) { Item->MoveData = NewMoveData; bDirty = true; }
                        if (Item->AuxData != NewAuxData) { Item->AuxData = NewAuxData; bDirty = true; }
                        if (Item->ReplicationBits != NewRepBits) { Item->ReplicationBits = NewRepBits; bDirty = true; }
                    }

                    if (bDirty)
                    {
                        BubbleInfo->Agents.MarkItemDirty(*Item);
                        bAnyDirty = true;
                    }
                }
            }
            if (bAnyDirty)
            {
                BubbleInfo->Agents.MarkArrayDirty();
                BubbleInfo->ForceNetUpdate();
            }
        }
    }
    else
    {
        // CLIENT: Stub. Replicated data application is handled by client-side processors for now.
        TArrayView<FUnitReplicatedTransformFragment> ReplicatedList = Context.GetMutableFragmentView<FUnitReplicatedTransformFragment>();
        for (int32 Idx = 0; Idx < NumEntities; ++Idx)
        {
            (void)ReplicatedList;
        }
    }
}