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
    UWorld* World = &ReplicationContext.World;
    if (!World || World->GetNetMode() == NM_Client)
    {
        return; // Only the server registers entities for replication
    }

    AUnitClientBubbleInfo* BubbleInfo = GetOrSpawnBubble(*World);
    if (!BubbleInfo)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - No AUnitClientBubbleInfo available in world %s"), *World->GetName());
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - No MassEntitySubsystem available in world %s"), *World->GetName());
        return;
    }

    // Read fragments directly from the entity (safely)
    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
    const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity);
    const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
    if (!NetIDFrag || !TransformFrag)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::AddEntity - Entity missing required fragments (NetID or Transform). Skipping registration."));
        return;
    }

    const FMassNetworkID& NetID = NetIDFrag->NetID;
    const FTransform& Xf = TransformFrag->GetTransform();

    FUnitReplicationItem* Item = BubbleInfo->Agents.FindItemByNetID(NetID);
    if (!Item)
    {
        FUnitReplicationItem NewItem;
        NewItem.NetID = NetID;
        // Fill stable owner key if available
        if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
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
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::RemoveEntity - No AUnitClientBubbleInfo available in world %s"), *World->GetName());
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassUnitReplicatorBase::RemoveEntity - No MassEntitySubsystem available in world %s"), *World->GetName());
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

    if (BubbleInfo->Agents.RemoveItemByNetID(NetID))
    {
        BubbleInfo->Agents.MarkArrayDirty();
    }
    else
    {
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

        // Update every bubble we found so clients receive replicated items
        for (AUnitClientBubbleInfo* BubbleInfo : Bubbles)
        {
            bool bAnyDirty = false;
            for (int32 Idx = 0; Idx < NumEntities; ++Idx)
            {
                const FTransform& Xf = TransformList[Idx].GetTransform();
                const FVector Loc = Xf.GetLocation();
                const FRotator Rot = Xf.Rotator();
                const FVector Sca = Xf.GetScale3D();
                const FMassNetworkID& NetID = NetIDList[Idx].NetID;

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