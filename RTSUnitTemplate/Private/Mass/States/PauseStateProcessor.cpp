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
#include "MassEntitySubsystem.h"
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
#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"

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
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());

    if (SignalSubsystem)
    {
        // Bindung der Funktion an das Signal "RangedAttack"
        ProjectileSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UPauseStateProcessor, OnProjectileSignalReceived));
    }
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
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i, ActorList[i].GetMutable());
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

    float AttackerRadius = CharFrag.CapsuleRadius;
    float TargetRadius = 0.f;

    if (TargetCharFrag)
    {
        TargetRadius = TargetCharFrag->CapsuleRadius;
        if (CharFrag.bUseBoxComponent || TargetCharFrag->bUseBoxComponent)
        {
            FVector Dir = (MutableTargetFrag.LastKnownLocation - Transform.GetLocation());
            Dir.Z = 0.f;
            if (!Dir.IsNearlyZero())
            {
                Dir.Normalize();
                AttackerRadius = CharFrag.GetRadiusInDirection(Dir, Transform.GetRotation().Rotator());
                
                FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(MutableTargetFrag.TargetEntity);
                if (TargetTransformFrag)
                {
                    TargetRadius = TargetCharFrag->GetRadiusInDirection(-Dir, TargetTransformFrag->GetTransform().GetRotation().Rotator());
                }
            }
        }
    }
    
    float AttackRange = Stats.AttackRange + AttackerRadius + TargetRadius;

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
            if (!Stats.bCanMoveWhileAttacking) SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
            else SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Run, Entity);
        }
    }
}

void UPauseStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    const FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
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
                    
                    float AttackerRadius = CharFrag.CapsuleRadius;
                    float TargetRadius = 0.f;

                    if (TargetCharFrag)
                    {
                        TargetRadius = TargetCharFrag->CapsuleRadius;
                        if (CharFrag.bUseBoxComponent || TargetCharFrag->bUseBoxComponent)
                        {
                            FVector Dir = (TargetFrag.LastKnownLocation - Transform.GetLocation());
                            Dir.Z = 0.f;
                            if (!Dir.IsNearlyZero())
                            {
                                Dir.Normalize();
                                AttackerRadius = CharFrag.GetRadiusInDirection(Dir, Transform.GetRotation().Rotator());
                                
                                FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity);
                                if (TargetTransformFrag)
                                {
                                    TargetRadius = TargetCharFrag->GetRadiusInDirection(-Dir, TargetTransformFrag->GetTransform().GetRotation().Rotator());
                                }
                            }
                        }
                    }

                    const float AttackRange = Stats.AttackRange + AttackerRadius + TargetRadius;

                    if (bIsTargetActive && Dist <= AttackRange)
                    {
                        if (SignalSubsystem)
                        {
                            // Signal triggert verzögert den Delegate (OnProjectileSignalReceived)
                            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::RangedAttack, Entity);
                        }
    
                        // Prediction-Zustand markieren
                        Item->bPredictedLatch = true;
                        Item->PredictionTimer = 0.f;
                        Item->PredictedPendingShots++;
    
                        Context.Defer().AddTag<FMassStateAttackTag>(Entity);

                        UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] PauseStateProcessor: Predicted shot for NetID=%u, Pending=%u"), 
                            NetID.GetValue(), Item->PredictedPendingShots);
                    }
                }
            }
        }
    }
}

void UPauseStateProcessor::OnProjectileSignalReceived(FName SignalName, const TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;
    
    UWorld* World = EntitySubsystem->GetWorld();
    if (!World || World->GetNetMode() != NM_Client) return; // FIX: Nur auf dem Client spawnen (Server spawnt via UnitBase)

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    for (const FMassEntityHandle& Entity : Entities)
    {
        if (EntityManager.IsEntityValid(Entity))
        {
            ExecuteProjectileSpawn(EntityManager, Entity);
        }
    }
}

void UPauseStateProcessor::ExecuteProjectileSpawn(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
    // 1. Daten aus Fragmenten abrufen
    const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
    const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
    FMassActorFragment* AF = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    const FMassAITargetFragment* TargetF = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
    const FMassCombatStatsFragment* StatsF = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

    if (!TF || !AC || !AF || !TargetF || !StatsF) return;

    AUnitBase* UnitActor = Cast<AUnitBase>(AF->GetMutable());
    if (!UnitActor || !UnitActor->ProjectileBaseClass) return;

    UWorld* World = UnitActor->GetWorld();
    if (!World) return;
    UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>();
    if (!VisualManager) return;

    const AProjectile* ProjCDO = VisualManager->GetProjectileCDO(UnitActor->ProjectileBaseClass);
    if (!ProjCDO) return;

    // 2. Positionsbestimmung: Nutze direkt die Actor-Logik für den Mündungs-Punkt
    FVector SpawnLocation = UnitActor->GetProjectileSpawnLocation();
    
    FTransform BaseSpawnXf = TF->GetTransform();
    BaseSpawnXf.SetLocation(SpawnLocation);

    const FVector BaseSpawnPos = SpawnLocation;
    
    // 3. Multi-Shot Logik aus CDO (Twin Projectiles / Homing Salven)
    int32 HomingCount = ProjCDO->HomingMissleCount;
    int32 BaseCount = (HomingCount > 0) ? HomingCount : 1;
    TArray<FVector> SpawnPositions;

    if (ProjCDO->TwinProjectileDistance >= 10.f)
    {
        FVector DirToTarget = (TargetF->LastKnownLocation - BaseSpawnPos).GetSafeNormal2D();
        FVector RightVector = DirToTarget.IsNearlyZero() ? UnitActor->GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
        FVector RightOffset = RightVector * ProjCDO->TwinProjectileDistance;
        SpawnPositions.Add(BaseSpawnPos - RightOffset);
        SpawnPositions.Add(BaseSpawnPos + RightOffset);
    }
    else
    {
        SpawnPositions.Add(BaseSpawnPos);
    }

    // 4. Finaler Spawn über VisualManager
    float ProjectileSpeed = (UnitActor->Attributes && UnitActor->Attributes->GetProjectileSpeed() > 0.f) ? UnitActor->Attributes->GetProjectileSpeed() : ProjCDO->MovementSpeed;

    // Logging ohne Throttling für den Spawn auf dem Client
    UE_LOG(LogTemp, Log, TEXT("[CLIENT] Spawning Projectile: Class=%s, Speed=%.2f, MaxPierced=%d, Damage=%.2f, PosZ=%.2f"),
        *UnitActor->ProjectileBaseClass->GetName(), ProjectileSpeed, ProjCDO->MaxPiercedTargets, ProjCDO->Damage, SpawnLocation.Z);

    for (const FVector& Pos : SpawnPositions)
    {
        for (int32 i = 0; i < BaseCount; ++i)
        {
            FTransform FinalSpawnXf = BaseSpawnXf;
            FinalSpawnXf.SetLocation(Pos);
            
            FVector TargetLoc = TargetF->LastKnownLocation;
            FVector Direction = (TargetLoc - Pos).GetSafeNormal();
            float InitialAngle = (HomingCount > 1) ? (360.0f / BaseCount) * i : 0.f;

            if (!Direction.IsNearlyZero()) FinalSpawnXf.SetRotation(FQuat(Direction.Rotation()));

            VisualManager->SpawnMassProjectile(
                UnitActor->ProjectileBaseClass,
                FinalSpawnXf,
                UnitActor, nullptr,
                TargetLoc,
                Entity,
                TargetF->TargetEntity,
                ProjectileSpeed,
                StatsF->TeamId,
                ProjCDO->FollowTarget,
                InitialAngle,
                ProjCDO->HomingRotationSpeed,
                ProjCDO->HomingMaxSpiralRadius,
                ProjCDO->HomingInterpSpeed,
                nullptr, // Keine Deferral hier nötig, da wir im Signal-Handler sind (Game Thread)
                UnitActor->ProjectileScale,
                -1.f, // Nutze CDO-Schaden via Lazy-Init
                -1    // Nutze CDO-MaxPiercedTargets via Lazy-Init (FIX!)
            );
        }
    }
}
