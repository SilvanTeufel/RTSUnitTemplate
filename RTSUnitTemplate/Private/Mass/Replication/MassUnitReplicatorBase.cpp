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

    if (Item.ReplicationBits != NewBits)
    {
        Item.ReplicationBits = NewBits;
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
                NewItem.OwnerName = Ow->GetFName();
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
        NewItem.YawQuantized = QuantizeAngle(Rot.Yaw);
        NewItem.TagBits = BuildReplicatedTagBits(EntityManager, Entity);
        UpdateReplicationBits(NewItem, EntityManager, Entity, BubbleInfo);

        
            if (const FMassMoveTargetFragment* MT = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
            {
                NewItem.Move_Center = (FVector3f)MT->Center;
                NewItem.Move_SlackRadius = MT->SlackRadius;
                NewItem.Move_DesiredSpeed = MT->DesiredSpeed.Get();
                NewItem.Move_IntentAtGoal = static_cast<uint8>(MT->IntentAtGoal);
                NewItem.Move_DistanceToGoal = MT->DistanceToGoal;
                // Versioning fields to allow client to resolve newer vs older
                NewItem.Move_ActionID = MT->GetCurrentActionID();
                NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();
            }
       
        // Fill AI target replication fields if available
        if (const FMassAITargetFragment* AIT = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity))
        {
            // Flags
            NewItem.AITargetFlags = 0u;
            if (AIT->bHasValidTarget) NewItem.AITargetFlags |= 1u;
            if (AIT->IsFocusedOnTarget) NewItem.AITargetFlags |= 2u;
            NewItem.AITargetLastKnownLocation = (FVector3f)AIT->LastKnownLocation;
            NewItem.AbilityTargetLocation = (FVector3f)AIT->AbilityTargetLocation;
            // Resolve target NetID if the target entity is valid
            uint32 TargetNetIDVal = 0u;
            if (AIT->TargetEntity.IsSet() && EntityManager.IsEntityActive(AIT->TargetEntity))
            {
                if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                {
                    TargetNetIDVal = TgtNet->NetID.GetValue();
                }
            }
            NewItem.AITargetNetID = TargetNetIDVal;

            // Resolve friendly target NetID
            uint32 FriendlyTargetNetIDVal = 0u;
            if (AIT->FriendlyTargetEntity.IsSet() && EntityManager.IsEntityActive(AIT->FriendlyTargetEntity))
            {
                if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                {
                    FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                }
            }
            NewItem.AIFriendlyTargetNetID = FriendlyTargetNetIDVal;
            NewItem.AIFriendlyTargetLastKnownLocation = (FVector3f)AIT->LastKnownFriendlyLocation;
            NewItem.Worker_BuildAreaPosition = (FVector3f)AIT->LastKnownFriendlyLocation;
        }
        // Fill additional fragments: CombatStats, AgentCharacteristics, AIState
        if (const FMassCombatStatsFragment* CS = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
        {
            // CS_Health, CS_Shield, CS_TeamId are now synchronized locally to save bandwidth
            // NewItem.CS_Health = CS->Health;
            // NewItem.CS_Shield = CS->Shield;
            // NewItem.CS_TeamId = (uint8)CS->TeamId;
        }
        if (const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity))
        {
            // AC_FlyHeight is now synchronized locally
            // NewItem.AC_FlyHeight = AC->FlyHeight;
        }
        if (const FMassAIStateFragment* AIS = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
        {
            // AIS_StateTimer is now synchronized locally
            // NewItem.AIS_StateTimer = AIS->StateTimer;
            NewItem.AIS_ProjectileFireCounter = AIS->ProjectileFireCounter;
            NewItem.AIS_LastTargetNetID = AIS->LastTargetNetID;
            NewItem.AIS_ProjectileTargetLocation = (FVector3f)AIS->LastProjectileTargetLocation;
        }

        // Fill Visual Effect Fragment
        if (const FMassVisualEffectFragment* VE = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity))
        {
            uint8 ActiveBits = 0;
            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);
            NewItem.VE_ActiveEffects = ActiveBits;
            
            // Pulsate data now handled locally
        }

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
        const float LocThresh = FMath::Max(0.0f, CVarRTS_ServerRep_LocThresholdCm.GetValueOnGameThread());
        const float AngleThresh = FMath::Clamp(CVarRTS_ServerRep_AngleThresholdDeg.GetValueOnGameThread(), 0.0f, 180.0f);
        const float ScaleThresh = FMath::Max(0.0f, CVarRTS_ServerRep_ScaleThreshold.GetValueOnGameThread());
        bool bDirty = false;
        const FVector NewLoc = Xf.GetLocation();
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
        if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
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
                    NewItem.OwnerName = OwnerName;
                    NewItem.Location = Loc;
                    auto QuantizeAngle = [](float AngleDeg)->uint16
                    {
                        const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
                        return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
                    };
                    NewItem.YawQuantized = QuantizeAngle(Rot.Yaw);
                    if (EM)
                    {
                        const FMassEntityHandle EH = Context.GetEntity(Idx);
                        NewItem.TagBits = BuildReplicatedTagBits(*EM, EH);
                        UpdateReplicationBits(NewItem, *EM, EH, BubbleInfo);
                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            NewItem.Move_Center = (FVector3f)MT->Center;
                            NewItem.Move_SlackRadius = MT->SlackRadius;
                            NewItem.Move_DesiredSpeed = MT->DesiredSpeed.Get();
                            NewItem.Move_IntentAtGoal = static_cast<uint8>(MT->IntentAtGoal);
                            NewItem.Move_DistanceToGoal = MT->DistanceToGoal;
                            // Versioning fields to allow client to resolve newer vs older
                            NewItem.Move_ActionID = MT->GetCurrentActionID();
                            NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();
                        }
                        
                        // Fill AI target fields if fragment exists
                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            NewItem.AITargetFlags = 0u;
                            if (AIT->bHasValidTarget) NewItem.AITargetFlags |= 1u;
                            if (AIT->IsFocusedOnTarget) NewItem.AITargetFlags |= 2u;
                            NewItem.AITargetLastKnownLocation = (FVector3f)AIT->LastKnownLocation;
                            NewItem.AbilityTargetLocation = (FVector3f)AIT->AbilityTargetLocation;
                            uint32 TargetNetIDVal = 0u;
                            if (AIT->TargetEntity.IsSet() && EM->IsEntityValid(AIT->TargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                                {
                                    TargetNetIDVal = TgtNet->NetID.GetValue();
                                }
                            }
                            NewItem.AITargetNetID = TargetNetIDVal;

                            // Resolve friendly target NetID
                            uint32 FriendlyTargetNetIDVal = 0u;
                            if (AIT->FriendlyTargetEntity.IsSet() && EM->IsEntityValid(AIT->FriendlyTargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                                {
                                    FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                                }
                            }
                            NewItem.AIFriendlyTargetNetID = FriendlyTargetNetIDVal;
                            NewItem.AIFriendlyTargetLastKnownLocation = (FVector3f)AIT->LastKnownFriendlyLocation;
                        }
                        // Fill additional replicated fragments at creation time (subset)
                        if (const FMassCombatStatsFragment* CS = EM->GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
                        {
                            // Redundant fields synchronized locally
                        }
                        if (const FMassAgentCharacteristicsFragment* AC = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
                        {
                        }
                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            // AIS_StateTimer is now synchronized locally
                            NewItem.AIS_ProjectileFireCounter = AIS->ProjectileFireCounter;
                            NewItem.AIS_LastTargetNetID = AIS->LastTargetNetID;
                            NewItem.AIS_ProjectileTargetLocation = (FVector3f)AIS->LastProjectileTargetLocation;
                        }

                        // Fill RunAnimation fragment data
                        if (const FRunAnimationFragment* RAF = EM->GetFragmentDataPtr<FRunAnimationFragment>(EH))
                        {
                            NewItem.RunAnimation_Duration = RAF->Duration;
                            NewItem.RunAnimation_AnimationState = (uint8)RAF->AnimationState.GetValue();
                        }

                        // Fill Visual Effect Fragment
                        if (const FMassVisualEffectFragment* VE = EM->GetFragmentDataPtr<FMassVisualEffectFragment>(EH))
                        {
                            uint8 ActiveBits = 0;
                            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
                            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
                            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);
                            NewItem.VE_ActiveEffects = ActiveBits;
                            
                            // Pulsate data now handled locally
                        }

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
                    // Read CVAR thresholds once per bubble pass
                    const float LocThresh = FMath::Max(0.0f, CVarRTS_ServerRep_LocThresholdCm.GetValueOnGameThread());
                    const float AngleThresh = FMath::Clamp(CVarRTS_ServerRep_AngleThresholdDeg.GetValueOnGameThread(), 0.0f, 180.0f);
                    const float ScaleThresh = FMath::Max(0.0f, CVarRTS_ServerRep_ScaleThreshold.GetValueOnGameThread());
                    const float HealthThresh = FMath::Max(0.0f, CVarRTS_ServerRep_HealthThreshold.GetValueOnGameThread());
                    const float SightThresh = FMath::Max(0.0f, CVarRTS_ServerRep_SightRadiusThreshold.GetValueOnGameThread());

                    const FMassEntityHandle EH = Context.GetEntity(Idx);
                    const uint32 NewBits = EM ? BuildReplicatedTagBits(*EM, EH) : Item->TagBits;
                    const bool bIsDead = (NewBits & UnitTagBits::Dead) != 0;

                    bool bDirty = false;

                    // Skip transform replication for dead units as requested
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
                        if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
                    }
                    
                    // Update tag bits and AI target if EntityManager is available
                    if (EM)
                    {
                        if (Item->TagBits != NewBits) { Item->TagBits = NewBits; bDirty = true; }
                        if (UpdateReplicationBits(*Item, *EM, EH, BubbleInfo)) { bDirty = true; }
                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            uint8 NewFlags = 0u;
                            if (AIT->bHasValidTarget) NewFlags |= 1u;
                            if (AIT->IsFocusedOnTarget) NewFlags |= 2u;
                            uint32 NewTargetNetID = 0u;
                            if (AIT->TargetEntity.IsSet() && EM->IsEntityValid(AIT->TargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                                {
                                    NewTargetNetID = TgtNet->NetID.GetValue();
                                }
                            }
                            if (Item->AITargetFlags != NewFlags) { Item->AITargetFlags = NewFlags; bDirty = true; }
                            if (Item->AITargetNetID != NewTargetNetID) { Item->AITargetNetID = NewTargetNetID; bDirty = true; }
                            if (!Item->AITargetLastKnownLocation.Equals((FVector3f)AIT->LastKnownLocation, 10.0f)) { Item->AITargetLastKnownLocation = (FVector3f)AIT->LastKnownLocation; bDirty = true; }
                            
                            // Sync friendly target
                            uint32 NewFriendlyTargetNetID = 0u;
                            if (AIT->FriendlyTargetEntity.IsSet() && EM->IsEntityValid(AIT->FriendlyTargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                                {
                                    NewFriendlyTargetNetID = TgtNet->NetID.GetValue();
                                }
                            }
                            if (Item->AIFriendlyTargetNetID != NewFriendlyTargetNetID) { Item->AIFriendlyTargetNetID = NewFriendlyTargetNetID; bDirty = true; }
                            if (!Item->AIFriendlyTargetLastKnownLocation.Equals((FVector3f)AIT->LastKnownFriendlyLocation, 10.0f)) { Item->AIFriendlyTargetLastKnownLocation = (FVector3f)AIT->LastKnownFriendlyLocation; bDirty = true; }

                            if (!Item->AbilityTargetLocation.Equals((FVector3f)AIT->AbilityTargetLocation, 10.0f)) { Item->AbilityTargetLocation = (FVector3f)AIT->AbilityTargetLocation; bDirty = true; }

                            // Sync RunAnimation fragment data
                            if (const FRunAnimationFragment* RAF = EM->GetFragmentDataPtr<FRunAnimationFragment>(EH))
                            {
                                if (Item->RunAnimation_Duration != RAF->Duration) { Item->RunAnimation_Duration = RAF->Duration; bDirty = true; }
                                uint8 NewStateVal = (uint8)RAF->AnimationState.GetValue();
                                if (Item->RunAnimation_AnimationState != NewStateVal) { Item->RunAnimation_AnimationState = NewStateVal; bDirty = true; }
                            }
                        }
                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            bool bMoveDirty = false;
                            if (!Item->Move_Center.Equals((FVector3f)MT->Center, 10.0f)) { Item->Move_Center = (FVector3f)MT->Center; bMoveDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->Move_SlackRadius, MT->SlackRadius, 1.0f)) { Item->Move_SlackRadius = MT->SlackRadius; bMoveDirty = true; }
                            const float DesiredSpeed = MT->DesiredSpeed.Get();
                            if (!FMath::IsNearlyEqual(Item->Move_DesiredSpeed, DesiredSpeed, 10.0f)) { Item->Move_DesiredSpeed = DesiredSpeed; bMoveDirty = true; }
                            const uint8 Intent = static_cast<uint8>(MT->IntentAtGoal);
                            if (Item->Move_IntentAtGoal != Intent) { Item->Move_IntentAtGoal = Intent; bMoveDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->Move_DistanceToGoal, MT->DistanceToGoal, 50.0f)) { Item->Move_DistanceToGoal = MT->DistanceToGoal; bMoveDirty = true; }
                            // Versioning fields
                            const uint16 NewActionID = MT->GetCurrentActionID();
                            if (Item->Move_ActionID != NewActionID) { Item->Move_ActionID = NewActionID; bMoveDirty = true; }
                            const float NewSrvStart = (float)MT->GetCurrentActionServerStartTime();
                            if (!FMath::IsNearlyEqual(Item->Move_ServerStartTime, NewSrvStart, 0.1f)) { Item->Move_ServerStartTime = NewSrvStart; bMoveDirty = true; }
                            if (bMoveDirty) { bDirty = true; }
                        }

                        // Keep additional replicated fragments in sync (subset)
                        if (const FMassCombatStatsFragment* CS = EM->GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
                        {
                            // if (!FMath::IsNearlyEqual(Item->CS_Health, CS->Health, HealthThresh)) { Item->CS_Health = CS->Health; bDirty = true; }
                            // if (Item->CS_TeamId != CS->TeamId) { Item->CS_TeamId = CS->TeamId; bDirty = true; }
                            // if (!FMath::IsNearlyEqual(Item->CS_Shield, CS->Shield, HealthThresh)) { Item->CS_Shield = CS->Shield; bDirty = true; }
                        }

                        if (const FMassAgentCharacteristicsFragment* AC = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
                        {
                            // if (!FMath::IsNearlyEqual(Item->AC_FlyHeight, AC->FlyHeight, 0.01f)) { Item->AC_FlyHeight = AC->FlyHeight; bDirty = true; }
                        }

                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            // Only replicate StateTimer if NOT dead, or if it's the first time it's dead (transition)
                            // bool bIsDeadTransition = (bIsDead && Item->AIS_StateTimer > 0.001f && AIS->StateTimer <= 0.001f);
                            // if (!bIsDead || bIsDeadTransition)
                            // {
                            //     if (!FMath::IsNearlyEqual(Item->AIS_StateTimer, AIS->StateTimer, 0.001f)) 
                            //     { 
                            //         Item->AIS_StateTimer = AIS->StateTimer; 
                            //         bDirty = true; 
                            //     }
                            // }
                            if (Item->AIS_ProjectileFireCounter != AIS->ProjectileFireCounter) { Item->AIS_ProjectileFireCounter = AIS->ProjectileFireCounter; bDirty = true; }
                            
                            // Also ensure Projectile details and LastTargetNetID is current if we are firing
                            if (bDirty)
                            {
                                Item->AIS_LastTargetNetID = AIS->LastTargetNetID;
                                Item->AIS_ProjectileTargetLocation = (FVector3f)AIS->LastProjectileTargetLocation;
                            }
                        }

                        // Update Friendly Target info
                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            uint32 FriendlyTargetNetIDVal = 0u;
                            if (AIT->FriendlyTargetEntity.IsSet() && EM->IsEntityActive(AIT->FriendlyTargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->FriendlyTargetEntity))
                                {
                                    FriendlyTargetNetIDVal = TgtNet->NetID.GetValue();
                                }
                            }

                            if (Item->AIFriendlyTargetNetID != FriendlyTargetNetIDVal)
                            {
                                Item->AIFriendlyTargetNetID = FriendlyTargetNetIDVal;
                                bDirty = true;
                            }
                            if (!Item->AIFriendlyTargetLastKnownLocation.Equals((FVector3f)AIT->LastKnownFriendlyLocation, 1.0f))
                            {
                                Item->AIFriendlyTargetLastKnownLocation = (FVector3f)AIT->LastKnownFriendlyLocation;
                                bDirty = true;
                            }
                        }

                        // Update Worker Stats (BuildAreaPosition)
                        if (const FMassWorkerStatsFragment* WS = EM->GetFragmentDataPtr<FMassWorkerStatsFragment>(EH))
                        {
                            if (!Item->Worker_BuildAreaPosition.Equals((FVector3f)WS->BuildAreaPosition, 1.0f))
                            {
                                Item->Worker_BuildAreaPosition = (FVector3f)WS->BuildAreaPosition;
                                bDirty = true;
                            }
                        }

                        // Update Visual Effect Fragment
                        if (const FMassVisualEffectFragment* VE = EM->GetFragmentDataPtr<FMassVisualEffectFragment>(EH))
                        {
                            uint8 ActiveBits = 0;
                            if (VE->bPulsateEnabled) ActiveBits |= (1 << 0);
                            if (VE->bRotationEnabled) ActiveBits |= (1 << 1);
                            if (VE->bOscillationEnabled) ActiveBits |= (1 << 2);

                            if (Item->VE_ActiveEffects != ActiveBits) { Item->VE_ActiveEffects = ActiveBits; bDirty = true; }
                            
                            // Pulsate data now handled locally
                        }

                        // Update EffectArea Impact Fragment
                        if (const FEffectAreaImpactFragment* Impact = EM->GetFragmentDataPtr<FEffectAreaImpactFragment>(EH))
                        {
                            // EA data now handled locally
                        }

                        if (RepLogLevel() >= 2)
                        {
                            //UE_LOG(LogTemp, Log, TEXT("ServerReplicate (Upd): NetID=%u Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
                            //    NetID.GetValue(), Item->CS_Health, Item->CS_MaxHealth, Item->CS_RunSpeed, Item->CS_TeamId,
                            //    Item->AC_bIsFlying?1:0, Item->AC_bIsInvisible?1:0, Item->AC_FlyHeight,
                            //    Item->AIS_StateTimer, Item->AIS_CanAttack?1:0, Item->AIS_CanMove?1:0, Item->AIS_HoldPosition?1:0);
                        }
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