// Fill out your copyright notice in the Description page of Project Settings.


#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
#include "Mass/Replication/MassUnitReplicatorBase.h"

#include "MassCommonFragments.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassReplicationFragments.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
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

namespace { inline int32 RepLogLevel(){ return CVarRTS_ServerReplicator_LogLevel.GetValueOnGameThread(); } }

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
			Bubble->SetNetUpdateFrequency(10.0f);
			Bubble->ForceNetUpdate();
		}
	}
	return Bubble;
}

void UMassUnitReplicatorBase::AddRequirements(FMassEntityQuery& EntityQuery)
{
    // Declare unique requirements for this replicator query.
    // Note: UMassReplicatorBase already adds FMassNetworkIDFragment by default.
    // Adding it again causes a duplicate-requirement assertion in UE 5.6.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    // Do NOT add FMassNetworkIDFragment here to avoid duplicates.
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

        const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
        BubbleInfo->Agents.MarkItemDirty(BubbleInfo->Agents.Items[NewIdx]);
        BubbleInfo->Agents.MarkArrayDirty();
        BubbleInfo->ForceNetUpdate();
    }
    else
    {
        // Update initial values just in case and mark dirty
        bool bDirty = false;
        if (!Item->Location.Equals(Xf.GetLocation(), 0.01f)) { Item->Location = Xf.GetLocation(); bDirty = true; }
        auto QuantizeAngle = [](float AngleDeg)->uint16
        {
            const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
            return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
        };
        const FRotator Rot = Xf.Rotator();
        const uint16 NewP = QuantizeAngle(Rot.Pitch);
        const uint16 NewY = QuantizeAngle(Rot.Yaw);
        const uint16 NewR = QuantizeAngle(Rot.Roll);
        if (Item->PitchQuantized != NewP) { Item->PitchQuantized = NewP; bDirty = true; }
        if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
        if (Item->RollQuantized != NewR) { Item->RollQuantized = NewR; bDirty = true; }
        if (!Item->Scale.Equals(Xf.GetScale3D(), 0.01f)) { Item->Scale = Xf.GetScale3D(); bDirty = true; }
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
            for (int32 Idx = 0; Idx < NumEntities; ++Idx)
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
                    // OwnerName left unset here; can be filled elsewhere if needed.
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
                    }
                    const int32 NewIdx = BubbleInfo->Agents.Items.Add(NewItem);
                    BubbleInfo->Agents.MarkItemDirty(BubbleInfo->Agents.Items[NewIdx]);
                    bAnyDirty = true;
                }
                else
                {
                    bool bDirty = false;
                    if (!Item->Location.Equals(Loc, 0.1f)) { Item->Location = Loc; bDirty = true; }
                    auto QuantizeAngle = [](float AngleDeg)->uint16
                    {
                        const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
                        return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
                    };
                    const uint16 NewP = QuantizeAngle(Rot.Pitch);
                    const uint16 NewY = QuantizeAngle(Rot.Yaw);
                    const uint16 NewR = QuantizeAngle(Rot.Roll);
                    if (Item->PitchQuantized != NewP) { Item->PitchQuantized = NewP; bDirty = true; }
                    if (Item->YawQuantized != NewY) { Item->YawQuantized = NewY; bDirty = true; }
                    if (Item->RollQuantized != NewR) { Item->RollQuantized = NewR; bDirty = true; }
                    if (!Item->Scale.Equals(Sca, 0.01f)) { Item->Scale = Sca; bDirty = true; }
                    // Update tag bits too if EntityManager is available
                    if (EM)
                    {
                        const FMassEntityHandle EH = Context.GetEntity(Idx);
                        const uint32 NewBits = BuildReplicatedTagBits(*EM, EH);
                        if (Item->TagBits != NewBits) { Item->TagBits = NewBits; bDirty = true; }
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