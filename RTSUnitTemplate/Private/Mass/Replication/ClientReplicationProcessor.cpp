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
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
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
	TMap<FName, FMassNetworkID> AuthoritativeByOwnerName;
	for (TActorIterator<AUnitRegistryReplicator> It(World); It; ++It)
	{
		RegistryActor = *It;
		for (const FUnitRegistryItem& Item : RegistryActor->Registry.Items)
		{
			if (Item.UnitIndex != INDEX_NONE) AuthoritativeByUnitIndex.Add(Item.UnitIndex, Item.NetID);
			if (Item.OwnerName != NAME_None) AuthoritativeByOwnerName.Add(Item.OwnerName, Item.NetID);
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
	EntityQuery.ForEachEntityChunk(Context, [this, AuthoritativeByUnitIndex, AuthoritativeByOwnerName, &EntityManager, &GlobalNetToEntity, LocalPC, World, AccumulatedDelta, &Context](FMassExecutionContext& ChunkCtx)
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
		TArrayView<FMassVelocityFragment> VelocityList = ChunkCtx.GetMutableFragmentView<FMassVelocityFragment>();
		TArrayView<FMassForceFragment> ForceList = ChunkCtx.GetMutableFragmentView<FMassForceFragment>();
		TArrayView<FMassSteeringFragment> SteeringList = ChunkCtx.GetMutableFragmentView<FMassSteeringFragment>();
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
				const FMassNetworkID* FoundID = nullptr;
				if (AUnitBase* AsUnit = Cast<AUnitBase>(OwnerActor))
				{
					FoundID = AuthoritativeByUnitIndex.Find(AsUnit->UnitIndex);
				}
				else if (AEffectArea* AsArea = Cast<AEffectArea>(OwnerActor))
				{
					FoundID = AuthoritativeByUnitIndex.Find(AsArea->AreaIndex);
				}

				if (!FoundID) // Fallback for Aktoren ohne Index
				{
					FoundID = AuthoritativeByOwnerName.Find(OwnerActor->GetFName());
				}

				if (FoundID && NetIDList[EntityIdx].NetID != *FoundID)
				{
					JustLinked[EntityIdx] = (NetIDList[EntityIdx].NetID.GetValue() == 0);
					NetIDList[EntityIdx].NetID = *FoundID;

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
							
							// Synchronize Target Entity if server has one
							// Note: We avoid resetting to zero here to prevent "flapping" with client-side detection.
							if (UseItem->TargetID != 0)
							{
								if (const FMassEntityHandle* Found = GlobalNetToEntity.Find(UseItem->TargetID))
								{
									if (AIT.TargetEntity != *Found)
									{
										AIT.TargetEntity = *Found;
										AIT.bHasValidTarget = true;
									}
								}
							}

							// Synchronize LastKnownLocation if provided and not a movement target
							if (!(UseItem->TagBits & UnitReplicationBits::Slot_TargetIsMove))
							{
								if (!UseItem->TargetLoc.IsNearlyZero())
								{
									AIT.LastKnownLocation = FVector(UseItem->TargetLoc);
									if (!AIT.TargetEntity.IsSet()) AIT.bHasValidTarget = true;
								}
							}

							// Synchronize other flags from packed bits
							AIT.IsFocusedOnTarget = (PE & UnitReplicationBits::Packed_IsFocusedOnTarget) != 0;
							if ((PE & UnitReplicationBits::Packed_HasValidTarget) != 0) AIT.bHasValidTarget = true;
							
							// Action Slot 2 (Abilities and Friendly Targets)
							if (UseItem->TagBits & UnitReplicationBits::Slot_ActionIsAbility)
							{
								AIT.AbilityTargetLocation = FVector(UseItem->ActionLoc);
							}
							else if (UseItem->TagBits & UnitReplicationBits::Slot_ActionIsFriendly)
							{
								AIT.LastKnownFriendlyLocation = FVector(UseItem->ActionLoc);
								if (UseItem->ActionID != 0)
								{
									if (const FMassEntityHandle* Found = GlobalNetToEntity.Find(UseItem->ActionID)) AIT.FriendlyTargetEntity = *Found;
								}
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

						// Effect Area Sync
						if (EffectAreaImpactList.IsValidIndex(EntityIdx))
						{
							FEffectAreaImpactFragment& Impact = EffectAreaImpactList[EntityIdx];
							bool bWasScaling = Impact.bIsScalingAfterImpact;
							Impact.bIsScalingAfterImpact = (UseItem->ReplicationBits & UnitReplicationBits::EA_bIsScalingAfterImpact) != 0;
							Impact.bImpactScaleTriggered = (UseItem->ReplicationBits & UnitReplicationBits::EA_bImpactScaleTriggered) != 0;
							Impact.bPendingDestruction = (UseItem->ReplicationBits & UnitReplicationBits::EA_bPendingDestruction) != 0;
							Impact.bImpactVFXTriggered = (UseItem->ReplicationBits & UnitReplicationBits::EA_bImpactVFXTriggered) != 0;

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
						}

						// Move Target
						if (MoveTargetList.IsValidIndex(EntityIdx))
						{
							FMassMoveTargetFragment& MT = MoveTargetList[EntityIdx];
							if (UseItem->TagBits & UnitReplicationBits::Slot_TargetIsMove)
							{
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
							else
							{
								// NEU: Wenn der Server keine Bewegung mehr signalisiert, muss der Client lokal stoppen.
								// Dies verhindert, dass der lokale MovementProcessor die Einheit gegen die Replikation schiebt.
								if (MT.DesiredSpeed.Get() > 0.f)
								{
									MT.DesiredSpeed.Set(0.f);
									MT.IntentAtGoal = EMassMovementAction::Stand;
								}
							}
						}
					}
				}
			}

			// c) Transform anwenden (Snap oder Reconciliation)
			if (bFromBubble)
			{
				FTransform& ClientXf = TransformList[EntityIdx].GetMutableTransform();
				const FVector CurrentLocation = ClientXf.GetLocation();
				const FVector TargetLocation = FinalXf.GetLocation();
				const float DistanceSq = FVector::DistSquared2D(CurrentLocation, TargetLocation);

				if (bUseFullReplication || JustLinked[EntityIdx] || DistanceSq > FMath::Square(FullReplicationDistance))
				{
					ClientXf = FinalXf;
				}
				else if (!DoesEntityHaveTag(EntityManager, ChunkCtx.GetEntity(EntityIdx), FMassStateDeadTag::StaticStruct()))
				{
					// --- Location Reconciliation ---
					
					// 1. Prüfen, ob die Einheit logisch stationär sein MUSS (Angriff/Pause ohne Bewegung)
					const bool bHasAttackTag = DoesEntityHaveTag(EntityManager, ChunkCtx.GetEntity(EntityIdx), FMassStateAttackTag::StaticStruct());
					const bool bHasPauseTag = DoesEntityHaveTag(EntityManager, ChunkCtx.GetEntity(EntityIdx), FMassStatePauseTag::StaticStruct());
					const bool bIsStationaryAttack = (bHasAttackTag || bHasPauseTag) && CombatList.IsValidIndex(EntityIdx) && !CombatList[EntityIdx].bCanMoveWhileAttacking;

					float CurrentKp = bIsStationaryAttack ? 1.0f : Kp;
					float CurrentMinErrorSq = bIsStationaryAttack ? 1.0f : MinErrorForCorrectionSq; // Nur 1cm Toleranz im Stand

					const bool bIsMoving = (MoveTargetList.IsValidIndex(EntityIdx) && MoveTargetList[EntityIdx].DesiredSpeed.Get() > 10.f) || 
										   (PredList.IsValidIndex(EntityIdx) && PredList[EntityIdx].bHasData);
					
					//if (bIsMoving)
					{
						// Increase tolerance and decrease correction speed while moving to prevent stuttering
						CurrentMinErrorSq *= 2.0f; // Toleranz während der Fahrt: 200 (ca. 14 cm) statt bisher 100 (10 cm)
						CurrentKp *= 0.4f;

						// If the server is pulling us backwards, be even more gentle
						const FVector Forward = ClientXf.GetRotation().GetForwardVector();
						const FVector ErrorDir = (TargetLocation - CurrentLocation).GetSafeNormal();
						if (FVector::DotProduct(Forward, ErrorDir) < -0.2f)
						{
							CurrentKp *= 0.5f;

							// NEU: Wenn die Replikation uns entgegen der Laufrichtung zieht, 
							// drosseln wir die lokale Geschwindigkeit leicht (um ca. 20%).
							if (VelocityList.IsValidIndex(EntityIdx))
							{
								VelocityList[EntityIdx].Value *= 0.8f;
							}
						}
					}
					//else
					//{
						// NEU: Auch im Stillstand eine leichte Dämpfung beibehalten, um Jitter zu vermeiden.
						//CurrentMinErrorSq *= 1.25f;
						//CurrentKp *= 0.75f;
					//}

					const float ErrorRatio = DistanceSq / CurrentMinErrorSq;
					if (ErrorRatio > 1.0f)
					{
						// NEU: Soft-Threshold Gewichtung (Fade-in von 0.0 auf 1.0)
						const float SoftWeight = FMath::Clamp(ErrorRatio - 1.0f, 0.0f, 1.0f);
						const float FinalKp = CurrentKp * SoftWeight;

						// Proportional gain Kp used as interpolation speed for smooth correction
						FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, AccumulatedDelta, FinalKp);

						if (CharList.IsValidIndex(EntityIdx) && !CharList[EntityIdx].bIsFlying)
						{
							// Z-Korrektur deaktivieren, da der lokale ActorTransformSyncProcessor (Ground-Trace) Vorrang hat
							NewLocation.Z = CurrentLocation.Z; 
						}

						// Use MaxCorrectionAccel as a velocity limit (Speed Cap) for the reconciliation
						const FVector DeltaMove = NewLocation - CurrentLocation;
						const float MaxMove = MaxCorrectionAccel * AccumulatedDelta;
						if (DeltaMove.SizeSquared() > FMath::Square(MaxMove))
						{
							NewLocation = CurrentLocation + (DeltaMove.GetSafeNormal() * MaxMove);
						}

						ClientXf.SetLocation(NewLocation);

						// NEU: Verhindert, dass die lokale Physik die Korrektur sofort wieder aushebelt
						// KORREKTUR: Nur dämpfen, wenn die Einheit laut Replikation NICHT in Bewegung ist oder stationär angreift.
						if (!bIsMoving || bIsStationaryAttack)
						{
							if (VelocityList.IsValidIndex(EntityIdx)) VelocityList[EntityIdx].Value *= 0.05f; // Härtere Bremse
							if (ForceList.IsValidIndex(EntityIdx)) ForceList[EntityIdx].Value = FVector::ZeroVector;
							
							// NEU: Auch die Lenkung (Avoidance/Movement) sofort unterbinden
							if (SteeringList.IsValidIndex(EntityIdx)) SteeringList[EntityIdx].DesiredVelocity = FVector::ZeroVector;
						}
					}

					// --- Rotation Reconciliation ---
					float FinalKpRot = bIsStationaryAttack ? 1.0f : KpRot;
					
					// NEU: Wenn die Einheit lokal zum Ziel rotiert, dämpfen wir die Korrektur durch die Replikation stark ab.
					// Dies verhindert das "Gegensteuern" der Replikation gegen die flüssige lokale Vorhersage.
					const bool bIsFollowTarget = FollowList.IsValidIndex(EntityIdx);
					const bool bHasAITarget = AITargetList.IsValidIndex(EntityIdx) && AITargetList[EntityIdx].bHasValidTarget;

					if (bIsFollowTarget || bHasAITarget)
					{
						FinalKpRot *= 0.1f; // Replikations-Einfluss stark reduzieren
					}

					if (bRotationYawOnly)
					{
						FRotator CurrentRotator = ClientXf.GetRotation().Rotator();
						FRotator TargetRotator = FinalXf.GetRotation().Rotator();
						float YawError = FRotator::NormalizeAxis(TargetRotator.Yaw - CurrentRotator.Yaw);

 					const float AbsYawError = FMath::Abs(YawError);
 					if (AbsYawError > (bIsStationaryAttack ? 0.1f : MinYawErrorForCorrectionDeg))
 					{
 						// NEU: Auch hier fadet die Korrekturstärke sanft ein (außer bei stationärem Angriff für sofortiges Snapping)
 						const float RotSoftWeight = bIsStationaryAttack ? 1.0f : FMath::Clamp((AbsYawError - MinYawErrorForCorrectionDeg) / MinYawErrorForCorrectionDeg, 0.0f, 1.0f);
 						const float FinalKpRotSoft = FinalKpRot * RotSoftWeight;

							// Apply proportional correction limited by MaxRotationCorrectionDegPerSec
							const float MaxStep = MaxRotationCorrectionDegPerSec * AccumulatedDelta;
							const float DesiredStep = YawError * FinalKpRotSoft;
							const float Step = FMath::Clamp(DesiredStep, -MaxStep, MaxStep);
							
							CurrentRotator.Yaw = FRotator::NormalizeAxis(CurrentRotator.Yaw + Step);
							ClientXf.SetRotation(CurrentRotator.Quaternion());
						}
					}
					else
					{
						// Full Rotation Slerp
						const FQuat CurrentRot = ClientXf.GetRotation();
						const FQuat TargetRot = FinalXf.GetRotation();
						
						// Angle distance for threshold check
						const float AngleRad = CurrentRot.AngularDistance(TargetRot);
						if (FMath::RadiansToDegrees(AngleRad) > MinYawErrorForCorrectionDeg)
						{
							// We use FinalKpRot * 10.0f to keep it in a similar range as the original logic 
							// where InterpRate was around 5-10.
							ClientXf.SetRotation(FQuat::Slerp(CurrentRot, TargetRot, FMath::Min(1.0f, AccumulatedDelta * FinalKpRot * 10.f)));
						}
					}
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
