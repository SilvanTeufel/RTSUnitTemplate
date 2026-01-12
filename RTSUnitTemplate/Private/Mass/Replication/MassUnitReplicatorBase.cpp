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
#include "Characters/Unit/UnitBase.h"

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

void UMassUnitReplicatorBase::AddRequirements(FMassEntityQuery& EntityQuery)
{
    // Keep requirements minimal to avoid excluding entities that lack optional fragments.
    // UMassReplicatorBase already requires FMassNetworkIDFragment; only add Transform here.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
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

    // 1) Ensure presence/update in replicated bubble array for clients
    FUnitReplicationItem* Item = BubbleInfo->Agents.FindItemByNetID(NetID);
    if (!Item)
    {
        FUnitReplicationItem NewItem;
        NewItem.NetID = NetID;
        // Fill stable owner key if available
        if (ActorFrag)
        {
            if (AActor* Ow = ActorFrag->GetMutable())
            {
                NewItem.OwnerName = Ow->GetFName();
            }
        }
        NewItem.Location = Xf.GetLocation();

        auto QuantizeAngle = [](float AngleDeg)->uint16
        {
            const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
            return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
        };
        const FRotator Rot = Xf.Rotator();
        NewItem.PitchQuantized = QuantizeAngle(Rot.Pitch);
        NewItem.YawQuantized = QuantizeAngle(Rot.Yaw);
        NewItem.RollQuantized = QuantizeAngle(Rot.Roll);
        NewItem.Scale = Xf.GetScale3D();
        NewItem.TagBits = BuildReplicatedTagBits(EntityManager, Entity);

        
            if (const FMassMoveTargetFragment* MT = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
            {
                NewItem.Move_bHasTarget = true;
                NewItem.Move_Center = MT->Center;
                NewItem.Move_SlackRadius = MT->SlackRadius;
                NewItem.Move_DesiredSpeed = MT->DesiredSpeed.Get();
                NewItem.Move_IntentAtGoal = static_cast<uint8>(MT->IntentAtGoal);
                NewItem.Move_DistanceToGoal = MT->DistanceToGoal;
                // Versioning fields to allow client to resolve newer vs older
                NewItem.Move_ActionID = MT->GetCurrentActionID();
                NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();
                NewItem.Move_CurrentAction = static_cast<uint8>(MT->GetCurrentAction());
            }
       
        // Fill AI target replication fields if available
        if (const FMassAITargetFragment* AIT = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity))
        {
            // Flags
            NewItem.AITargetFlags = 0u;
            if (AIT->bHasValidTarget) NewItem.AITargetFlags |= 1u;
            if (AIT->IsFocusedOnTarget) NewItem.AITargetFlags |= 2u;
            NewItem.AITargetLastKnownLocation = AIT->LastKnownLocation;
            NewItem.AbilityTargetLocation = AIT->AbilityTargetLocation;
            // Resolve target NetID if the target entity is valid
            uint32 TargetNetIDVal = 0u;
            if (AIT->TargetEntity.IsSet() && EntityManager.IsEntityValid(AIT->TargetEntity))
            {
                if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                {
                    TargetNetIDVal = TgtNet->NetID.GetValue();
                }
            }
            NewItem.AITargetNetID = TargetNetIDVal;
            // Build seen arrays (cap to 64 entries each to limit bandwidth)
            NewItem.AITargetPrevSeenIDs.Reset();
            NewItem.AITargetCurrSeenIDs.Reset();
            int32 CountPrev = 0;
            for (const FMassEntityHandle& H : AIT->PreviouslySeen)
            {
                if (EntityManager.IsEntityValid(H))
                {
                    if (const FMassNetworkIDFragment* SNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(H))
                    {
                        NewItem.AITargetPrevSeenIDs.Add(SNet->NetID.GetValue());
                        if (++CountPrev >= 64) break;
                    }
                }
            }
            int32 CountCurr = 0;
            for (const FMassEntityHandle& H : AIT->CurrentlySeen)
            {
                if (EntityManager.IsEntityValid(H))
                {
                    if (const FMassNetworkIDFragment* SNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(H))
                    {
                        NewItem.AITargetCurrSeenIDs.Add(SNet->NetID.GetValue());
                        if (++CountCurr >= 64) break;
                    }
                }
            }
        }
        // Fill additional fragments: CombatStats, AgentCharacteristics, AIState
        if (const FMassCombatStatsFragment* CS = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
        {
            NewItem.CS_Health = CS->Health;
            NewItem.CS_MaxHealth = CS->MaxHealth;
            NewItem.CS_RunSpeed = CS->RunSpeed;
            NewItem.CS_TeamId = CS->TeamId;
            NewItem.CS_AttackRange = CS->AttackRange;
            NewItem.CS_AttackDamage = CS->AttackDamage;
            NewItem.CS_AttackDuration = CS->AttackDuration;
            NewItem.CS_IsAttackedDuration = CS->IsAttackedDuration;
            NewItem.CS_CastTime = CS->CastTime;
            NewItem.CS_IsInitialized = CS->IsInitialized;
            NewItem.CS_RotationSpeed = CS->RotationSpeed;
            NewItem.CS_Armor = CS->Armor;
            NewItem.CS_MagicResistance = CS->MagicResistance;
            NewItem.CS_Shield = CS->Shield;
            NewItem.CS_MaxShield = CS->MaxShield;
            NewItem.CS_SightRadius = CS->SightRadius;
            NewItem.CS_LoseSightRadius = CS->LoseSightRadius;
            NewItem.CS_PauseDuration = CS->PauseDuration;
            NewItem.CS_bUseProjectile = CS->bUseProjectile;
        }
        if (const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity))
        {
            NewItem.AC_bIsFlying = AC->bIsFlying;
            NewItem.AC_bIsInvisible = AC->bIsInvisible;
            NewItem.AC_FlyHeight = AC->FlyHeight;
            NewItem.AC_bCanOnlyAttackFlying = AC->bCanOnlyAttackFlying;
            NewItem.AC_bCanOnlyAttackGround = AC->bCanOnlyAttackGround;
            NewItem.AC_bCanBeInvisible = AC->bCanBeInvisible;
            NewItem.AC_bCanDetectInvisible = AC->bCanDetectInvisible;
            NewItem.AC_LastGroundLocation = AC->LastGroundLocation;
            NewItem.AC_DespawnTime = AC->DespawnTime;
            NewItem.AC_RotatesToMovement = AC->RotatesToMovement;
            NewItem.AC_RotatesToEnemy = AC->RotatesToEnemy;
            NewItem.AC_RotationSpeed = AC->RotationSpeed;
            // Quantized pieces from AgentCharacteristics (REMOVED redundant PositionedTransform fields)
            NewItem.AC_CapsuleHeight = AC->CapsuleHeight;
            NewItem.AC_CapsuleRadius = AC->CapsuleRadius;
        }
        if (const FMassAIStateFragment* AIS = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
        {
            NewItem.AIS_StateTimer = AIS->StateTimer;
            NewItem.AIS_CanAttack = AIS->CanAttack;
            NewItem.AIS_CanMove = AIS->CanMove;
            NewItem.AIS_HoldPosition = AIS->HoldPosition;
            NewItem.AIS_HasAttacked = AIS->HasAttacked;
            NewItem.AIS_PlaceholderSignal = AIS->PlaceholderSignal;
            NewItem.AIS_StoredLocation = AIS->StoredLocation;
            NewItem.AIS_SwitchingState = AIS->SwitchingState;
            NewItem.AIS_BirthTime = AIS->BirthTime;
            NewItem.AIS_DeathTime = AIS->DeathTime;
            NewItem.AIS_IsInitialized = AIS->IsInitialized;
        }
        if (RepLogLevel() >= 2)
        {
            UE_LOG(LogTemp, Log, TEXT("ServerReplicate (Add): NetID=%u Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
                NetID.GetValue(), NewItem.CS_Health, NewItem.CS_MaxHealth, NewItem.CS_RunSpeed, NewItem.CS_TeamId,
                NewItem.AC_bIsFlying?1:0, NewItem.AC_bIsInvisible?1:0, NewItem.AC_FlyHeight,
                NewItem.AIS_StateTimer, NewItem.AIS_CanAttack?1:0, NewItem.AIS_CanMove?1:0, NewItem.AIS_HoldPosition?1:0);
        }

        const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
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
        const uint16 NewP = QuantizeAngleWithThreshold(Rot.Pitch);
        const uint16 NewY = QuantizeAngleWithThreshold(Rot.Yaw);
        const uint16 NewR = QuantizeAngleWithThreshold(Rot.Roll);
        if (Item->PitchQuantized != NewP) { Item->PitchQuantized = NewP; bDirty = true; }
        if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
        if (Item->RollQuantized != NewR) { Item->RollQuantized = NewR; bDirty = true; }
        const FVector NewScale = Xf.GetScale3D();
        if (!Item->Scale.Equals(NewScale, ScaleThresh)) { Item->Scale = NewScale; bDirty = true; }
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

        // Update every bubble we found so clients receive replicated items
        for (AUnitClientBubbleInfo* BubbleInfo : Bubbles)
        {
            bool bAnyDirty = false;
            for (int32 Idx = LoopStart; Idx < LoopEnd; ++Idx)
            {
                const FMassNetworkID& NetID = NetIDList[Idx].NetID;

                // We now replicate dead entities as well. Do not remove them from the bubble here.
                // Dead state will be carried in TagBits and consumers can react accordingly.

                const FTransform& Xf = TransformList[Idx].GetTransform();
                const FVector Loc = Xf.GetLocation();
                const FRotator Rot = Xf.Rotator();
                const FVector Sca = Xf.GetScale3D();

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
                    UE_LOG(LogTemp, Log, TEXT("ServerReplicate: NetID=%u Owner=%s UnitIndex=%d Loc=(%.1f,%.1f,%.1f) Rot=(P%.1f Y%.1f R%.1f) Scale=(%.2f,%.2f,%.2f)"),
                        NetID.GetValue(), *OwnerName.ToString(), OwnerUnitIndex,
                        Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll, Sca.X, Sca.Y, Sca.Z);
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
                    NewItem.PitchQuantized = QuantizeAngle(Rot.Pitch);
                    NewItem.YawQuantized = QuantizeAngle(Rot.Yaw);
                    NewItem.RollQuantized = QuantizeAngle(Rot.Roll);
                    NewItem.Scale = Sca;
                    if (EM)
                    {
                        const FMassEntityHandle EH = Context.GetEntity(Idx);
                        NewItem.TagBits = BuildReplicatedTagBits(*EM, EH);
                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            NewItem.Move_bHasTarget = true;
                            NewItem.Move_Center = MT->Center;
                            NewItem.Move_SlackRadius = MT->SlackRadius;
                            NewItem.Move_DesiredSpeed = MT->DesiredSpeed.Get();
                            NewItem.Move_IntentAtGoal = static_cast<uint8>(MT->IntentAtGoal);
                            NewItem.Move_DistanceToGoal = MT->DistanceToGoal;
                            // Versioning fields to allow client to resolve newer vs older
                            NewItem.Move_ActionID = MT->GetCurrentActionID();
                            NewItem.Move_ServerStartTime = (float)MT->GetCurrentActionServerStartTime();
                            NewItem.Move_CurrentAction = static_cast<uint8>(MT->GetCurrentAction());
                        }
                        
                        // Fill AI target fields if fragment exists
                        if (const FMassAITargetFragment* AIT = EM->GetFragmentDataPtr<FMassAITargetFragment>(EH))
                        {
                            NewItem.AITargetFlags = 0u;
                            if (AIT->bHasValidTarget) NewItem.AITargetFlags |= 1u;
                            if (AIT->IsFocusedOnTarget) NewItem.AITargetFlags |= 2u;
                            NewItem.AITargetLastKnownLocation = AIT->LastKnownLocation;
                            NewItem.AbilityTargetLocation = AIT->AbilityTargetLocation;
                            uint32 TargetNetIDVal = 0u;
                            if (AIT->TargetEntity.IsSet() && EM->IsEntityValid(AIT->TargetEntity))
                            {
                                if (const FMassNetworkIDFragment* TgtNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
                                {
                                    TargetNetIDVal = TgtNet->NetID.GetValue();
                                }
                            }
                            NewItem.AITargetNetID = TargetNetIDVal;
                        }
                        // Fill additional replicated fragments at creation time (full data)
                        if (const FMassCombatStatsFragment* CS = EM->GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
                        {
                            NewItem.CS_Health = CS->Health;
                            NewItem.CS_MaxHealth = CS->MaxHealth;
                            NewItem.CS_RunSpeed = CS->RunSpeed;
                            NewItem.CS_TeamId = CS->TeamId;
                            NewItem.CS_AttackRange = CS->AttackRange;
                            NewItem.CS_AttackDamage = CS->AttackDamage;
                            NewItem.CS_AttackDuration = CS->AttackDuration;
                            NewItem.CS_IsAttackedDuration = CS->IsAttackedDuration;
                            NewItem.CS_CastTime = CS->CastTime;
                            NewItem.CS_IsInitialized = CS->IsInitialized;
                            NewItem.CS_RotationSpeed = CS->RotationSpeed;
                            NewItem.CS_Armor = CS->Armor;
                            NewItem.CS_MagicResistance = CS->MagicResistance;
                            NewItem.CS_Shield = CS->Shield;
                            NewItem.CS_MaxShield = CS->MaxShield;
                            NewItem.CS_SightRadius = CS->SightRadius;
                            NewItem.CS_LoseSightRadius = CS->LoseSightRadius;
                            NewItem.CS_PauseDuration = CS->PauseDuration;
                            NewItem.CS_bUseProjectile = CS->bUseProjectile;
                        }
                        if (const FMassAgentCharacteristicsFragment* AC = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
                        {
                            NewItem.AC_bIsFlying = AC->bIsFlying;
                            NewItem.AC_bIsInvisible = AC->bIsInvisible;
                            NewItem.AC_FlyHeight = AC->FlyHeight;
                            NewItem.AC_bCanOnlyAttackFlying = AC->bCanOnlyAttackFlying;
                            NewItem.AC_bCanOnlyAttackGround = AC->bCanOnlyAttackGround;
                            NewItem.AC_bCanBeInvisible = AC->bCanBeInvisible;
                            NewItem.AC_bCanDetectInvisible = AC->bCanDetectInvisible;
                            NewItem.AC_LastGroundLocation = AC->LastGroundLocation;
                            NewItem.AC_DespawnTime = AC->DespawnTime;
                            NewItem.AC_RotatesToMovement = AC->RotatesToMovement;
                            NewItem.AC_RotatesToEnemy = AC->RotatesToEnemy;
                            NewItem.AC_RotationSpeed = AC->RotationSpeed;
                            // Quantized pieces from AgentCharacteristics (REMOVED redundant PositionedTransform fields)
                            NewItem.AC_CapsuleHeight = AC->CapsuleHeight;
                            NewItem.AC_CapsuleRadius = AC->CapsuleRadius;
                        }
                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            NewItem.AIS_StateTimer = AIS->StateTimer;
                            NewItem.AIS_CanAttack = AIS->CanAttack;
                            NewItem.AIS_CanMove = AIS->CanMove;
                            NewItem.AIS_HoldPosition = AIS->HoldPosition;
                            NewItem.AIS_HasAttacked = AIS->HasAttacked;
                            NewItem.AIS_PlaceholderSignal = AIS->PlaceholderSignal;
                            NewItem.AIS_StoredLocation = AIS->StoredLocation;
                            NewItem.AIS_SwitchingState = AIS->SwitchingState;
                            NewItem.AIS_BirthTime = AIS->BirthTime;
                            NewItem.AIS_DeathTime = AIS->DeathTime;
                            NewItem.AIS_IsInitialized = AIS->IsInitialized;
                        }
                    }
                    const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
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

                    bool bDirty = false;
                    if (!Item->Location.Equals(Loc, LocThresh)) { Item->Location = Loc; bDirty = true; }
                    auto QuantizeAngleWithThreshold = [AngleThresh](float AngleDeg)->uint16
                    {
                        const float ClampedStep = FMath::Max(0.1f, AngleThresh);
                        const float Snapped = FMath::RoundToFloat(AngleDeg / ClampedStep) * ClampedStep;
                        const float Norm = FMath::Fmod(Snapped + 360.0f, 360.0f);
                        return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
                    };
                    const uint16 NewP = QuantizeAngleWithThreshold(Rot.Pitch);
                    const uint16 NewY = QuantizeAngleWithThreshold(Rot.Yaw);
                    const uint16 NewR = QuantizeAngleWithThreshold(Rot.Roll);
                    if (Item->PitchQuantized != NewP) { Item->PitchQuantized = NewP; bDirty = true; }
                    if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
                    if (Item->RollQuantized != NewR) { Item->RollQuantized = NewR; bDirty = true; }
                    if (!Item->Scale.Equals(Sca, ScaleThresh)) { Item->Scale = Sca; bDirty = true; }
                    // Update tag bits and AI target if EntityManager is available
                    if (EM)
                    {
                        const FMassEntityHandle EH = Context.GetEntity(Idx);
                        const uint32 NewBits = BuildReplicatedTagBits(*EM, EH);
                        if (Item->TagBits != NewBits) { Item->TagBits = NewBits; bDirty = true; }
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
                            if (!Item->AITargetLastKnownLocation.Equals(AIT->LastKnownLocation, 10.0f)) { Item->AITargetLastKnownLocation = AIT->LastKnownLocation; bDirty = true; }
                            if (!Item->AbilityTargetLocation.Equals(AIT->AbilityTargetLocation, 10.0f)) { Item->AbilityTargetLocation = AIT->AbilityTargetLocation; bDirty = true; }
                            // Seen arrays (throttled/only if content changed and enabled)
                            if (CVarRTS_ServerRep_ReplicateSeenIDs.GetValueOnGameThread() != 0)
                            {
                                TArray<uint32> NewPrevIDs; NewPrevIDs.Reserve(32); // Reduced from 64 to save bandwidth
                                for (const FMassEntityHandle& H : AIT->PreviouslySeen)
                                {
                                    if (EM->IsEntityValid(H))
                                    {
                                        if (const FMassNetworkIDFragment* SNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(H))
                                        {
                                            NewPrevIDs.Add(SNet->NetID.GetValue()); if (NewPrevIDs.Num()>=32) break;
                                        }
                                    }
                                }
                                TArray<uint32> NewCurrIDs; NewCurrIDs.Reserve(32); // Reduced from 64 to save bandwidth
                                for (const FMassEntityHandle& H : AIT->CurrentlySeen)
                                {
                                    if (EM->IsEntityValid(H))
                                    {
                                        if (const FMassNetworkIDFragment* SNet = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(H))
                                        {
                                            NewCurrIDs.Add(SNet->NetID.GetValue()); if (NewCurrIDs.Num()>=32) break;
                                        }
                                    }
                                }
                                if (Item->AITargetPrevSeenIDs != NewPrevIDs) { Item->AITargetPrevSeenIDs = MoveTemp(NewPrevIDs); bDirty = true; }
                                if (Item->AITargetCurrSeenIDs != NewCurrIDs) { Item->AITargetCurrSeenIDs = MoveTemp(NewCurrIDs); bDirty = true; }
                            }
                            else
                            {
                                if (Item->AITargetPrevSeenIDs.Num() > 0) { Item->AITargetPrevSeenIDs.Reset(); bDirty = true; }
                                if (Item->AITargetCurrSeenIDs.Num() > 0) { Item->AITargetCurrSeenIDs.Reset(); bDirty = true; }
                            }
                        }
                        if (const FMassMoveTargetFragment* MT = EM->GetFragmentDataPtr<FMassMoveTargetFragment>(EH))
                        {
                            bool bMoveDirty = false;
                            if (Item->Move_bHasTarget != true) { Item->Move_bHasTarget = true; bMoveDirty = true; }
                            if (!Item->Move_Center.Equals(MT->Center, 10.0f)) { Item->Move_Center = MT->Center; bMoveDirty = true; }
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
                            const uint8 NewCurrAction = static_cast<uint8>(MT->GetCurrentAction());
                            if (Item->Move_CurrentAction != NewCurrAction) { Item->Move_CurrentAction = NewCurrAction; bMoveDirty = true; }
                            if (bMoveDirty) { bDirty = true; }
                        }

                        // Keep additional replicated fragments in sync
                        if (const FMassCombatStatsFragment* CS = EM->GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
                        {
                            if (!FMath::IsNearlyEqual(Item->CS_Health, CS->Health, HealthThresh)) { Item->CS_Health = CS->Health; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_MaxHealth, CS->MaxHealth, HealthThresh)) { Item->CS_MaxHealth = CS->MaxHealth; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_RunSpeed, CS->RunSpeed, 10.0f)) { Item->CS_RunSpeed = CS->RunSpeed; bDirty = true; }
                            if (Item->CS_TeamId != CS->TeamId) { Item->CS_TeamId = CS->TeamId; bDirty = true; }
                            // Extended combat stats
                            if (!FMath::IsNearlyEqual(Item->CS_AttackRange, CS->AttackRange, 10.0f)) { Item->CS_AttackRange = CS->AttackRange; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_AttackDamage, CS->AttackDamage, 1.0f)) { Item->CS_AttackDamage = CS->AttackDamage; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_AttackDuration, CS->AttackDuration, 0.1f)) { Item->CS_AttackDuration = CS->AttackDuration; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_IsAttackedDuration, CS->IsAttackedDuration, 0.5f)) { Item->CS_IsAttackedDuration = CS->IsAttackedDuration; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_CastTime, CS->CastTime, 0.1f)) { Item->CS_CastTime = CS->CastTime; bDirty = true; }
                            if (Item->CS_IsInitialized != CS->IsInitialized) { Item->CS_IsInitialized = CS->IsInitialized; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_RotationSpeed, CS->RotationSpeed, 1.0f)) { Item->CS_RotationSpeed = CS->RotationSpeed; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_Armor, CS->Armor, 1.0f)) { Item->CS_Armor = CS->Armor; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_MagicResistance, CS->MagicResistance, 1.0f)) { Item->CS_MagicResistance = CS->MagicResistance; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_Shield, CS->Shield, HealthThresh)) { Item->CS_Shield = CS->Shield; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_MaxShield, CS->MaxShield, HealthThresh)) { Item->CS_MaxShield = CS->MaxShield; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_SightRadius, CS->SightRadius, SightThresh)) { Item->CS_SightRadius = CS->SightRadius; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_LoseSightRadius, CS->LoseSightRadius, SightThresh)) { Item->CS_LoseSightRadius = CS->LoseSightRadius; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->CS_PauseDuration, CS->PauseDuration, 0.5f)) { Item->CS_PauseDuration = CS->PauseDuration; bDirty = true; }
                            if (Item->CS_bUseProjectile != CS->bUseProjectile) { Item->CS_bUseProjectile = CS->bUseProjectile; bDirty = true; }
                        }
                        if (const FMassAgentCharacteristicsFragment* AC = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
                        {
                            const bool NewFlying = AC->bIsFlying;
                            const bool NewInvis = AC->bIsInvisible;
                            const float NewFlyH = AC->FlyHeight;
                            if (Item->AC_bIsFlying != NewFlying) { Item->AC_bIsFlying = NewFlying; bDirty = true; }
                            if (Item->AC_bIsInvisible != NewInvis) { Item->AC_bIsInvisible = NewInvis; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_FlyHeight, NewFlyH, 0.01f)) { Item->AC_FlyHeight = NewFlyH; bDirty = true; }
                            // Extended fields
                            if (Item->AC_bCanOnlyAttackFlying != AC->bCanOnlyAttackFlying) { Item->AC_bCanOnlyAttackFlying = AC->bCanOnlyAttackFlying; bDirty = true; }
                            if (Item->AC_bCanOnlyAttackGround != AC->bCanOnlyAttackGround) { Item->AC_bCanOnlyAttackGround = AC->bCanOnlyAttackGround; bDirty = true; }
                            if (Item->AC_bCanBeInvisible != AC->bCanBeInvisible) { Item->AC_bCanBeInvisible = AC->bCanBeInvisible; bDirty = true; }
                            if (Item->AC_bCanDetectInvisible != AC->bCanDetectInvisible) { Item->AC_bCanDetectInvisible = AC->bCanDetectInvisible; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_LastGroundLocation, AC->LastGroundLocation, 0.01f)) { Item->AC_LastGroundLocation = AC->LastGroundLocation; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_DespawnTime, AC->DespawnTime, 0.001f)) { Item->AC_DespawnTime = AC->DespawnTime; bDirty = true; }
                            if (Item->AC_RotatesToMovement != AC->RotatesToMovement) { Item->AC_RotatesToMovement = AC->RotatesToMovement; bDirty = true; }
                            if (Item->AC_RotatesToEnemy != AC->RotatesToEnemy) { Item->AC_RotatesToEnemy = AC->RotatesToEnemy; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_RotationSpeed, AC->RotationSpeed, 0.01f)) { Item->AC_RotationSpeed = AC->RotationSpeed; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_CapsuleHeight, AC->CapsuleHeight, 0.01f)) { Item->AC_CapsuleHeight = AC->CapsuleHeight; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AC_CapsuleRadius, AC->CapsuleRadius, 0.01f)) { Item->AC_CapsuleRadius = AC->CapsuleRadius; bDirty = true; }
                        }
                        if (const FMassAIStateFragment* AIS = EM->GetFragmentDataPtr<FMassAIStateFragment>(EH))
                        {
                            if (!FMath::IsNearlyEqual(Item->AIS_StateTimer, AIS->StateTimer, 0.001f)) { Item->AIS_StateTimer = AIS->StateTimer; bDirty = true; }
                            if (Item->AIS_CanAttack != AIS->CanAttack) { Item->AIS_CanAttack = AIS->CanAttack; bDirty = true; }
                            if (Item->AIS_CanMove != AIS->CanMove) { Item->AIS_CanMove = AIS->CanMove; bDirty = true; }
                            if (Item->AIS_HoldPosition != AIS->HoldPosition) { Item->AIS_HoldPosition = AIS->HoldPosition; bDirty = true; }
                            if (Item->AIS_HasAttacked != AIS->HasAttacked) { Item->AIS_HasAttacked = AIS->HasAttacked; bDirty = true; }
                            if (Item->AIS_PlaceholderSignal != AIS->PlaceholderSignal) { Item->AIS_PlaceholderSignal = AIS->PlaceholderSignal; bDirty = true; }
                            if (!Item->AIS_StoredLocation.Equals(AIS->StoredLocation, 0.1f)) { Item->AIS_StoredLocation = AIS->StoredLocation; bDirty = true; }
                            if (Item->AIS_SwitchingState != AIS->SwitchingState) { Item->AIS_SwitchingState = AIS->SwitchingState; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AIS_BirthTime, AIS->BirthTime, 0.001f)) { Item->AIS_BirthTime = AIS->BirthTime; bDirty = true; }
                            if (!FMath::IsNearlyEqual(Item->AIS_DeathTime, AIS->DeathTime, 0.001f)) { Item->AIS_DeathTime = AIS->DeathTime; bDirty = true; }
                            if (Item->AIS_IsInitialized != AIS->IsInitialized) { Item->AIS_IsInitialized = AIS->IsInitialized; bDirty = true; }
                        }
                        if (RepLogLevel() >= 2)
                        {
                            UE_LOG(LogTemp, Log, TEXT("ServerReplicate (Upd): NetID=%u Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
                                NetID.GetValue(), Item->CS_Health, Item->CS_MaxHealth, Item->CS_RunSpeed, Item->CS_TeamId,
                                Item->AC_bIsFlying?1:0, Item->AC_bIsInvisible?1:0, Item->AC_FlyHeight,
                                Item->AIS_StateTimer, Item->AIS_CanAttack?1:0, Item->AIS_CanMove?1:0, Item->AIS_HoldPosition?1:0);
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