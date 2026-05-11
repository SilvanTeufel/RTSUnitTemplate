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
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
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
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

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
        auto SightList = ChunkContext.GetMutableFragmentView<FMassSightFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        auto ForceList = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();

        const bool bIsClient = (World->GetNetMode() == NM_Client);
        const bool bIsServer = (World->GetNetMode() != NM_Client);

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // State modification stays here
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassSightFragment& SightFrag = SightList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for UpdateMoveTarget

            if (bIsClient && !Stats.bCanMoveWhileAttacking)
            {
                if (VelocityList.IsValidIndex(i)) VelocityList[i].Value *= 0.1f;
                if (ForceList.IsValidIndex(i)) ForceList[i].Value = FVector::ZeroVector;

                if (PredictionList.Num() > 0)
                {
                    FMassClientPredictionFragment& Pred = PredictionList[i];
                    Pred.Location = Transform.GetLocation();
                    Pred.PredDesiredSpeed = 0.f;
                    Pred.bHasData = true;
                }
                
                MoveTarget.DesiredSpeed.Set(0.f);
                MoveTarget.IntentAtGoal = EMassMovementAction::Stand;

                
                // NEU: Prädiktive Rotation zum Ziel
                FVector LookAtDir = (TargetFrag.LastKnownLocation - Transform.GetLocation());
                LookAtDir.Z = 0.f;
                if (LookAtDir.Normalize())
                {
                    FQuat LookAtQuat = LookAtDir.ToOrientationQuat();
                    FTransform& MutableTransform = ChunkContext.GetMutableFragmentView<FTransformFragment>()[i].GetMutableTransform();
                    MutableTransform.SetRotation(LookAtQuat);
                }
                
            }

            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
            FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity) : nullptr;
            const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

            if (!bIsTargetActive || !TargetFrag.bHasValidTarget || bIsTargetDead)
            {
                if (!StateFrag.SwitchingState)
                {
                    SightFrag.AttackerTeamOverlapsPerTeam.Empty();
                    // Queue signal instead of sending directly
                    if (bIsServer)
                    {
                        UpdateMoveTarget(
                         MoveTarget,
                         StateFrag.StoredLocation,
                         Stats.RunSpeed,
                         World);
                    }
                
                    StateFrag.SwitchingState = true;
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SetUnitStatePlaceholder, Entity);
                    }
                }
                continue;
            }
            
            // --- Attack Cycle Timer ---
            StateFrag.StateTimer += ExecutionInterval; // State modification stays here
            
                // --- Range Check ---
                //const float Dist = FVector::Dist(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

                float AttackerRadius = CharFrag.CapsuleRadius;
                float TargetRadius = 0.f;

                FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
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
            
                if (Dist <= AttackRange)
                {
                    // --- Melee Impact Check ---
                    if (StateFrag.StateTimer <= Stats.AttackDuration)
                    {
                        if (!Stats.bUseProjectile && !StateFrag.HasAttacked)
                        {
                            if (SignalSubsystem)
                            {
                                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::MeleeAttack, Entity);
                            }
                            StateFrag.HasAttacked = true; // State modification stays here
                        }
                    }else if (!StateFrag.SwitchingState) // --- Attack Duration Over ---
                    {
                        StateFrag.SwitchingState = true;
                        if (SignalSubsystem)
                        {
                            SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Pause, Entity);
                        }
                        StateFrag.HasAttacked = false; // State modification stays here
                        continue;
                    }
                }
                else if (!StateFrag.SwitchingState)
                {
                    StateFrag.SwitchingState = true;
                    if (SignalSubsystem)
                    {
                        if (!Stats.bCanMoveWhileAttacking) SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                        else SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Run, Entity);
                    }
                    continue;
                }
      
        }
    }); // End ForEachEntityChunk


}