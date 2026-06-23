// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitApplyMassMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "HAL/IConsoleManager.h"

// CLIENT-ONLY multiplier for the avoidance/separation/soft-avoidance Force before it is integrated into
// local motion. On the client the avoidance processors (UnitSeparationProcessor / UnitSoftAvoidanceProcessor /
// UnitMovingAvoidanceProcessor, all Server|Client|Standalone) run a FULL local sim whose Force fights the
// authoritative reconciler on the same FTransformFragment -> clump/corner/dirty-area jitter. Since the server's
// authoritative (already-separated) positions are replicated, the client doesn't need full local avoidance.
// <1 weakens it (smoother), 1=full, 0=disabled. Tunable live via console.
static TAutoConsoleVariable<float> CVarRTS_ClientAvoidanceForceScale(
	TEXT("net.RTS.Client.AvoidanceForceScale"),
	0.25f,
	TEXT("Client-only multiplier for avoidance Force before local integration. <1 reduces local avoidance that fights the reconciler (clump/corner/dirty-area jitter). 1=full, 0=disabled."),
	ECVF_Default);

UUnitApplyMassMovementProcessor::UUnitApplyMassMovementProcessor(): EntityQuery()
{

	//ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	//ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UUnitApplyMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly); 
	
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
	
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	
	EntityQuery.RegisterWithProcessor(*this);

	ClientEntityQuery.Initialize(EntityManager);
	ClientEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	//ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	ClientEntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	// Mirror relevant state tags on client to limit work to moving entities
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

void UUnitApplyMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());
    if (DeltaTime <= 0.0f)
    {
        return;
    }

	UWorld* World = GetWorld();
	if (!World) return;
	
    if (World->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UUnitApplyMassMovementProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());
    const float AvoidanceScale = CVarRTS_ClientAvoidanceForceScale.GetValueOnGameThread();

    ClientEntityQuery.ForEachEntityChunk(Context, [this, DeltaTime, AvoidanceScale](FMassExecutionContext& LocalContext)
    {
        const int32 NumEntities = LocalContext.GetNumEntities();
        if (NumEntities == 0) return;

        const FMassMovementParameters& MovementParams = LocalContext.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = LocalContext.GetFragmentView<FMassSteeringFragment>();
        const TArrayView<FTransformFragment> LocationList = LocalContext.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = LocalContext.GetMutableFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = LocalContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = LocalContext.GetFragmentView<FMassActorFragment>();

        const bool bFreezeXY = LocalContext.DoesArchetypeHaveTag<FMassStateStopXYMovementTag>();
        
        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
            const FMassAIStateFragment& AIState = AIStateList[EntityIndex];

            if (!AIState.CanMove)
            {
                Velocity.Value = FVector::ZeroVector;
                Force.Value = FVector::ZeroVector;
                continue;
            }

            if (const AActor* Actor = ActorList[EntityIndex].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                     if (!Velocity.Value.IsNearlyZero() || !Steering.DesiredVelocity.IsNearlyZero() || !Force.Value.IsNearlyZero())
                     {
                        UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitApplyMassMovementProcessor Client: %s - Velocity: %s, Steering: %s, Force: %s"), 
                            *Actor->GetName(), *Velocity.Value.ToString(), *Steering.DesiredVelocity.ToString(), *Force.Value.ToString());
                     }
                }
            }

            const float OriginalZVelocity = Velocity.Value.Z;
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const FVector AvoidanceForce = Force.Value;
            const float MaxSpeed = MovementParams.MaxSpeed;
            const float Acceleration = MovementParams.MaxAcceleration;

            const FVector CurrentHorizontalVelocity(Velocity.Value.X, Velocity.Value.Y, 0.f);
            const FVector DesiredHorizontalVelocity(DesiredVelocity.X, DesiredVelocity.Y, 0.f);
            // CLIENT: avoidance Force is weakened (AvoidanceScale) because the authoritative, already-separated
            // server positions are replicated & reconciled; full local avoidance would fight the reconciler and
            // cause clump/corner/dirty-area jitter. Steering toward the goal is unaffected.
            const FVector HorizontalAvoidanceForce = FVector(AvoidanceForce.X, AvoidanceForce.Y, 0.f) * AvoidanceScale;

            FVector AccelInput = (DesiredHorizontalVelocity - CurrentHorizontalVelocity);
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);
            FVector HorizontalVelocityDelta = (AccelInput + HorizontalAvoidanceForce) * DeltaTime * 4.f;

            FVector NewHorizontalVelocity = CurrentHorizontalVelocity + HorizontalVelocityDelta;
            NewHorizontalVelocity = NewHorizontalVelocity.GetClampedToMaxSize(MaxSpeed);

            if (bFreezeXY)
            {
                Velocity.Value = FVector(0.f, 0.f, OriginalZVelocity);
            }
            else
            {
                Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);
            }

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;

            UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(LocalContext.GetWorld());
            const FMassAgentCharacteristicsFragment& Characteristics = CharacteristicsList[EntityIndex];

            if (NavSys && !Characteristics.bIsFlying)
            {
                const FVector ProjectionExtent(Characteristics.CapsuleRadius * 4.0f, Characteristics.CapsuleRadius * 4.0f, SoftAvoidanceZExtent);

                FNavLocation ProjectedLocation;
                // Use capsule-based extent to detect if we are on the mesh or inside a DirtyArea
                bool bNeedsAvoidance = false;
                if (!NavSys->ProjectPointToNavigation(NewLocation, ProjectedLocation, ProjectionExtent) ||
                    FVector::DistSquared2D(NewLocation, ProjectedLocation.Location) > FMath::Square(5.f))
                {
                    bNeedsAvoidance = true;
                }
                else
                {
                    // NEU: Check, ob der projizierte Punkt in einer UNavArea_Obstacle (Energy Wall) liegt!
                    if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
                    {
                        const uint32 PolyAreaID = Recast->GetPolyAreaID(ProjectedLocation.NodeRef);
                        const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                        if (PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass()))
                        {
                            bNeedsAvoidance = true;
                        }
                    }
                }

                if (bNeedsAvoidance)
                {
                    LocalContext.Defer().AddTag<FMassSoftAvoidanceTag>(LocalContext.GetEntity(EntityIndex));
                }
            }

            CurrentTransform.SetTranslation(NewLocation);

            Force.Value = FVector::ZeroVector;
        }
    });
}

void UUnitApplyMassMovementProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

    EntityQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& LocalContext)
    {
        const int32 NumEntities = LocalContext.GetNumEntities();
        if (NumEntities == 0) return;

        const FMassMovementParameters& MovementParams = LocalContext.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = LocalContext.GetFragmentView<FMassSteeringFragment>();
        const TArrayView<FTransformFragment> LocationList = LocalContext.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = LocalContext.GetMutableFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = LocalContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = LocalContext.GetFragmentView<FMassActorFragment>();

        const bool bFreezeXY = LocalContext.DoesArchetypeHaveTag<FMassStateStopXYMovementTag>();

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
            const FMassAIStateFragment& AIState = AIStateList[EntityIndex];

            if (!AIState.CanMove)
            {
                Velocity.Value = FVector::ZeroVector;
                Force.Value = FVector::ZeroVector;
                continue;
            }

            if (const AActor* Actor = ActorList[EntityIndex].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                     if (!Velocity.Value.IsNearlyZero() || !Steering.DesiredVelocity.IsNearlyZero() || !Force.Value.IsNearlyZero())
                     {
                        UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitApplyMassMovementProcessor Server: %s - Velocity: %s, Steering: %s, Force: %s"), 
                            *Actor->GetName(), *Velocity.Value.ToString(), *Steering.DesiredVelocity.ToString(), *Force.Value.ToString());
                     }
                }
            }

            const float OriginalZVelocity = Velocity.Value.Z;
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const FVector AvoidanceForce = Force.Value;
            const float MaxSpeed = MovementParams.MaxSpeed;
            const float Acceleration = MovementParams.MaxAcceleration;

            const FVector CurrentHorizontalVelocity(Velocity.Value.X, Velocity.Value.Y, 0.f);
            const FVector DesiredHorizontalVelocity(DesiredVelocity.X, DesiredVelocity.Y, 0.f);
            const FVector HorizontalAvoidanceForce(AvoidanceForce.X, AvoidanceForce.Y, 0.f);

            FVector AccelInput = (DesiredHorizontalVelocity - CurrentHorizontalVelocity);
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);
            FVector HorizontalVelocityDelta = (AccelInput + HorizontalAvoidanceForce) * DeltaTime * 4.f;

            FVector NewHorizontalVelocity = CurrentHorizontalVelocity + HorizontalVelocityDelta;
            NewHorizontalVelocity = NewHorizontalVelocity.GetClampedToMaxSize(MaxSpeed);

            if (bFreezeXY)
            {
                Velocity.Value = FVector(0.f, 0.f, OriginalZVelocity);
            }
            else
            {
                Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);
            }

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;

            UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(LocalContext.GetWorld());
            const FMassAgentCharacteristicsFragment& Characteristics = CharacteristicsList[EntityIndex];

            if (NavSys && !Characteristics.bIsFlying)
            {
                const FVector ProjectionExtent(Characteristics.CapsuleRadius * 4.0f, Characteristics.CapsuleRadius * 4.0f, SoftAvoidanceZExtent);

                FNavLocation ProjectedLocation;
                // Use capsule-based extent to detect if we are on the mesh or inside a DirtyArea
                bool bNeedsAvoidance = false;
                if (!NavSys->ProjectPointToNavigation(NewLocation, ProjectedLocation, ProjectionExtent) || 
                    FVector::DistSquared2D(NewLocation, ProjectedLocation.Location) > FMath::Square(5.f))
                {
                    bNeedsAvoidance = true;
                }
                else
                {
                    // NEU: Check, ob der projizierte Punkt in einer UNavArea_Obstacle (Energy Wall) liegt!
                    if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
                    {
                        const uint32 PolyAreaID = Recast->GetPolyAreaID(ProjectedLocation.NodeRef);
                        const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                        if (PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass()))
                        {
                            bNeedsAvoidance = true;
                        }
                    }
                }

                if (bNeedsAvoidance)
                {
                    LocalContext.Defer().AddTag<FMassSoftAvoidanceTag>(LocalContext.GetEntity(EntityIndex));
                }
            }

            CurrentTransform.SetTranslation(NewLocation);

            Force.Value = FVector::ZeroVector;
        }
    });
}