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
#include "Core/RTSUnitUtils.h"
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
    bRequiresGameThreadExecution = true;
}

void UPauseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite); // Eigene Position für Distanzcheck
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    
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
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        auto ForceList = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i, ActorList[i].GetMutable());
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i, ActorList[i].GetMutable());
            }
        }
    });
}

void UPauseStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
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
    const bool bIsFriendlyActive = EntityManager.IsEntityActive(MutableTargetFrag.FriendlyTargetEntity);
    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment* CharFragPtr = CharList.IsValidIndex(EntityIdx) ? &CharList[EntityIdx] : nullptr;

    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];
    
    if ((bIsFriendlyActive && !RTSUnitUtils::IsWithinFollowThreshold(EntityManager, Entity, MutableTargetFrag, CharFragPtr, Transform.GetLocation(), MoveTarget, World, 6.f)) || !bIsTargetActive || !MutableTargetFrag.bHasValidTarget || bIsTargetDead)
    {
        if (!StateFrag.SwitchingState)
        {
            StateFrag.SwitchingState = true;
            
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
            
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
            }
        }
        return;
    }

    StateFrag.StateTimer += ExecutionInterval;

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const FMassAgentCharacteristicsFragment& CharFrag = *CharFragPtr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    const float Dist = FVector::Dist2D(Transform.GetLocation(), MutableTargetFrag.LastKnownLocation);
    
    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, MutableTargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    if (Dist <= AttackRange)
    {
        if (StateFrag.StateTimer >= Stats.PauseDuration && !StateFrag.SwitchingState)
        {
            StateFrag.SwitchingState = true;
            if (Stats.bUseProjectile)
            {
                if (SignalSubsystem)
                {
                    StateFrag.StateTimer = 0.f;
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::RangedAttack, Entity);
                }
            }
            else
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Attack, Entity);
                }
            }
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

void UPauseStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
{
    if (StateFrag.SwitchingStateClient)
    {
        StateFrag.SwitchingStateClient = false;
    }


    ApplyAttackStopLogic(Context, Stats, TargetFrag, Entity, EntityIdx);

    StateFrag.StateTimerClient += ExecutionInterval;
    
    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];

    bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
    const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];

    if (bIsFriendlyActive && !RTSUnitUtils::IsWithinFollowThreshold(EntityManager, Entity, TargetFrag, &CharFrag, Transform.GetLocation(), MoveTarget, EntityManager.GetWorld(), 6.f))
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;
            auto& Defer = Context.Defer();

            StateFrag.PlaceholderSignal = UnitSignals::Idle;
            if (AUnitBase* UnitBase = Cast<AUnitBase>(Actor))
            {
                UnitBase->UnitStatePlaceholder = UnitData::Idle;
            }

            if (StateFrag.CanAttack && StateFrag.IsInitialized)
            {
                Defer.AddTag<FMassStateDetectTag>(Entity);
            }
            Defer.RemoveTag<FMassStatePauseTag>(Entity);
            Defer.AddTag<FMassStateIdleTag>(Entity);
        }
        return;
    }

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
    
    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    if (bIsTargetActive)
    {
        if (Dist <= AttackRange)
        {
            if (StateFrag.StateTimerClient >= Stats.PauseDuration)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    StateFrag.SwitchingStateClient = true;
                    StateFrag.StateTimerClient = 0.f;
                    auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
                    if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
                    {
                        if (!Stats.bCanMoveWhileAttacking)
                        {
                            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                            Pred.Location = TargetFrag.LastKnownLocation;
                            Pred.PredDesiredSpeed = 0.f;
                            Pred.bHasData = true;
                        }
                    }
                    Context.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                    Context.Defer().AddTag<FMassStateAttackTag>(Entity);
                }
            }
        }
        else if (Dist > AttackRange + 150.f)
        {
            if (!StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = true;
                StateFrag.StateTimerClient = 0.f;
                auto& Defer = Context.Defer();
                auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
                if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
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
                if (StateFrag.CanAttack && StateFrag.IsInitialized)
                {
                    Defer.AddTag<FMassStateDetectTag>(Entity);
                }
                Defer.RemoveTag<FMassStatePauseTag>(Entity);
                
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
    }
    else
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;
            auto& Defer = Context.Defer();
            auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
            if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
            {
                FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                if (StateFrag.PlaceholderSignal == UnitSignals::Run)
                {
                    if (const FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                    {
                        Pred.Location = MoveTargetFrag->Center;
                    }
                    Pred.PredDesiredSpeed = Stats.RunSpeed;
                }
                else
                {
                    Pred.Location = Transform.GetLocation();
                    Pred.PredDesiredSpeed = 0.f;
                }
                Pred.bHasData = true;
            }
            
            Defer.RemoveTag<FMassStatePauseTag>(Entity);
            
            if (StateFrag.PlaceholderSignal == UnitSignals::Run)
                Defer.AddTag<FMassStateRunTag>(Entity);
            else
                Defer.AddTag<FMassStateIdleTag>(Entity);
        }
    }
}

void UPauseStateProcessor::ApplyAttackStopLogic(FMassExecutionContext& Context, 
    const FMassCombatStatsFragment& Stats, const FMassAITargetFragment& TargetFrag, 
    const FMassEntityHandle Entity, const int32 EntityIdx)
{
    if (!Stats.bCanMoveWhileAttacking)
    {
        auto VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
        auto ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
        auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
        auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto TransformList = Context.GetFragmentView<FTransformFragment>();

        if (VelocityList.IsValidIndex(EntityIdx)) VelocityList[EntityIdx].Value *= 0.1f;
        if (ForceList.IsValidIndex(EntityIdx)) ForceList[EntityIdx].Value = FVector::ZeroVector;

        if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
        {
            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
            Pred.Location = TransformList[EntityIdx].GetTransform().GetLocation();
            Pred.PredDesiredSpeed = 0.f;
            Pred.bHasData = true;
        }
    }
}

// float UPauseStateProcessor::GetCombinedRadii...

void UPauseStateProcessor::OnProjectileSignalReceived(FName SignalName, const TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem) return;
	
	UWorld* World = EntitySubsystem->GetWorld();
	if (!World || World->GetNetMode() != NM_Client) return; // FIX: Nur auf dem Client spawnen (Server spawnt via UnitBase)

	// Schutz gegen Mehrfach-Instanzen: Nur ein Spawn pro Entity pro Frame
	static TMap<FMassEntityHandle, uint64> LastSpawnFramePerEntity;
	const uint64 CurrentFrame = GFrameCounter;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (EntityManager.IsEntityValid(Entity))
		{
			if (uint64* LastFrame = LastSpawnFramePerEntity.Find(Entity))
			{
				if (*LastFrame == CurrentFrame) continue; // Bereits in diesem Frame gespawnt
			}
			LastSpawnFramePerEntity.Add(Entity, CurrentFrame);

			ExecuteProjectileSpawn(EntityManager, Entity);
		}
	}

	// Aufräumen alter Einträge
	if (LastSpawnFramePerEntity.Num() > 2000)
	{
		for (auto It = LastSpawnFramePerEntity.CreateIterator(); It; ++It)
		{
			if (It.Value() < CurrentFrame) It.RemoveCurrent();
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

    for (const FVector& Pos : SpawnPositions)
    {
        for (int32 i = 0; i < BaseCount; ++i)
        {
            FTransform FinalSpawnXf = BaseSpawnXf;
            FinalSpawnXf.SetLocation(Pos);
            
            FVector TargetLoc = TargetF->LastKnownLocation;
            FVector Direction = (TargetLoc - Pos).GetSafeNormal();
            float InitialAngle = 0.f;
            float RotSpeed = ProjCDO->HomingRotationSpeed;
            float MaxRadius = ProjCDO->HomingMaxSpiralRadius;
            float FinalSpeed = ProjectileSpeed;

            if (HomingCount > 0)
            {
                // Richtungs-Jitter für fächerförmigen Start (Identisch zum Server)
                if (BaseCount > 1)
                {
                    float JitterAngle = (360.0f / BaseCount) * i;
                    FVector Right, Up;
                    Direction.FindBestAxisVectors(Right, Up);
                    Direction = (Direction + (Right * FMath::Cos(FMath::DegreesToRadians(JitterAngle)) + Up * FMath::Sin(FMath::DegreesToRadians(JitterAngle))) * 0.1f).GetSafeNormal();
                }
                InitialAngle = FMath::RandRange(0.f, 360.f);
                RotSpeed *= FMath::RandRange(0.9f, 1.4f);
                if (FMath::RandBool()) RotSpeed *= -1.f;
                MaxRadius *= FMath::RandRange(0.8f, 1.2f);
                FinalSpeed += FMath::RandRange(-ProjCDO->HomingSpeedVariation, ProjCDO->HomingSpeedVariation);
            }
            else
            {
                InitialAngle = (HomingCount > 1) ? (360.0f / BaseCount) * i : 0.f;
            }

            if (!Direction.IsNearlyZero()) FinalSpawnXf.SetRotation(FQuat(Direction.Rotation()));

            bool bFinalFollowTarget = ProjCDO->FollowTarget;
            if (HomingCount > 0) bFinalFollowTarget = true; // FIX: Konsistenz mit Server erzwingen

            VisualManager->SpawnMassProjectile(
                UnitActor->ProjectileBaseClass,
                FinalSpawnXf,
                UnitActor, nullptr,
                TargetLoc,
                Entity,
                TargetF->TargetEntity,
                FinalSpeed,
                StatsF->TeamId,
                bFinalFollowTarget, // FIX: bIsHoming wird dadurch im Fragment TRUE
                InitialAngle,
                RotSpeed,
                MaxRadius,
                ProjCDO->HomingInterpSpeed,
                nullptr, // Keine Deferral hier nötig, da wir im Signal-Handler sind (Game Thread)
                UnitActor->ProjectileScale,
                -1.f, // Nutze CDO-Schaden via Lazy-Init
                -1,    // Nutze CDO-MaxPiercedTargets via Lazy-Init (FIX!)
                true   // bIsPredicted
            );
        }
    }
}
