// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"
#include "Async/Async.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"

URunStateProcessor::URunStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void URunStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
 EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Tag optional auf Client prüfen
    
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
   	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
 TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    // Throttle follow updates to max once per second (local static accumulator)
    static float FollowAccum = 0.0f;
    FollowAccum += ExecutionInterval;
    const bool bFollowTickThisFrameLocal = (FollowAccum >= 1.0f);
    if (bFollowTickThisFrameLocal)
    {
        FollowAccum = 0.0f;
    }
    // propagate to member so ExecuteClient/Server can read it
    this->bFollowTickThisFrame = bFollowTickThisFrameLocal;
    // Get World and Signal Subsystem once
    
    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }

}

void URunStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    // On client, only check for arrival and switch to Idle locally by adjusting tags via deferred commands.
    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto PredictionList = ChunkContext.GetFragmentView<FMassClientPredictionFragment>();
        const bool bHasPredList = PredictionList.Num() > 0;
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
            }

            // If already dead, do not modify any tags on client
            if (DoesEntityHaveTag(EntityManager, Entity, FMassStateDeadTag::StaticStruct()))
            {
                continue;
            }
            // If health is zero or below, mark as dead and skip further processing
            if (Stats.Health <= 0.f)
            {
                auto& Defer = ChunkContext.Defer();
                Defer.AddTag<FMassStateDeadTag>(Entity);
                continue;
            }

            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            
            FVector FinalDestination = MoveTarget.Center;
            float AcceptanceRadius = MoveTarget.SlackRadius;

            if (bHasPredList)
            {
                const FMassClientPredictionFragment& Pred = PredictionList[i];
                if (Pred.bHasData)
                {
                    FinalDestination = Pred.Location;
                    AcceptanceRadius = Pred.PredAcceptanceRadius;
                }
            }

            // Only arrival check on client (skip if following a unit)
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
         
            if (FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius*2.f)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    VelocityList[i].Value = FVector::ZeroVector;
                    SwitchToIdleState(ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            const bool bHasDetectTag = DoesEntityHaveTag(EntityManager, Entity, FMassStateDetectTag::StaticStruct());
            if (bHasDetectTag)
            {
                const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
                if (TargetFrag.bHasValidTarget && bIsTargetActive)
                {
                    if (!Stats.bCanMoveWhileAttacking)
                    {
                        if (!StateFrag.SwitchingStateClient)
                        {
                            SwitchToChaseState(ChunkContext, Entity, StateFrag);
                        }
                        continue;
                    }
                    else
                    {
                        // For bCanMoveWhileAttacking units, we check if they should switch to Pause (start attacking)
                        const float DistSq = FVector::DistSquared2D(CurrentLocation, TargetFrag.LastKnownLocation);

                        const FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
                        const bool bIsEnemyActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
                        const FMassAgentCharacteristicsFragment* TargetCharFrag = bIsEnemyActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                        const FTransformFragment* TargetTransformFrag = bIsEnemyActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
                        const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                        const float CombinedRadii = CharFrag ? RTSUnitUtils::GetCombinedRadii(*CharFrag, CurrentMassTransform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation) : 0.f;
                        const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                        const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                        if (DistSq <= AttackRangeSq)
                        {
                            if (!StateFrag.SwitchingStateClient)
                            {
                                SwitchToPauseState(ChunkContext, Entity, StateFrag);
                            }
                            continue;
                        }
                    }
                }
            }
        }
    });
}

void URunStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;
    
    EntityQuery.ForEachEntityChunk(Context,

        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!DoesEntityHaveTag(EntityManager, Entity, FMassStateRunTag::StaticStruct()))
            {
                continue;
            }

            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float AcceptanceRadius = MoveTarget.SlackRadius; //150.f;

            StateFrag.StateTimer += ExecutionInterval;

            
            // Follow friendly target directly if assigned
            const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
            
            if (DoesEntityHaveTag(EntityManager,Entity, FMassStateDetectTag::StaticStruct()) && TargetFrag.bHasValidTarget && bIsTargetActive && !Stats.bCanMoveWhileAttacking)
            {
                SwitchToChaseState(ChunkContext, Entity, StateFrag);
                continue;
            }else if (bIsFriendlyActive && !StateFrag.SwitchingState)
            {
                // Determine the friendly's current location
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                if (const FTransformFragment* FriendlyTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyTransform->GetTransform().GetLocation();
                }

                // Desired ring position at FollowRadius away from the friendly (XY only)
                const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
                FVector ToSelf2D = (CurrentMassTransform.GetLocation() - FriendlyLoc);
                ToSelf2D.Z = 0.f;
                const float Len2D = ToSelf2D.Size2D();
                const FVector Dir2D = (Len2D > KINDA_SMALL_NUMBER) ? (ToSelf2D / Len2D) : FVector::XAxisVector;
                FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                // Apply optional random XY offset, clamped to radius
                float OffsetMag = FMath::Clamp(TargetFrag.FollowOffset, 0.f, FollowRadius);
                if (OffsetMag > 0.f)
                {
                    // Unique, deterministic angular offset per entity to avoid identical positions
                    uint64 Seed = (uint64)Entity.Index | ((uint64)Entity.SerialNumber << 32);
                    // SplitMix64 scramble for good distribution
                    Seed += 0x9E3779B97F4A7C15ull;
                    Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
                    Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
                    Seed ^= (Seed >> 31);
                    // Map to [0,1) with 53-bit precision, then to [0, 2pi)
                    const double Unit = (double)(Seed >> 11) * (1.0 / 9007199254740992.0);
                    const float Angle = (float)(Unit * 2.0 * PI);
                    const float CosA = FMath::Cos(Angle);
                    const float SinA = FMath::Sin(Angle);
                    DesiredPos.X += CosA * OffsetMag;
                    DesiredPos.Y += SinA * OffsetMag;
                }
                // Use grounded Z if the target has characteristic data (buildings)
                if (const FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    DesiredPos.Z = TargetCharFrag->LastGroundLocation;
                }
                else
                {
                    DesiredPos.Z = FriendlyLoc.Z; // ignore Z while following
                }

                // Ensure DesiredPos is not in a dirty area
                if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
                {
                    FNavLocation DesiredNav;
                    if (NavSys->ProjectPointToNavigation(DesiredPos, DesiredNav, FVector(500.f, 500.f, 500.f)))
                    {
                        bool bDesiredDirty = false;
                        const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
                        if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
                        {
                            const uint32 PolyAreaID = Recast->GetPolyAreaID(DesiredNav.NodeRef);
                            const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                            bDesiredDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
                        }
                        
                        if (bDesiredDirty)
                        {
                            // Shift outward until clean
                            static const float ShiftRadii[] = {100.f, 250.f, 500.f};
                            for (float Shift : ShiftRadii)
                            {
                                FVector Candidate = DesiredPos + Dir2D * Shift;
                                FNavLocation CandNav;
                                if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(500.f, 500.f, 500.f)))
                                {
                                    if (const ARecastNavMesh* RM = Cast<ARecastNavMesh>(NavData))
                                    {
                                        const uint32 AID = RM->GetPolyAreaID(CandNav.NodeRef);
                                        const UClass* AC = RM->GetAreaClass(AID);
                                        if (!(AC && AC->IsChildOf(UNavArea_Obstacle::StaticClass())))
                                        {
                                            DesiredPos = CandNav.Location;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            DesiredPos = DesiredNav.Location;
                        }
                    }
                }


                UpdateMoveTarget(MoveTarget, DesiredPos, Stats.RunSpeed, World);  
                
                if (FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius)
                {
                    if (!StateFrag.SwitchingState)
                    {
                        VelocityList[i].Value = FVector::ZeroVector;
                        SwitchToIdleState(ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                    }
                    continue;
                }
                // Update MoveTarget towards the adjusted desired position; skip Idle arrival while following
            }
            else if (FVector::Dist2D(CurrentLocation, FinalDestination) <= (AcceptanceRadius))
            {
                VelocityList[i].Value = FVector::ZeroVector;
                SwitchToIdleState(ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                continue;
            }
            else if ( DoesEntityHaveTag(EntityManager, Entity, FMassStateDetectTag::StaticStruct()) &&
                    TargetFrag.bHasValidTarget && bIsTargetActive && Stats.bCanMoveWhileAttacking)
            {
                    const float DistSq = FVector::DistSquared2D(CurrentLocation, TargetFrag.LastKnownLocation);
                    
                    const FMassAgentCharacteristicsFragment* TargetCharFragPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                    const FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
                    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, CurrentMassTransform, TargetCharFragPtr, TargetTransform, TargetFrag.LastKnownLocation);
                    const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                    const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                    if (DistSq <= AttackRangeSq && !StateFrag.SwitchingState)
                    {
                        SwitchToPauseState(ChunkContext, Entity, StateFrag);
                        continue;
                    }
            }
            
        }
    }); // End ForEachEntityChunk

}

void URunStateProcessor::SwitchToIdleState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor)
{
    auto& Defer = Context.Defer();

    if (StateFrag.CanAttack && StateFrag.IsInitialized)
    {
        Defer.AddTag<FMassStateDetectTag>(Entity);
    }

    StateFrag.PlaceholderSignal = UnitSignals::Idle;
    if (AUnitBase* UnitBase = Cast<AUnitBase>(UnitActor))
    {
        UnitBase->UnitStatePlaceholder = UnitData::Idle;
    }
    
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        StateFrag.SwitchingStateClient = true;
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateChaseTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePauseTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
        Defer.AddTag<FMassStateIdleTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Idle, Entity);
        }
    }
}

void URunStateProcessor::SwitchToChaseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    auto& Defer = Context.Defer();
    
    if (StateFrag.CanAttack && StateFrag.IsInitialized)
    {
        Defer.AddTag<FMassStateDetectTag>(Entity);
    }
    
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        StateFrag.SwitchingStateClient = true;
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePauseTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
        Defer.AddTag<FMassStateChaseTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
        }
    }
}

void URunStateProcessor::SwitchToPauseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        auto& Defer = Context.Defer();
        StateFrag.SwitchingStateClient = true;
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
        Defer.RemoveTag<FMassStateChaseTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
        Defer.AddTag<FMassStatePauseTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Pause, Entity);
        }
    }
}
