// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitMovementProcessor.h"

// ... other includes ...
#include "MassExecutionContext.h"
#include "Steering/MassSteeringFragments.h" // Ensure included
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavAreas/NavArea_EnergyWall.h"
#include "Async/Async.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/CapsuleComponent.h"

// Projection-based, MONOTONIC path-index advance (shared by client + server path-following).
// The old logic advanced PathFrag.CurrentPathPointIndex ONLY when the unit was within PathWaypointAcceptanceRadius
// of the CURRENT waypoint. If the unit was pushed PAST a waypoint (overshoot / sideways separation shove / volatile
// FOLLOW target) without entering that radius, the index STALLED and the unit steered BACKWARD toward the passed
// waypoint -> the "waypoint error" jerk (worst on FOLLOW, where the follow target is self-referential + the leader
// moves, so re-pathfinding is constant and early waypoints land near/behind the unit). Instead we project the unit
// onto the path polyline and steer toward the FORWARD end of the closest segment, clamped to never go backward
// (monotonic). Bounded look-ahead keeps it cheap and avoids skipping across hairpins. 2D (flat navigation).
static void RTS_AdvancePathIndexByProjection(FUnitNavigationPathFragment& PathFrag, const TArray<FNavPathPoint>& PathPoints, const FVector& CurrentLocation)
{
	const int32 NumPts = PathPoints.Num();
	if (NumPts < 2) return;
	const int32 Cur = FMath::Clamp(PathFrag.CurrentPathPointIndex, 0, NumPts - 1);
	const int32 StartSeg = FMath::Max(0, Cur - 1);                 // include the segment leading to the current target
	const int32 EndSeg = FMath::Min(NumPts - 2, Cur + 4);          // bounded look-ahead (cheap; realistic overshoot)
	int32 BestTargetIdx = Cur;
	float BestSegDistSq = TNumericLimits<float>::Max();
	for (int32 Seg = StartSeg; Seg <= EndSeg; ++Seg)
	{
		const FVector A = PathPoints[Seg].Location;
		const FVector B = PathPoints[Seg + 1].Location;
		FVector AB = B - A; AB.Z = 0.f;
		const float ABLenSq = AB.SizeSquared();
		if (ABLenSq <= KINDA_SMALL_NUMBER) continue;
		FVector AP = CurrentLocation - A; AP.Z = 0.f;
		const float T = FMath::Clamp(FVector::DotProduct(AP, AB) / ABLenSq, 0.f, 1.f);
		const FVector Proj = A + AB * T;
		const float DistSq = FVector::DistSquared2D(CurrentLocation, Proj);
		if (DistSq < BestSegDistSq)
		{
			BestSegDistSq = DistSq;
			BestTargetIdx = Seg + 1; // steer toward the forward end of the segment the unit is currently on
		}
	}
	PathFrag.CurrentPathPointIndex = FMath::Clamp(FMath::Max(Cur, BestTargetIdx), 0, NumPts - 1); // monotonic
}

UUnitMovementProcessor::UUnitMovementProcessor(): EntityQuery()
{
    // Run BEFORE steering, avoidance, and movement integration
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; // Or potentially Input
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
    ProcessingPhase = EMassProcessingPhase::PrePhysics; // Good phase for pathfinding/intent
    
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UUnitMovementProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());
}

void UUnitMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);        // READ current position
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);  // READ the target location/speed
    EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite); // WRITE desired velocity
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite); // Keep for managing path state
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
    
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);   
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);  
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    //EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);

	ClientEntityQuery.Initialize(EntityManager);
	ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite);

    ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
    
    ClientEntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);   
    ClientEntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);  
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);   
    ClientEntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    //ClientEntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

    ClientEntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
    ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
    ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
    ClientEntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    ClientEntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
    
	ClientEntityQuery.RegisterWithProcessor(*this);
}



void UUnitMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Lazy-init EntitySubsystem on first Execute in case InitializeInternal ran before world subsystems were ready (common on clients)
    if (!EntitySubsystem)
    {
        EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        if (!EntitySubsystem)
        {
            return;
        }
    }

    if (World->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UUnitMovementProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    const bool bHasNavSystem = (NavSystem != nullptr);

    ClientEntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassSteeringFragment& Steering = SteeringList[i];
            Steering.DesiredVelocity = FVector::ZeroVector;

            const FMassAIStateFragment& AIState = AIStateList[i];
            const FMassMoveTargetFragment& MoveTarget = TargetList[i];
            
            if (const AActor* Actor = ActorList[i].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitMovementProcessor Client: %s - AIState.CanMove: %s, IsInitialized: %s, Target: %s, Speed: %f"), 
                        *Actor->GetName(), AIState.CanMove ? TEXT("True") : TEXT("False"), AIState.IsInitialized ? TEXT("True") : TEXT("False"), *MoveTarget.Center.ToString(), MoveTarget.DesiredSpeed.Get());
                }
            }
            
            // === [PredStall] error-only: a unit with an ACTIVE prediction (bHasData) the mover is about to
            // SKIP because it can't move -> one reason a commanded client unit stands still. Gated on
            // "predicting for >1s" so only genuinely-stuck units log (no per-frame spam). ===
            if (PredList[i].bHasData && (!AIState.IsInitialized || !AIState.CanMove))
            {
                const float Since = World ? (World->GetTimeSeconds() - PredList[i].CommandPredictTime) : -1.f;
                if (Since > 1.0f && Since < 1.2f)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[PredStall] CANTMOVE Idx=%d init=%d canMove=%d since=%.2f pred=%s cur=%s"),
                        Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                        AIState.IsInitialized ? 1 : 0, AIState.CanMove ? 1 : 0, Since,
                        *PredList[i].Location.ToString(), *TransformList[i].GetTransform().GetLocation().ToString());
                }
            }
            if (!AIState.IsInitialized)
            {
                continue;
            }
            if (!AIState.CanMove)
            {
                continue;
            }

            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];

            // Stationary worker states (Build / ResourceExtraction / Repair) must NOT be locally moved on
            // the client. A prediction left over from the preceding GoTo state (Pred.bHasData=true with a
            // now-frozen Location) makes this mover keep steering toward that stale point - and the
            // Pred.bHasData branch below bumps a zero DesiredSpeed up to 100 - so the unit drifts/jitters
            // while the server (DesiredSpeed=0, no prediction) stands completely still. Drop the stale
            // prediction and hold; the reconciler keeps the unit at the authoritative server position.
            {
                const bool bStationaryWorker =
                    DoesEntityHaveTag(EntityManager, Entity, FMassStateBuildTag::StaticStruct()) ||
                    DoesEntityHaveTag(EntityManager, Entity, FMassStateResourceExtractionTag::StaticStruct()) ||
                    DoesEntityHaveTag(EntityManager, Entity, FMassStateRepairTag::StaticStruct());
                if (bStationaryWorker)
                {
                    PredList[i].bHasData = false;
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    PathFrag.ResetPath();
                    PathFrag.bIsPathfindingInProgress = false;
                    continue;
                }
            }

            FVector CurrentLocation = CurrentMassTransform.GetLocation();
            FVector FinalDestination = MoveTarget.Center;
            float DesiredSpeedUsed = MoveTarget.DesiredSpeed.Get();
            float AcceptanceRadiusUsed = MoveTarget.SlackRadius;

            FMassClientPredictionFragment& Pred = PredList[i];
            if (Pred.bHasData)
            {
                FinalDestination = Pred.Location;
                if (Pred.PredDesiredSpeed > KINDA_SMALL_NUMBER)
                {
                    DesiredSpeedUsed = Pred.PredDesiredSpeed;
                }
                else if (DesiredSpeedUsed <= KINDA_SMALL_NUMBER)
                {
                    DesiredSpeedUsed = 100.f;
                }
                if (Pred.PredAcceptanceRadius > KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = Pred.PredAcceptanceRadius;
                }
                else if (AcceptanceRadiusUsed <= KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = 100.f;
                }
                // Clear the prediction ONLY when the UNIT has actually reached its predicted target.
                // PREVIOUS BUG: this compared MoveTarget.Center vs Pred.Location. With replication ON, the server
                // replicates MoveTarget.Center == the commanded target (== Pred.Location) almost immediately, so the
                // condition was satisfied the instant the bubble delivered the new MoveTarget.Center - while the unit
                // was still at the START (cur far away). That cleared the prediction every command, local movement
                // stopped, and the unit snapped on the next 10Hz replication tick ("held, then snaps"). With
                // replication OFF it never fired because MoveTarget.Center stayed stale. Per the invariant
                // "a prediction must not be cleared while the unit has not reached its target", gate the clear on the
                // UNIT's distance to Pred.Location, not the server target's.
                if (FVector::DistSquared2D(CurrentLocation, Pred.Location) <= FMath::Square(AcceptanceRadiusUsed) &&
                    FVector::DistSquared2D(CurrentLocation, MoveTarget.Center) <= FMath::Square(AcceptanceRadiusUsed))
                {
                    Pred.bHasData = false;
                }
            }

            
            if (CharFrag.bIsFlying)
            {
                if (FVector::DistSquared(CurrentLocation, FinalDestination) > FMath::Square(AcceptanceRadiusUsed))
                {
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                }
                continue;
            }
            
            if (const AUnitBase* UnitBase = Cast<AUnitBase>( ActorList[i].Get()))
            {
                if (const UCapsuleComponent* Capsule = UnitBase->GetCapsuleComponent())
                {
                    CurrentLocation.Z -= Capsule->GetScaledCapsuleHalfHeight()/2;
                }
            }
            
            if (FVector::DistSquared2D(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadiusUsed))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress)
            {
                // Begin a new path request if we have navigation; otherwise, steer directly.
                if (bHasNavSystem)
                {
                    PathFrag.bIsPathfindingInProgress = true;
                    
                    // WICHTIG: Wir speichern exakt das ab, was wir anfragen (UNPROJIZIERT)
                    PathFrag.PathTargetLocation = FinalDestination;
                    
                    if (Pred.bHasData)
                    {
                        Pred.Location = FinalDestination;
                    }

                    FNavLocation ProjectedStartLocation;
                    if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                    {
                        // FIX A: Absolute Bremse sofort aktivieren, damit Momentum uns nicht in die Wand drückt!
                        Steering.DesiredVelocity = FVector::ZeroVector; 
                        
                        // Wir übergeben FinalDestination direkt an den Pathfinder!
                        RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, FinalDestination);
                    }
                    else
                    {
                        // GEÄNDERT: Kein Blindflug!
                        Steering.DesiredVelocity = FVector::ZeroVector;
                        PathFrag.bIsPathfindingInProgress = false;
                        PathFrag.ResetPath();
                        // [PredStall] error-only: start point won't project onto the (client) navmesh -> no path
                        // requested -> unit stands. Client-specific if the client navmesh has holes the server lacks.
                        if (Pred.bHasData)
                        {
                            const float Since = World ? (World->GetTimeSeconds() - Pred.CommandPredictTime) : -1.f;
                            if (Since > 1.0f && Since < 1.2f)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[PredStall] PROJECT-FAIL Idx=%d since=%.2f cur=%s pred=%s"),
                                    Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                                    Since, *CurrentLocation.ToString(), *Pred.Location.ToString());
                            }
                        }
                    }
                }
                else
                {
                    // GEÄNDERT: DirectSteer für Bodentruppen OHNE Pfad ist gefährlich.
                    // Wenn sie keinen Pfad haben, sollen sie stehen bleiben, nicht fliegen!
                    Steering.DesiredVelocity = FVector::ZeroVector; 
                    PathFrag.bIsPathfindingInProgress = false;
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
                // GEÄNDERT: Kein blindes Vorwärtslaufen mehr! 
                // Wenn wir den Pfad berechnen, bleiben wir stehen, um nicht durch Wände zu glitchen.
                Steering.DesiredVelocity = FVector::ZeroVector; 
            }
            else if (PathFrag.HasValidPath())
            {
                // NEU: Wenn das Navmesh unter uns neu generiert wurde (EnergyWall gebaut),
                // verwerfen wir den alten Pfad sofort, damit wir im nächsten Frame neu planen!
                if (!PathFrag.CurrentPath->IsValid())
                {
                    PathFrag.ResetPath();
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    continue; 
                }

                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();

                // Monotonic projection-based advance (replaces proximity-only ++index that stalled on overshoot/
                // pushed-past waypoints -> backward "waypoint error", worst on FOLLOW). See helper above.
                RTS_AdvancePathIndexByProjection(PathFrag, PathPoints, CurrentLocation);

                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    FVector MoveDir = (PathPoints[PathFrag.CurrentPathPointIndex].Location - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeedUsed;
                }
                else
                {
                    PathFrag.ResetPath();
                }
            }
            else
            {
                 // GEÄNDERT: DirectSteer für Bodentruppen OHNE Pfad ist gefährlich.
                 // Wenn sie keinen Pfad haben, sollen sie stehen bleiben, nicht fliegen!
                 Steering.DesiredVelocity = FVector::ZeroVector;
            }

            // [PredStall] DECISIVE end-of-frame probe (no bHasData / no 1s gating): if a RECENTLY-COMMANDED unit
            // ends this frame with ZERO steering, it stands this frame. This captures EVERY no-move branch in one
            // place (arrived-collapse, no-path, pathing, degenerate-path, project-fail) and works even if the
            // prediction was already cleared (CommandPredictTime survives a clear). If the stuck units NEVER
            // appear here while standing, they are excluded from the client mover query entirely (a state-tag
            // issue), NOT a movement-branch issue.
            {
                const float SinceCmd = World ? (World->GetTimeSeconds() - Pred.CommandPredictTime) : 999.f;
                if (SinceCmd > 0.3f && SinceCmd < 0.6f && Steering.DesiredVelocity.IsNearlyZero())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[PredStall] NOMOVE Idx=%d since=%.2f bHasData=%d arrived=%d distFinal=%.0f accept=%.0f hasPath=%d pathing=%d final=%s cur=%s pred=%s moveCtr=%s"),
                        Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                        SinceCmd, Pred.bHasData ? 1 : 0,
                        (FVector::DistSquared2D(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadiusUsed)) ? 1 : 0,
                        FVector::Dist2D(CurrentLocation, FinalDestination), AcceptanceRadiusUsed,
                        PathFrag.HasValidPath() ? 1 : 0, PathFrag.bIsPathfindingInProgress ? 1 : 0,
                        *FinalDestination.ToString(), *CurrentLocation.ToString(), *Pred.Location.ToString(), *MoveTarget.Center.ToString());
                }
            }
        }
    });
}

void UUnitMovementProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FUnitNavigationPathFragment> PathList = ChunkContext.GetMutableFragmentView<FUnitNavigationPathFragment>();
        const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassSteeringFragment& Steering = SteeringList[i];
            Steering.DesiredVelocity = FVector::ZeroVector; // Default to stopped for this frame

            const FMassAIStateFragment& AIState = AIStateList[i];
            FMassMoveTargetFragment& MoveTarget = TargetList[i];

            if (const AActor* Actor = ActorList[i].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitMovementProcessor Server: %s - AIState.CanMove: %s, IsInitialized: %s, Target: %s, Speed: %f"), 
                        *Actor->GetName(), AIState.CanMove ? TEXT("True") : TEXT("False"), AIState.IsInitialized ? TEXT("True") : TEXT("False"), *MoveTarget.Center.ToString(), MoveTarget.DesiredSpeed.Get());
                }
            }

            if (!AIState.CanMove || !AIState.IsInitialized)
            {
                continue;
            }
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            FUnitNavigationPathFragment& PathFrag = PathList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            
            FVector CurrentLocation = CurrentMassTransform.GetLocation();
            FVector FinalDestination = MoveTarget.Center;
            const float DesiredSpeed = MoveTarget.DesiredSpeed.Get();
            const float AcceptanceRadius = MoveTarget.SlackRadius;

            if (CharFrag.bIsFlying)
            {
                if (FVector::DistSquared(CurrentLocation, FinalDestination) > FMath::Square(AcceptanceRadius))
                {
                    FVector MoveDir = (FinalDestination - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                }
                continue; 
            }
            
            if (const AUnitBase* UnitBase = Cast<AUnitBase>( ActorList[i].Get()))
            {
                if (const UCapsuleComponent* Capsule = UnitBase->GetCapsuleComponent())
                {
                    CurrentLocation.Z -= Capsule->GetScaledCapsuleHalfHeight()/2;
                }
            }
     
            if (FVector::DistSquared2D(CurrentLocation, FinalDestination) <= FMath::Square(AcceptanceRadius))
            {
                PathFrag.ResetPath();
                PathFrag.bIsPathfindingInProgress = false;
            }
            else if ((!PathFrag.HasValidPath() || PathFrag.PathTargetLocation != FinalDestination) && !PathFrag.bIsPathfindingInProgress)
            {
                PathFrag.bIsPathfindingInProgress = true;
                
                // WICHTIG: Wir speichern exakt das ab, was wir anfragen (UNPROJIZIERT)
                PathFrag.PathTargetLocation = FinalDestination;

                FNavLocation ProjectedStartLocation;
                if (NavSystem->ProjectPointToNavigation(CurrentLocation, ProjectedStartLocation, NavMeshProjectionExtent))
                {
                    // FIX A: Absolute Bremse sofort aktivieren, damit Momentum uns nicht in die Wand drückt!
                    Steering.DesiredVelocity = FVector::ZeroVector; 
                    
                    // Wir übergeben FinalDestination direkt an den Pathfinder!
                    RequestPathfindingAsync(Entity, ProjectedStartLocation.Location, FinalDestination);
                }
                else
                {
                    // GEÄNDERT: Kein Blindflug mehr! Wenn wir nicht auf dem Navmesh sind, bleiben wir stehen.
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    PathFrag.bIsPathfindingInProgress = false; 
                    PathFrag.ResetPath();
                }
            }
            else if (PathFrag.bIsPathfindingInProgress)
            {
            }
            else if (PathFrag.HasValidPath())
            {
                // NEU: Wenn das Navmesh unter uns neu generiert wurde (EnergyWall gebaut),
                // verwerfen wir den alten Pfad sofort, damit wir im nächsten Frame neu planen!
                if (!PathFrag.CurrentPath->IsValid())
                {
                    PathFrag.ResetPath();
                    Steering.DesiredVelocity = FVector::ZeroVector;
                    continue; 
                }

                const TArray<FNavPathPoint>& PathPoints = PathFrag.CurrentPath->GetPathPoints();

                // Monotonic projection-based advance (same logic as client; keeps server/client path-following
                // identical so their trajectories don't diverge extra — important since FOLLOW positions already
                // differ slightly between server and client).
                RTS_AdvancePathIndexByProjection(PathFrag, PathPoints, CurrentLocation);

                if (PathPoints.IsValidIndex(PathFrag.CurrentPathPointIndex))
                {
                    FVector MoveDir = (PathPoints[PathFrag.CurrentPathPointIndex].Location - CurrentLocation).GetSafeNormal();
                    Steering.DesiredVelocity = MoveDir * DesiredSpeed;
                }
                else
                {
                    PathFrag.ResetPath();
                }
            }
            else
            {
                 // GEÄNDERT: Auch auf dem Server darf eine Bodentruppe ohne Pfad nicht einfach stur geradeaus laufen.
                 Steering.DesiredVelocity = FVector::ZeroVector; 
            }
        }
    });
}


void UUnitMovementProcessor::RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation)
{
    UWorld* World = GetWorld();
    if (!World) return;

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (!NavSystem)
    {
        ResetPathfindingFlagDeferred(Entity);
        return;
    }

    ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
    if (!NavData)
    {
        ResetPathfindingFlagDeferred(Entity);
        return;
    }
    
    const bool bClientWorld = World->IsNetMode(NM_Client);

    if (!CachedStrictFilter.IsValid())
    {
        FSharedNavQueryFilter NewFilter = NavData->GetDefaultQueryFilter()->GetCopy();
        // GEÄNDERT: Exkludiere NUR die EnergyWall! Basis bleibt erreichbar.
        NewFilter->SetExcludedArea(NavData->GetAreaID(UNavArea_EnergyWall::StaticClass()));
        CachedStrictFilter = NewFilter;
    }
    
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [NavSystem, NavData, Entity, StartLocation, EndLocation, World, bClientWorld, StrictFilter = CachedStrictFilter] () mutable
    {
        // --- 1. STRICT MODE: Exklusion hartcodieren ---
        // Wir nutzen den gecachten Filter
        
        FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation, StrictFilter);
        
        // Erlaubt Partial Paths: Er plant bis zum Ufer, weil die Insel jetzt mathematisch "unendlich" weit weg ist.
        Query.SetAllowPartialPaths(true); 

        FPathFindingResult PathResult = NavSystem->FindPathSync(Query, EPathFindingMode::Regular);

        // --- 2. ESCAPE MODE FALLBACK ---
        if (PathResult.Result == ENavigationQueryResult::Fail)
        {
            // Inline Escape Filter (Erlaubt alles wieder)
            FSharedConstNavQueryFilter EscapeFilter = NavData->GetDefaultQueryFilter();
            
            FPathFindingQuery EscapeQuery(nullptr, *NavData, StartLocation, EndLocation, EscapeFilter);
            EscapeQuery.SetAllowPartialPaths(true);

            PathResult = NavSystem->FindPathSync(EscapeQuery, EPathFindingMode::Regular);

            if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 0)
            {
                const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
                
                // Auf was für einem Polygon STARTEN wir?
                bool bStartsInWall = false;
                
                for (int32 i = 0; i < PathPoints.Num(); ++i)
                {
                    if (PathPoints[i].HasNodeRef())
                    {
                        const UClass* AreaClass = NavData->GetAreaClass(PathPoints[i].NodeRef);
                        // GEÄNDERT: Prüfe auf EnergyWall
                        bStartsInWall = AreaClass && AreaClass->IsChildOf(UNavArea_EnergyWall::StaticClass());
                        break; 
                    }
                }

                if (!bStartsInWall)
                {
                    PathResult.Result = ENavigationQueryResult::Fail;
                    PathResult.Path->ResetForRepath();
                }
                else
                {
                    int32 CutOffIndex = -1;
                    for (int32 i = 0; i < PathPoints.Num(); ++i)
                    {
                        if (!PathPoints[i].HasNodeRef()) continue;
                        const UClass* AreaClass = NavData->GetAreaClass(PathPoints[i].NodeRef);
                        // GEÄNDERT: Prüfe auf EnergyWall
                        const bool bIsObstacle = AreaClass && AreaClass->IsChildOf(UNavArea_EnergyWall::StaticClass());

                        if (!bIsObstacle) {
                            // FIX B: ANTI-TUNNELING CHECK
                            float DistToSafeGround = FVector::Dist2D(StartLocation, PathPoints[i].Location);
                            if (DistToSafeGround > 120.0f) // Toleranzwert für Wanddicke
                            {
                                PathResult.Result = ENavigationQueryResult::Fail;
                                PathResult.Path->ResetForRepath();
                                CutOffIndex = -1; 
                                break;
                            }

                            CutOffIndex = i + 1; 
                            break;
                        }
                    }

                    if (CutOffIndex > 0 && CutOffIndex < PathPoints.Num())
                    {
                        TArray<FNavPathPoint>& MutablePoints = const_cast<TArray<FNavPathPoint>&>(PathResult.Path->GetPathPoints());
                        MutablePoints.SetNum(CutOffIndex);
                    }
                }
            }
        }
        
        AsyncTask(ENamedThreads::GameThread,
            [Entity, PathResult, World, EndLocation, bClientWorld]() mutable
        {
            if (!World) return;

            UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
            if (!EntitySubsystem) return;

            FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

            if (!EntityManager.IsEntityValid(Entity)) return;

            EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
                [Entity, PathResult, EndLocation, bClientWorld, World](FMassEntityManager& System)
                {
                    if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
                    {
                        PathFrag->bIsPathfindingInProgress = false;

                        const bool bTargetChanged = FVector::DistSquared2D(PathFrag->PathTargetLocation, EndLocation) > FMath::Square(10.f);
                        if (bTargetChanged)
                        {
                            // [PredStall] error-only: the path we just computed is DISCARDED because the target moved
                            // between request and return. If this fires every frame for a predicting unit, some other
                            // client processor is THRASHING Pred.Location/MoveTarget -> the unit never gets a usable
                            // path and stands. This is client-only (server has no Pred fragment).
                            if (bClientWorld)
                            {
                                if (const FMassClientPredictionFragment* PredDbg = System.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
                                {
                                    const float Since = World ? (World->GetTimeSeconds() - PredDbg->CommandPredictTime) : -1.f;
                                    if (PredDbg->bHasData && Since > 1.0f && Since < 1.4f)
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("[PredStall] TARGET-THRASH since=%.2f requested=%s nowTarget=%s pred=%s"),
                                            Since, *EndLocation.ToString(), *PathFrag->PathTargetLocation.ToString(), *PredDbg->Location.ToString());
                                    }
                                }
                            }
                            return;
                        }

                        if (PathResult.IsSuccessful() && PathResult.Path.IsValid() && PathResult.Path->GetPathPoints().Num() > 1)
                        {
                            PathFrag->CurrentPath = PathResult.Path;
                            PathFrag->CurrentPathPointIndex = 1;

                            const FVector PathEndLoc = PathResult.Path->GetEndLocation();
                            if (FVector::DistSquared(PathEndLoc, EndLocation) > FMath::Square(10.0f))
                            {
                                PathFrag->PathTargetLocation = PathEndLoc;
                                // CLIENT: MoveTarget.Center NICHT mit dem LOKALEN Pfad-Ende ueberschreiben.
                                // MoveTarget.Center ist der REPLIZIERTE autoritative Server-Endpunkt (Bubble TargetLoc,
                                // MassUnitReplicatorBase.cpp:357 -> ClientReplicationProcessor.cpp:358). Das clientseitige
                                // Pathfinding endet besonders an Cliffs/DirtyAreas/NavMesh-Loechern an einem ANDEREN Punkt
                                // als der Server (Partial Paths / abweichende Projektion). Setzt der Client damit seinen
                                // eigenen Endpunkt, jagt die Einheit ein anderes Ziel als der Server, "kommt nie an" und
                                // jittert im Stand. Der lokale Pfad steuert weiterhin via PathTargetLocation/CurrentPath;
                                // nur die Ziel-/Ankunftsreferenz (MoveTarget.Center) bleibt server-autoritativ.
                                if (!bClientWorld)
                                {
                                    if (FMassMoveTargetFragment* TargetFrag = System.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                                    {
                                        TargetFrag->Center = PathEndLoc;
                                    }
                                }
                                else
                                {
                                    // CLIENT FIX (partial path): the client mover steers to Pred.Location
                                    // (FinalDestination), NOT MoveTarget.Center. For a target that is NOT fully
                                    // reachable (gap / obstacle / off-nav / elevated) every path is PARTIAL and ends at
                                    // PathEndLoc != Pred.Location. The mover's re-request gate fires whenever
                                    // PathTargetLocation != FinalDestination, so the client re-requests EVERY frame
                                    // (pathing=1 forever), never reaches the follow-path branch, and stands still -
                                    // while the SERVER, which clamps MoveTarget.Center to PathEndLoc above, walks to the
                                    // navmesh edge. Mirror that clamp onto the PREDICTION only (MoveTarget.Center stays
                                    // server-authoritative): now PathTargetLocation == FinalDestination, the client
                                    // follows the partial path to the reachable edge and arrives, matching the server.
                                    if (FMassClientPredictionFragment* PredFrag = System.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
                                    {
                                        if (PredFrag->bHasData)
                                        {
                                            PredFrag->Location = PathEndLoc;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                PathFrag->PathTargetLocation = EndLocation;
                            }
                        }
                        else
                        {
                            // [PredStall] error-only: async path came back UNUSABLE (Fail / <=1 points) for a unit
                            // that is still actively predicting -> it re-requests next frame and keeps standing.
                            // result codes: 0=Invalid 1=Error 2=Fail 3=Success.
                            if (bClientWorld)
                            {
                                if (const FMassClientPredictionFragment* PredDbg = System.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
                                {
                                    const float Since = World ? (World->GetTimeSeconds() - PredDbg->CommandPredictTime) : -1.f;
                                    if (PredDbg->bHasData && Since > 1.0f && Since < 1.4f)
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("[PredStall] PATH-FAIL result=%d numPts=%d since=%.2f end=%s pred=%s"),
                                            (int32)PathResult.Result,
                                            (PathResult.Path.IsValid() ? PathResult.Path->GetPathPoints().Num() : -1),
                                            Since, *EndLocation.ToString(), *PredDbg->Location.ToString());
                                    }
                                }
                            }
                            PathFrag->ResetPath();
                        }
                    }
                });
        });
    });
}

// Create a new deferred version of your helper function
void UUnitMovementProcessor::ResetPathfindingFlagDeferred(FMassEntityHandle Entity)
{
     if (!EntitySubsystem) return; // Make sure EntitySubsystem is valid
     
     FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
     if (!EntityManager.IsEntityValid(Entity)) return;

     EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
         [Entity](FMassEntityManager& System)
         {
             if (FUnitNavigationPathFragment* PathFrag = System.GetFragmentDataPtr<FUnitNavigationPathFragment>(Entity))
             {
                 PathFrag->bIsPathfindingInProgress = false;
             }
         });
}