// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
#include "MassReplicationFragments.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Core/RTSUnitUtils.h"
#include "Async/Async.h"
#include "Controller\PlayerController\CustomControllerBase.h"


UAttackStateProcessor::UAttackStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UAttackStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None); // Exclude Chase too
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Already excluded by other logic, but explicit
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Ensure the member SignalSubsystem is valid (initialized in Initialize)
    if (!SignalSubsystem) return;

    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;
    EntityQuery.ForEachEntityChunk(Context,
        // Use deferred signaling via ChunkContext; do NOT dispatch AsyncTask here
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto NetIDList = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();

        const bool bIsClient = (World->GetNetMode() == NM_Client);
        const bool bIsServer = (World->GetNetMode() != NM_Client);

        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassNetworkID& NetID = NetIDList[i].NetID;

            StateFrag.StateTimer += ExecutionInterval;

            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateFrag, TargetFrag, Stats, Entity, i, ActorList[i].GetMutable());
            }
            
            if (bIsServer)
            {
                ServerExecute(EntityManager, ChunkContext, StateFrag, TargetFrag, Stats, Entity, i, ActorList[i].GetMutable());
            }
        }
    }); // End ForEachEntityChunk
}

void UAttackStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
{
    UWorld* World = Context.GetWorld();
    if (!World) return;

    if (StateFrag.SwitchingStateClient)
    {
        StateFrag.SwitchingStateClient = false;
    }


    StateFrag.StateTimerClient += ExecutionInterval;

    FUnitReplicationItem* Item = nullptr;
    if (URTSWorldCacheSubsystem* CacheSubsystem = World->GetSubsystem<URTSWorldCacheSubsystem>())
    {
        if (AUnitClientBubbleInfo* Bubble = CacheSubsystem->GetBubble())
        {
            const auto NetIDList = Context.GetFragmentView<FMassNetworkIDFragment>();
            Item = Bubble->Agents.FindItemByNetID(NetIDList[EntityIdx].NetID);
            if (Item)
            {
                Item->PredictionTimer += ExecutionInterval;
            }
        }
    }

    auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    auto VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
    auto ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
    auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];

    if (!Stats.bCanMoveWhileAttacking)
    {
        if (VelocityList.IsValidIndex(EntityIdx)) VelocityList[EntityIdx].Value *= 0.1f;
        if (ForceList.IsValidIndex(EntityIdx)) ForceList[EntityIdx].Value = FVector::ZeroVector;

        if (PredictionList.Num() > 0)
        {
            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
            Pred.Location = Transform.GetLocation();
            Pred.PredDesiredSpeed = 0.f;
            Pred.bHasData = true;
        }
    }

    bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity) : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    if (!bIsTargetActive || !TargetFrag.bHasValidTarget || bIsTargetDead)
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;
            if (Item) Item->PredictionTimer = 0.f;

            StateFrag.PlaceholderSignal = UnitSignals::Idle;
            if (AUnitBase* UnitBase = Cast<AUnitBase>(Actor))
            {
                UnitBase->UnitStatePlaceholder = UnitData::Idle;
            }

            auto& Defer = Context.Defer();
            if (StateFrag.CanAttack && StateFrag.IsInitialized)
            {
                Defer.AddTag<FMassStateDetectTag>(Entity);
            }
            Defer.RemoveTag<FMassStateAttackTag>(Entity);
            Defer.AddTag<FMassStateIdleTag>(Entity);
        }
        return;
    }

    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];
    const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    if (Dist <= AttackRange)
    {
        if (StateFrag.StateTimerClient >= Stats.AttackDuration)
        {
            if (!StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = true;
                StateFrag.StateTimerClient = 0.f;
                if (Item) Item->PredictionTimer = 0.f;

                auto& Defer = Context.Defer();
                if (PredictionList.Num() > 0)
                {
                    FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                    Pred.Location = Transform.GetLocation();
                    Pred.PredDesiredSpeed = 0.f;
                    Pred.bHasData = true;
                }
                Defer.RemoveTag<FMassStateAttackTag>(Entity);
                Defer.AddTag<FMassStatePauseTag>(Entity);
            }
        }
    }
    else if (!StateFrag.SwitchingStateClient)
    {
        StateFrag.SwitchingStateClient = true;
        StateFrag.StateTimerClient = 0.f;
        if (Item) Item->PredictionTimer = 0.f;

        auto& Defer = Context.Defer();
        if (PredictionList.Num() > 0)
        {
            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
            if (const FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
            {
                Pred.Location = MoveTargetFrag->Center;
            }
            else
            {
                Pred.Location = TargetFrag.LastKnownLocation;
            }
            Pred.PredDesiredSpeed = Stats.RunSpeed;
            Pred.bHasData = true;
        }
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        
        if (Stats.bCanMoveWhileAttacking)
        {
            Defer.AddTag<FMassStateRunTag>(Entity);
        }
        else
        {
            Defer.AddTag<FMassStateChaseTag>(Entity);
        }
    }
}

void UAttackStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
{
    auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    auto VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
    auto ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];

    if (!Stats.bCanMoveWhileAttacking)
    {
        if (VelocityList.IsValidIndex(EntityIdx)) VelocityList[EntityIdx].Value *= 0.1f;
        if (ForceList.IsValidIndex(EntityIdx)) ForceList[EntityIdx].Value = FVector::ZeroVector;
                
        MoveTarget.DesiredSpeed.Set(0.f);
        MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
    }

    bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity) : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    if (!bIsTargetActive || !TargetFrag.bHasValidTarget || bIsTargetDead)
    {
        StateFrag.SwitchingState = true;
        StateFrag.PlaceholderSignal = UnitSignals::Idle;

        if (AUnitBase* UnitBase = Cast<AUnitBase>(Actor))
        {
            UnitBase->UnitStatePlaceholder = UnitData::Idle;
        }

        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
        }
        return;
    }

    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];
    const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    if (Dist <= AttackRange)
    {
        // --- Melee Impact Check ---
        if (StateFrag.StateTimer <= Stats.AttackDuration)
        {
            if (!Stats.bUseProjectile && !StateFrag.HasAttacked)
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::MeleeAttack, Entity);
                }
                StateFrag.HasAttacked = true;
            }
        }
        else if (!StateFrag.SwitchingState) // --- Attack Duration Over ---
        {
            StateFrag.SwitchingState = true;
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Pause, Entity);
            }
            StateFrag.HasAttacked = false;
        }
    }
    else if (!StateFrag.SwitchingState)
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            if (Stats.bCanMoveWhileAttacking)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Run, Entity);
            }
            else
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
            }
        }
    }
}

// float UAttackStateProcessor::GetCombinedRadii...