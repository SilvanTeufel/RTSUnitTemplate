// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitApplyMassMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

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

	EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
	
	EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	
	EntityQuery.RegisterWithProcessor(*this);

	ClientEntityQuery.Initialize(EntityManager);
	ClientEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
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

	ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
	
	ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
	ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	
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
        static int32 GApplyMoveExecTickCounter = 0;
        if ((++GApplyMoveExecTickCounter % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][ApplyMassMovement] Execute tick"));
            }
        }
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
   
    ClientEntityQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& LocalContext)
    {
        const int32 NumEntities = LocalContext.GetNumEntities();
        if (NumEntities == 0) return;

        const FMassMovementParameters& MovementParams = LocalContext.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = LocalContext.GetFragmentView<FMassSteeringFragment>();
        const TArrayView<FTransformFragment> LocationList = LocalContext.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = LocalContext.GetMutableFragmentView<FMassVelocityFragment>();


        static int32 GApplyMoveClientChunkCounter = 0;
        if (((++GApplyMoveClientChunkCounter) % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][ApplyMassMovement] ExecuteClient: Entities=%d"), NumEntities);
            }
        }
        
        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();

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

            Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            const FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;
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

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();

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

            Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            const FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;
            CurrentTransform.SetTranslation(NewLocation);

            Force.Value = FVector::ZeroVector;
        }
    });
}