// Fill out your copyright notice in the Description page of Project Settings.

#include "Mass/Replication/ClientReplicationProcessor.h"
#include "HAL/IConsoleManager.h"

// CVARs for ClientReplicationProcessor (client)
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_EnableCache(
    TEXT("net.RTS.ClientReplication.EnableCache"),
    1,
    TEXT("Enable shared OwnerName->Binding cache to avoid actor scans."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ClientReplication_CacheRebuildSeconds(
    TEXT("net.RTS.ClientReplication.CacheRebuildSeconds"),
    1.0f,
    TEXT("Seconds between cache rebuilds when enabled."),
    ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_BudgetPerTick(
    TEXT("net.RTS.ClientReplication.BudgetPerTick"),
    64,
    TEXT("Max reconcile/link/unlink operations per tick on client."),
    ECVF_Default);
// 0=Off, 1=Warn, 2=Verbose
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_LogLevel(
    TEXT("net.RTS.ClientReplication.LogLevel"),
    0,
    TEXT("Logging level: 0=Off, 1=Warn, 2=Verbose."),
    ECVF_Default);
// Debounce window to wait after last registry chunk before unlinking missing entities
static TAutoConsoleVariable<float> CVarRTS_ClientReplication_UnlinkDebounceSeconds(
    TEXT("net.RTS.ClientReplication.UnlinkDebounceSeconds"),
    0.10f,
    TEXT("Seconds to wait after the registry changes before executing unlink reconciliation."),
    ECVF_Default);

#include "MassCommonFragments.h"
#include "MassEntityTypes.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassActorSubsystem.h"
#include "MassReplicationFragments.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpawnerUnit.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameFramework/PlayerState.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

UClientReplicationProcessor::UClientReplicationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UClientReplicationProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
}

void UClientReplicationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FUnitReplicatedTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRotateToMouseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FRunAnimationFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassUnitYawFollowFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassUnitYawFollowTag>(EMassFragmentPresence::Optional);
	EntityQuery.RegisterWithProcessor(*this);

	MappingQuery.Initialize(EntityManager);
	MappingQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	MappingQuery.RegisterWithProcessor(*this);
}

void UClientReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (bSkipReplication) return;

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval) return;
	const float AccumulatedDelta = TimeSinceLastRun;
	TimeSinceLastRun = 0.f;
	
	UWorld* World = GetWorld();
	if (!World || !World->IsNetMode(NM_Client)) return;
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass) return;

	AExtendedControllerBase* LocalPC = Cast<AExtendedControllerBase>(World->GetFirstPlayerController());

	// 1) Gather existing client-side NetIDs
	TSet<uint32> ExistingIDs;
	EntityQuery.ForEachEntityChunk(Context, [&ExistingIDs](FMassExecutionContext& GatherCtx)
	{
		const int32 Num = GatherCtx.GetNumEntities();
		const TConstArrayView<FMassNetworkIDFragment> NetIDList = GatherCtx.GetFragmentView<FMassNetworkIDFragment>();
		for (int32 i = 0; i < Num; ++i) ExistingIDs.Add(NetIDList[i].NetID.GetValue());
	});

	// 2) Registry and Bubble mapping
	AUnitRegistryReplicator* RegistryActor = nullptr;
	TMap<int32, FMassNetworkID> AuthoritativeByUnitIndex;
	for (TActorIterator<AUnitRegistryReplicator> It(World); It; ++It)
	{
		RegistryActor = *It;
		for (const FUnitRegistryItem& Item : RegistryActor->Registry.Items)
		{
			if (Item.UnitIndex != INDEX_NONE) AuthoritativeByUnitIndex.Add(Item.UnitIndex, Item.NetID);
		}
		break;
	}

	TMap<uint32, FMassEntityHandle> GlobalNetToEntity;
	MappingQuery.ForEachEntityChunk(Context, [&GlobalNetToEntity](FMassExecutionContext& Ctx)
	{
		const int32 Num = Ctx.GetNumEntities();
		const TConstArrayView<FMassNetworkIDFragment> NetIDs = Ctx.GetFragmentView<FMassNetworkIDFragment>();
		for (int32 i = 0; i < Num; ++i)
		{
			const uint32 NID = NetIDs[i].NetID.GetValue();
			if (NID != 0) GlobalNetToEntity.Add(NID, Ctx.GetEntity(i));
		}
	});

	int32 Actions = 0;
	int32 MaxActionsPerTick = CVarRTS_ClientReplication_BudgetPerTick.GetValueOnGameThread();

	// 3) Hauptschleife: Replikation anwenden
	EntityQuery.ForEachEntityChunk(Context, [this, AuthoritativeByUnitIndex, &EntityManager, &GlobalNetToEntity, LocalPC, World, AccumulatedDelta, &Context](FMassExecutionContext& ChunkCtx)
	{
		static TMap<TWeakObjectPtr<AActor>, int32> ZeroIdStreak;
		const int32 NumEntities = ChunkCtx.GetNumEntities();

		TArrayView<FUnitReplicatedTransformFragment> ReplicatedTransformList = ChunkCtx.GetMutableFragmentView<FUnitReplicatedTransformFragment>();
		TArrayView<FTransformFragment> TransformList = ChunkCtx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorList = ChunkCtx.GetMutableFragmentView<FMassActorFragment>();
		TArrayView<FMassNetworkIDFragment> NetIDList = ChunkCtx.GetMutableFragmentView<FMassNetworkIDFragment>();
		TArrayView<FMassAITargetFragment> AITargetList = ChunkCtx.GetMutableFragmentView<FMassAITargetFragment>();
		TArrayView<FMassCombatStatsFragment> CombatList = ChunkCtx.GetMutableFragmentView<FMassCombatStatsFragment>();
		TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkCtx.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
		TArrayView<FMassAIStateFragment> AIStateList = ChunkCtx.GetMutableFragmentView<FMassAIStateFragment>();
		TArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkCtx.GetMutableFragmentView<FMassMoveTargetFragment>();
		TArrayView<FMassVisualEffectFragment> EffectList = ChunkCtx.GetMutableFragmentView<FMassVisualEffectFragment>();
		TArrayView<FMassWorkerStatsFragment> WorkerStatsList = ChunkCtx.GetMutableFragmentView<FMassWorkerStatsFragment>();
		TArrayView<FEffectAreaImpactFragment> EffectAreaImpactList = ChunkCtx.GetMutableFragmentView<FEffectAreaImpactFragment>();
		TArrayView<FRunAnimationFragment> RunAnimList = ChunkCtx.GetMutableFragmentView<FRunAnimationFragment>();
		TConstArrayView<FMassUnitYawFollowFragment> FollowList = ChunkCtx.GetFragmentView<FMassUnitYawFollowFragment>();
		TArrayView<FMassClientPredictionFragment> PredList = ChunkCtx.GetMutableFragmentView<FMassClientPredictionFragment>();

		TArray<bool> JustLinked;
		JustLinked.Init(false, NumEntities);

		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			// a) Authoritative NetID Sync
			if (AActor* OwnerActor = ActorList[EntityIdx].GetMutable())
			{
				if (AUnitBase* AsUnit = Cast<AUnitBase>(OwnerActor))
				{
					if (const FMassNetworkID* ByIdx = AuthoritativeByUnitIndex.Find(AsUnit->UnitIndex))
					{
						if (NetIDList[EntityIdx].NetID != *ByIdx)
						{
							JustLinked[EntityIdx] = (NetIDList[EntityIdx].NetID.GetValue() == 0);
							NetIDList[EntityIdx].NetID = *ByIdx;
						}
					}
				}
			}

			// b) Replikations-Daten aus der Bubble holen
			FTransform FinalXf = TransformList[EntityIdx].GetTransform();
			bool bFromBubble = false;

			if (URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>())
			{
				if (AUnitClientBubbleInfo* Bubble = CacheSub->GetBubble(false))
				{
					if (const FUnitReplicationItem* UseItem = Bubble->Agents.FindItemByNetID(NetIDList[EntityIdx].NetID))
					{
						const uint16 YQ = (uint16)(UseItem->PackedBits & 0xFFFF);
						const uint16 PE = (uint16)(UseItem->PackedBits >> 16);
						const float LYaw = (static_cast<float>(YQ) / 65535.0f) * 360.0f;

						FVector LocalScale = CharList.IsValidIndex(EntityIdx) ? CharList[EntityIdx].Scale : FVector::OneVector;
						FinalXf = FTransform(FQuat(FRotator(0.f, LYaw, 0.f)), FVector(UseItem->Location), LocalScale);
						bFromBubble = true;

						// Apply TagBits
						ApplyReplicatedTagBits(EntityManager, ChunkCtx.GetEntity(EntityIdx), UseItem->TagBits);

						// AI Target Slot 1
						if (AITargetList.IsValidIndex(EntityIdx))
						{
							FMassAITargetFragment& AIT = AITargetList[EntityIdx];
							AIT.bHasValidTarget = (PE & UnitReplicationBits::Packed_HasValidTarget) != 0;
							AIT.IsFocusedOnTarget = (PE & UnitReplicationBits::Packed_IsFocusedOnTarget) != 0;
							if (!(UseItem->TagBits & UnitReplicationBits::Slot_TargetIsMove))
							{
								AIT.LastKnownLocation = FVector(UseItem->TargetLoc);
							}

							if (UseItem->TargetID != 0)
							{
								if (const FMassEntityHandle* Found = GlobalNetToEntity.Find(UseItem->TargetID))
								{
									AIT.TargetEntity = *Found;
								}
								else
								{
									AIT.TargetEntity.Reset();
								}
							}
							else
							{
								AIT.TargetEntity.Reset();
							}
							
							// Action Slot 2
							if (UseItem->TagBits & UnitReplicationBits::Slot_ActionIsAbility) AIT.AbilityTargetLocation = FVector(UseItem->ActionLoc);
							else if (UseItem->TagBits & UnitReplicationBits::Slot_ActionIsFriendly)
							{
								AIT.LastKnownFriendlyLocation = FVector(UseItem->ActionLoc);
								if (UseItem->ActionID != 0)
								{
									if (const FMassEntityHandle* Found = GlobalNetToEntity.Find(UseItem->ActionID)) AIT.FriendlyTargetEntity = *Found;
								}
								else AIT.FriendlyTargetEntity.Reset();
							}
						}

						// AI State & Projectile
						if (AIStateList.IsValidIndex(EntityIdx))
						{
							FMassAIStateFragment& AIS = AIStateList[EntityIdx];
							AIS.CanAttack = (UseItem->ReplicationBits & UnitReplicationBits::AIS_CanAttack) != 0;
							AIS.CanMove = (UseItem->ReplicationBits & UnitReplicationBits::AIS_CanMove) != 0;
							if (UseItem->TagBits & UnitReplicationBits::Slot_ActionIsProjectile)
							{
								AIS.ProjectileFireCounter = (uint8)((UseItem->AuxData >> 16) & 0xFF);
								AIS.LastProjectileTargetLocation = FVector(UseItem->ActionLoc);
								AIS.LastTargetNetID = UseItem->ActionID;

								// Resolve Projectile Style from Registry
								uint8 StyleIdx = (uint8)((UseItem->ReplicationBits & UnitReplicationBits::AIS_StyleIndexMask) >> UnitReplicationBits::AIS_StyleIndexShift);
								if (StyleIdx > 0 && Bubble && Bubble->ProjectileStyleRegistry.IsValidIndex(StyleIdx - 1))
								{
									AIS.LastProjectileClass = Bubble->ProjectileStyleRegistry[StyleIdx - 1].ProjectileClass;
								}
								else
								{
									AIS.LastProjectileClass = nullptr; // Fallback to unit default
								}
							}
						}

						// Animation
						if (RunAnimList.IsValidIndex(EntityIdx))
						{
							RunAnimList[EntityIdx].Duration = (float)(UseItem->AuxData & 0xFFFF) / 100.f;
							RunAnimList[EntityIdx].AnimationState = static_cast<UnitData::EState>((PE & UnitReplicationBits::Packed_AnimStateMask) >> UnitReplicationBits::Packed_AnimStateShift);
						}

						// Visual Effects
						if (EffectList.IsValidIndex(EntityIdx))
						{
							uint16 Active = (PE & UnitReplicationBits::Packed_ActiveEffectsMask) >> UnitReplicationBits::Packed_ActiveEffectsShift;
							EffectList[EntityIdx].bPulsateEnabled = (Active & (1 << 0)) != 0;
							EffectList[EntityIdx].bRotationEnabled = (Active & (1 << 1)) != 0;
							EffectList[EntityIdx].bOscillationEnabled = (Active & (1 << 2)) != 0;
							EffectList[EntityIdx].bForceHidden = (UseItem->ReplicationBits & UnitReplicationBits::VE_bForceHidden) != 0;
						}

						// Move Target
						if (MoveTargetList.IsValidIndex(EntityIdx) && (UseItem->TagBits & UnitReplicationBits::Slot_TargetIsMove))
						{
							FMassMoveTargetFragment& MT = MoveTargetList[EntityIdx];
							MT.Center = FVector(UseItem->TargetLoc);
							MT.SlackRadius = (float)(UseItem->MoveData & 0xFF);
							MT.DesiredSpeed.Set((float)((UseItem->MoveData >> 8) & 0xFFF));
							MT.IntentAtGoal = static_cast<EMassMovementAction>((PE & UnitReplicationBits::Packed_MoveIntentMask) >> UnitReplicationBits::Packed_MoveIntentShift);
							MT.DistanceToGoal = (float)((UseItem->AuxData >> 24) & 0xFF) * 4.f;
							
							const uint32 ActionID = (UseItem->MoveData >> 20) & 0xFFF;
							if (AActor* OA = ActorList[EntityIdx].GetMutable())
							{
								MT.CreateReplicatedAction(MT.IntentAtGoal, ActionID, World->GetTimeSeconds(), (double)UseItem->Move_ServerStartTime);
							}
						}
					}
				}
			}

			// c) Transform anwenden (Snap oder Reconciliation)
			if (bFromBubble)
			{
				if (bUseFullReplication || JustLinked[EntityIdx])
				{
					TransformList[EntityIdx].GetMutableTransform() = FinalXf;
				}
				else if (!DoesEntityHaveTag(EntityManager, ChunkCtx.GetEntity(EntityIdx), FMassStateDeadTag::StaticStruct()))
				{
					// Gentle reconciliation (simplified for this task)
					FTransform& ClientXf = TransformList[EntityIdx].GetMutableTransform();
					ClientXf.SetLocation(FMath::VInterpTo(ClientXf.GetLocation(), FinalXf.GetLocation(), AccumulatedDelta, 5.0f));

					const float UnitRotationSpeed = (CombatList[EntityIdx].RotationSpeed > 0.f) ? 
													 CombatList[EntityIdx].RotationSpeed : 
													 CharList[EntityIdx].RotationSpeed;

					// Ein Faktor von 5.0f entsprach ca. 200-300 Grad/s in der Wahrnehmung.
					// Wir koppeln dies nun dynamisch:
					float DynamicInterpRate = FMath::Max(5.0f, UnitRotationSpeed / 40.0f);

					// NEU: Wenn die Einheit lokal zum Ziel rotiert, dämpfen wir die Korrektur durch die Replikation stark ab.
					// Dies verhindert das "Gegensteuern" der Replikation gegen die flüssige lokale Vorhersage.
					const bool bIsFollowTarget = FollowList.IsValidIndex(EntityIdx);
					if (bIsFollowTarget)
					{
						DynamicInterpRate *= 0.1f; // Replikations-Einfluss stark reduzieren
					}
					
					ClientXf.SetRotation(FQuat::Slerp(ClientXf.GetRotation(), FinalXf.GetRotation(), FMath::Min(1.0f, AccumulatedDelta * DynamicInterpRate)));
				}
			}

			// d) Self-Heal
			if (AActor* OA = ActorList[EntityIdx].GetMutable())
			{
				if (NetIDList[EntityIdx].NetID.GetValue() == 0)
				{
					int32& Streak = ZeroIdStreak.FindOrAdd(OA);
					if (++Streak >= 3)
					{
						if (UMassActorBindingComponent* Bind = OA->FindComponentByClass<UMassActorBindingComponent>()) Bind->RequestClientMassLink();
						Streak = 0;
					}
				}
				else ZeroIdStreak.Remove(OA);
			}
		}
	});
}
