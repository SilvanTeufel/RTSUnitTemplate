// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/ActorTransformSyncProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/Actor.h"
#include "Async/Async.h"
#include "NavigationSystem.h"

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    // Optional ExecutionOrder settings...
}

void UActorTransformSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
    
    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    // Still need LOD info to know if the actor should be visible/updated
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
    //EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    //EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this); // Important!

}


/**
 * @brief Checks if the processor should execute its main logic based on a dynamic tick rate.
 * @param ActualDeltaTime The delta time for the current frame.
 * @return True if the processor should execute, false otherwise.
 */
bool UActorTransformSyncProcessor::ShouldProceedWithTick(const float ActualDeltaTime)
{
    // --- Dynamic Tick Rate Calculation ---
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f);
    MinTickInterval = FMath::Max(0.001f, MinTickInterval);
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval);

    if (ActualDeltaTime <= 0.0f) return false;

    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;

    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold);
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, ActualDeltaTime);

    AccumulatedTimeA += ActualDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval)
    {
        return false;
    }

    AccumulatedTimeA = 0.0f;
    return true;
}

/**
 * @brief Adjusts the entity's vertical position to snap to the ground or maintain flying height.
 * @param UnitBase The unit actor, used for component and type info.
 * @param CharFragment The characteristics fragment, for flight data and storing ground location.
 * @param CurrentActorLocation The current world location of the actor/instance.
 * @param ActualDeltaTime The current frame's delta time, for interpolation.
 * @param MassTransform The entity's transform fragment, which will be updated with the correct scale.
 * @param InOutFinalLocation The location being calculated, its Z value will be modified by this function.
 */
void UActorTransformSyncProcessor::HandleGroundAndHeight(const AUnitBase* UnitBase, FMassAgentCharacteristicsFragment& CharFragment, const FVector& CurrentActorLocation, const float ActualDeltaTime, FTransform& MassTransform, FVector& InOutFinalLocation) const
{
    float HeightOffset;

    // Determine height offset and scale based on unit type
    if (UnitBase->bUseSkeletalMovement)
    {
        MassTransform.SetScale3D(UnitBase->GetActorScale3D());
        HeightOffset = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    else
    {
        const FTransform& ISMTransform = UnitBase->ISMComponent->GetComponentTransform();
        MassTransform.SetScale3D(ISMTransform.GetScale3D());
        HeightOffset = ISMTransform.GetScale3D().Z / 2.0f;
        if (UnitBase->MassActorBindingComponent->bAddCapsuleHalfHeightToIsm)
        {
            HeightOffset += UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }
    }

    // --- Ground/Height Adjustment Logic ---
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(UnitBase);
    FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);

    const FVector TraceStart = FVector(InOutFinalLocation.X, InOutFinalLocation.Y, InOutFinalLocation.Z + 1000.0f);
    const FVector TraceEnd = FVector(InOutFinalLocation.X, InOutFinalLocation.Y, InOutFinalLocation.Z - 2000.0f);

    if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
    {
        
        const AActor* HitActor = Hit.GetActor();
        const float DeltaZ = Hit.ImpactPoint.Z - CurrentActorLocation.Z;

        if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= (HeightOffset+100.f) && !CharFragment.bIsFlying) // && DeltaZ <= HeightOffset
        {
            CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;
            const float CurrentZ = CurrentActorLocation.Z;
            const float TargetZ = Hit.ImpactPoint.Z + HeightOffset;
            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);
        }
        else if (!CharFragment.bIsFlying)
        {
            const float CurrentZ = CurrentActorLocation.Z;
            const float TargetZ = CharFragment.LastGroundLocation + HeightOffset;
            
            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);
        }
        else if (IsValid(HitActor) && CharFragment.bIsFlying)
        {
            const float CurrentZ = CurrentActorLocation.Z;
            const float TargetZ = Hit.ImpactPoint.Z + CharFragment.FlyHeight;
            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);
            CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;
        }
    }
    else
    {
        if (CharFragment.bIsFlying)
        {
            InOutFinalLocation.Z = CharFragment.LastGroundLocation + CharFragment.FlyHeight;
        }
        else
        {
            InOutFinalLocation.Z = CharFragment.LastGroundLocation + HeightOffset;
        }
    }
}

void UActorTransformSyncProcessor::RotateTowardsMovement(AUnitBase* UnitBase, const FVector& CurrentVelocity, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FMassAIStateFragment& State, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    FQuat DesiredQuat = InOutMassTransform.GetRotation();

    if (Char.RotatesToMovement && !CurrentVelocity.IsNearlyZero(50.f) && !CurrentActorLocation.Equals(State.StoredLocation, UnitBase->StopRunTolerance))
    {
        FVector LookAtDir = CurrentVelocity;
        LookAtDir.Z = 0.f;
        if (LookAtDir.Normalize())
        {
            UnitBase->SetUnitState(UnitData::Run);
            DesiredQuat = LookAtDir.ToOrientationQuat();
        }
    }
    else if (UnitBase->GetUnitState() == UnitData::Run)
    {
        UnitBase->SetUnitState(UnitData::Idle);
    }

    const float RotationSpeedRad = FMath::DegreesToRadians(Stats.RotationSpeed * Char.RotationSpeed);
    FQuat NewQuat = InOutMassTransform.GetRotation();

    if (Char.RotatesToMovement)
    {
        NewQuat = (RotationSpeedRad > KINDA_SMALL_NUMBER)
            ? FMath::QInterpConstantTo(InOutMassTransform.GetRotation(), DesiredQuat, ActualDeltaTime, RotationSpeedRad)
            : DesiredQuat;
    }
    
    InOutMassTransform.SetRotation(NewQuat);
}

void UActorTransformSyncProcessor::RotateTowardsTarget(FMassEntityManager& EntityManager, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    if (!TargetFrag.TargetEntity.IsSet() || !Char.RotatesToEnemy)
    {
        return;
    }

    FVector TargetLocation = TargetFrag.LastKnownLocation;
    const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;
    if (EntityManager.IsEntityValid(TargetEntity))
    {
        if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity))
        {
            TargetLocation = TargetXform->GetTransform().GetLocation();
        }
    }

    FVector Dir = TargetLocation - CurrentActorLocation;
    Dir.Z = 0.f;
    if (!Dir.Normalize())
    {
        return;
    }
    
    const FQuat DesiredQuat = Dir.ToOrientationQuat();
    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    
    const FQuat NewQuat = (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
        ? FMath::QInterpConstantTo(InOutMassTransform.GetRotation(), DesiredQuat, ActualDeltaTime, FMath::DegreesToRadians(RotationSpeedDeg))
        : DesiredQuat;

    InOutMassTransform.SetRotation(NewQuat);
}

void UActorTransformSyncProcessor::RotateTowardsAbility(const FMassAITargetFragment& AbilityTarget, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    // Calculate direction from the unit to the ability's target location
    FVector Dir = AbilityTarget.AbilityTargetLocation - CurrentActorLocation;
    Dir.Z = 0.f;  // Flatten to the XY plane for rotation

    if (!Dir.Normalize())
    {
        return; // Avoid issues if the direction is zero
    }

    const FQuat DesiredQuat = Dir.ToOrientationQuat();

    // --- Interpolate rotation ---
    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    const FQuat CurrentQuat = InOutMassTransform.GetRotation();
    
    // Interpolate towards the desired rotation or set it instantly if speed is negligible
    const FQuat NewQuat = (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
        ? FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, ActualDeltaTime, FMath::DegreesToRadians(RotationSpeedDeg))
        : DesiredQuat;

    InOutMassTransform.SetRotation(NewQuat);
}

void UActorTransformSyncProcessor::DispatchPendingUpdates(TArray<FActorTransformUpdatePayload>&& PendingUpdates)
{
    if (PendingUpdates.IsEmpty())
    {
        return;
    }

    AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingUpdates)]()
    {
        for (const FActorTransformUpdatePayload& Update : Updates)
        {
            if (AActor* Actor = Update.ActorPtr.Get())
            {
                if (Update.bUseSkeletal)
                {
                    Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
                else if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
                {
                    Unit->Multicast_UpdateISMInstanceTransform(Update.InstanceIndex, Update.NewTransform);
                }
            }
        }
    });
}

void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (!ShouldProceedWithTick(ActualDeltaTime))
    {
        return;
    }

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(EntityQuery.GetNumMatchingEntities());

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, ActualDeltaTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            FVector FinalLocation = MassTransform.GetLocation();

            // Determine the actor's current location once, as it's used by multiple functions
            FVector CurrentActorLocation = UnitBase->GetActorLocation();
            if (!UnitBase->bUseSkeletalMovement)
            {
                FTransform InstanceTransform;
                UnitBase->ISMComponent->GetInstanceTransform(UnitBase->InstanceIndex, InstanceTransform, true);
                CurrentActorLocation = InstanceTransform.GetLocation();
            }

            // 1. Adjust height for ground snapping or flying
            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, FinalLocation);
            
            // 2. Adjust rotation based on state (moving vs. attacking)
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const bool bIsAttackingOrPaused = DoesEntityHaveTag(EntityManager, Entity, FMassStateAttackTag::StaticStruct()) ||
                                              DoesEntityHaveTag(EntityManager, Entity, FMassStatePauseTag::StaticStruct());

            if (UnitBase->ShouldRotateToAbilityClick())
            {
                // Pass the consolidated AI Target fragment.
                RotateTowardsAbility(TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }else if (!bIsAttackingOrPaused)
            {
                RotateTowardsMovement(UnitBase, VelocityList[i].Value, StatsList[i], CharList[i], StateList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            else
            {
                RotateTowardsTarget(EntityManager, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            
            // 3. Apply final location and cache the result
            MassTransform.SetLocation(FinalLocation);
            CharList[i].PositionedTransform = MassTransform;
            
            // 4. Queue an update to be performed on the game thread if the transform has changed
            if (!CurrentActorLocation.Equals(FinalLocation, 0.025f))
            {
                PendingActorUpdates.Emplace(Actor, MassTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
            }
        }
    });

    // 5. Asynchronously dispatch all queued updates to the game thread
    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}