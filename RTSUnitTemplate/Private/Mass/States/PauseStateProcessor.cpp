// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/PauseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Controller\PlayerController\CustomControllerBase.h"
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassReplicationFragments.h"

UPauseStateProcessor::UPauseStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UPauseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position für Distanzcheck
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPauseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    UWorld* World = EntityManager.GetWorld();
    if (!World || !SignalSubsystem) return;

    const bool bIsClient = (World->GetNetMode() == NM_Client);

    EntityQuery.ForEachEntityChunk(Context, [this, bIsClient, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i);
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i);
            }
        }
    });
}

void UPauseStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    FMassAITargetFragment& MutableTargetFrag = const_cast<FMassAITargetFragment&>(TargetFrag);
    
    if (!StateFrag.CanAttack)
    {
        MutableTargetFrag.TargetEntity.Reset();
        MutableTargetFrag.bHasValidTarget = false;
    }

    bool bIsTargetActive = EntityManager.IsEntityActive(MutableTargetFrag.TargetEntity);
    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    auto SightList = Context.GetMutableFragmentView<FMassSightFragment>();
    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    
    if (!bIsTargetActive || !MutableTargetFrag.bHasValidTarget || bIsTargetDead)
    {
        if (!StateFrag.SwitchingState)
        {
            if (SightList.Num() > 0) SightList[EntityIdx].AttackerTeamOverlapsPerTeam.Empty();
            if (MoveTargetList.Num() > 0)
            {
                UpdateMoveTarget(MoveTargetList[EntityIdx], StateFrag.StoredLocation, Stats.RunSpeed, World);
            }

            StateFrag.SwitchingState = true;
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
            }
        }
        return;
    }

    StateFrag.StateTimer += ExecutionInterval;

    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    
    const float Dist = FVector::Dist2D(Transform.GetLocation(), MutableTargetFrag.LastKnownLocation);
    float TargetCapsule = TargetCharFrag ? TargetCharFrag->CapsuleRadius : 0.f;
    float AttackRange = Stats.AttackRange + CharFrag.CapsuleRadius + TargetCapsule;

    if (Dist <= AttackRange)
    {
        if (StateFrag.StateTimer >= Stats.PauseDuration && !StateFrag.SwitchingState)
        {
            StateFrag.SwitchingState = true;
            int32 TargetTeamId = TgtStatsPtr ? TgtStatsPtr->TeamId : -1;

            if (Stats.bUseProjectile)
            {
                if (TargetTeamId != -1 && SightList.Num() > 0)
                {
                    SightList[EntityIdx].AttackerTeamOverlapsPerTeam.FindOrAdd(TargetTeamId)++;
                }
                
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::RangedAttack, Entity);
                }
            }
            else
            {
                if (TargetTeamId != -1 && SightList.Num() > 0)
                {
                    SightList[EntityIdx].AttackerTeamOverlapsPerTeam.FindOrAdd(TargetTeamId)++;
                }
                
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Attack, Entity);
                }
            }
        }
    }
    else if (!StateFrag.SwitchingState)
    {
        if (SightList.Num() > 0) SightList[EntityIdx].AttackerTeamOverlapsPerTeam.Empty();
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
        }
    }
}

void UPauseStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    const FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    const auto NetIDList = Context.GetFragmentView<FMassNetworkIDFragment>();
    const FMassNetworkID NetID = NetIDList[EntityIdx].NetID;

    if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
    {
        if (AUnitClientBubbleInfo* Bubble = Cache->GetBubble(false))
        {
            if (FUnitReplicationItem* Item = Bubble->Agents.FindItemByNetID(NetID))
            {
                // Advance client-only prediction timer
                Item->PredictionTimer += ExecutionInterval;

                const float LeadSeconds = 0.05f; // Small lead time for prediction
                if (Item->PredictionTimer >= FMath::Max(0.f, Stats.PauseDuration - LeadSeconds) && !Item->bPredictedLatch)
                {
                    // In-range and valid target check (mirror server logic)
                    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
                    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
                    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
                    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];

                    bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
                    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                    float TargetCapsule = TargetCharFrag ? TargetCharFrag->CapsuleRadius : 0.f;
                    const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                    const float AttackRange = Stats.AttackRange + CharFrag.CapsuleRadius + TargetCapsule;

                    if (bIsTargetActive && Dist <= AttackRange && Item->AIS_ProjectileClass)
                    {
                        if (UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>())
                        {
                            FTransform SpawnXf = Transform;
                            FVector SpawnPos = SpawnXf.GetLocation() + SpawnXf.GetRotation().GetForwardVector() * 50.f + SpawnXf.GetRotation().GetUpVector() * 100.f;
                            SpawnXf.SetLocation(SpawnPos);

                            FVector TargetLoc = TargetFrag.LastKnownLocation;
                            FVector ShootDir = (TargetLoc - SpawnPos).GetSafeNormal();
                            if (!ShootDir.IsNearlyZero())
                            {
                                SpawnXf.SetRotation(FQuat(ShootDir.Rotation()));
                            }

                            VisualManager->SpawnMassProjectile(
                                Item->AIS_ProjectileClass,
                                SpawnXf,
                                nullptr, nullptr,
                                TargetLoc,
                                Entity,
                                TargetFrag.TargetEntity,
                                Item->AIS_ProjectileSpeed,
                                Stats.TeamId,
                                Stats.bUseProjectile,
                                Item->AIS_HomingInitialAngle,
                                Item->AIS_HomingRotationSpeed,
                                Item->AIS_HomingMaxSpiralRadius,
                                Item->AIS_HomingInterpSpeed,
                                &Context.Defer()
                            );

                            // Latch and increment pending for reconciliation
                            Item->bPredictedLatch = true;
                            Item->PredictedPendingShots++;
                            
                            // Optimization: Reset timer to avoid immediate re-fire if interval is small
                            Item->PredictionTimer = 0.f;

                            UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] PauseStateProcessor: Predicted shot for NetID=%u, Pending=%u"), 
                                NetID.GetValue(), Item->PredictedPendingShots);
                        }
                    }
                }
            }
        }
    }
}
