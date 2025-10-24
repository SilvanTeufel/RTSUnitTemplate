#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "MassEntitySubsystem.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassCommonFragments.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"

// Implementierung der Fast Array Item Callbacks
static FTransform BuildTransformFromItem(const FUnitReplicationItem& Item)
{
	const float Pitch = (static_cast<float>(Item.PitchQuantized) / 65535.0f) * 360.0f;
	const float Yaw = (static_cast<float>(Item.YawQuantized) / 65535.0f) * 360.0f;
	const float Roll = (static_cast<float>(Item.RollQuantized) / 65535.0f) * 360.0f;
	FTransform Xf;
	Xf.SetLocation(Item.Location);
	Xf.SetRotation(FQuat(FRotator(Pitch, Yaw, Roll)));
	Xf.SetScale3D(Item.Scale);
	return Xf;
}

void FUnitReplicationItem::PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		UnitReplicationCache::SetLatest(NetID, BuildTransformFromItem(*this));
	}
}

void FUnitReplicationItem::PostReplicatedChange(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		UnitReplicationCache::SetLatest(NetID, BuildTransformFromItem(*this));
	}
}

void FUnitReplicationItem::PreReplicatedRemove(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		UnitReplicationCache::Remove(NetID);
	}
}

AUnitClientBubbleInfo::AUnitClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	// Aktiviere Replikation für diesen Actor
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);

	// Setze Owner Pointer für Fast Array
	Agents.OwnerBubble = this;
}

void AUnitClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnitClientBubbleInfo, Agents);
}

void AUnitClientBubbleInfo::OnRep_Agents()
{
	Agents.OwnerBubble = this;
}

void AUnitClientBubbleInfo::BeginPlay()
{
	Super::BeginPlay();

	// Stelle sicher dass der Owner Pointer gesetzt ist
	Agents.OwnerBubble = this;
}

#if WITH_EDITORONLY_DATA
void AUnitClientBubbleInfo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
