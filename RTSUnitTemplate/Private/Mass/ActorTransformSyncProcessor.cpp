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
        EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
        EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
    
        EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
        EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
        EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::Any); // Added to any group
        
        EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

        EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
        EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
        EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
		EntityQuery.RegisterWithProcessor(*this);

		ClientEntityQuery.Initialize(EntityManager);
		ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

        ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
        ClientEntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::Any); // Added to any group
            
        ClientEntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

        ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
        ClientEntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
    
        ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    /*
		ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	*/
		ClientEntityQuery.RegisterWithProcessor(*this);
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
    if (UnitBase->bUseSkeletalMovement || UnitBase->bUseIsmWithActorMovement)
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

            // Pitch-only Slope-Alignment: richte die Vorwärtsachse auf die Projektion auf der Bodenebene aus,
            // rotiere dabei ausschließlich um die Right-Achse (kein Roll), Yaw bleibt erhalten.
            const FVector SurfaceUp = Hit.ImpactNormal.GetSafeNormal();

            // Yaw-Only Basis aus aktueller Rotation
            const FRotator CurrentRot = MassTransform.GetRotation().Rotator();
            const FRotator YawOnlyRot(0.f, CurrentRot.Yaw, 0.f);
            const FQuat BaseYawQuat = YawOnlyRot.Quaternion();

            const FVector YawForward = BaseYawQuat.GetForwardVector(); // bereits ohne Pitch/Roll
            const FVector RightAxis = BaseYawQuat.GetRightVector();

            FVector SlopeForward = FVector::VectorPlaneProject(YawForward, SurfaceUp).GetSafeNormal();

            if (!SlopeForward.IsNearlyZero())
            {
                // Signierter Winkel um die Right-Achse zwischen YawForward und SlopeForward
                const FVector Cross = FVector::CrossProduct(YawForward, SlopeForward);
                const float Sin = FVector::DotProduct(Cross, RightAxis);
                const float Cos = FVector::DotProduct(YawForward, SlopeForward);
                const float PitchAngleRad = FMath::Atan2(Sin, Cos);

                const FQuat PitchQuat(RightAxis, PitchAngleRad);
                const FQuat DesiredQuat = PitchQuat * BaseYawQuat;

                // Interpolation zur Zielrotation (nur Pitch ändert sich)
                const float GroundSlopeRotationSpeedDegrees = 360.0f; // ggf. als UPROPERTY konfigurieren
                const FQuat NewRotQuat = FMath::QInterpConstantTo(
                    MassTransform.GetRotation(),
                    DesiredQuat,
                    ActualDeltaTime,
                    FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
                );
                MassTransform.SetRotation(NewRotQuat);
            }
        }
        else if (!CharFragment.bIsFlying) // Not on a valid ground hit, but not flying (e.g., walking off a ledge, or on another unit)
        {
            const float CurrentZ = CurrentActorLocation.Z;
            const float TargetZ = CharFragment.LastGroundLocation + HeightOffset;

            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);

            // Revert pitch and roll to zero (level) if not on valid ground
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f;
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
            );
            MassTransform.SetRotation(NewRotQuat);
        }
        else if (IsValid(HitActor) && CharFragment.bIsFlying) // Flying, but a hit occurred (e.g., flying over terrain)
        {
            const float CurrentZ = CurrentActorLocation.Z;
            const float TargetZ = Hit.ImpactPoint.Z + CharFragment.FlyHeight;
            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);
            CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;

            // For flying units, maintain flat pitch/roll unless specific flight controls dictate otherwise
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f; // Or a specific flying rotation speed
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
            );
            MassTransform.SetRotation(NewRotQuat);
        }
    }
    else // No ground hit detected (e.g., entirely in air)
    {
        if (CharFragment.bIsFlying)
        {
            InOutFinalLocation.Z = CharFragment.LastGroundLocation + CharFragment.FlyHeight;
        }
        else // Not flying and no ground hit (e.g., falling or airborne)
        {
            InOutFinalLocation.Z = CharFragment.LastGroundLocation + HeightOffset;

            // Revert pitch and roll to zero (level)
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f;
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
            );
            MassTransform.SetRotation(NewRotQuat);
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
            
            if (UnitBase->GetUnitState() != UnitData::Run && UnitBase->GetUnitState() != UnitData::GoToRepair && UnitBase->GetUnitState() != UnitData::Repair && UnitBase->GetUnitState() != UnitData::Casting)
            {
                UnitBase->SetUnitState(UnitData::Run);
            }
            DesiredQuat = LookAtDir.ToOrientationQuat();

            if (!UnitBase->bUseSkeletalMovement && !UnitBase->bUseIsmWithActorMovement)
                DesiredQuat *= UnitBase->MeshRotationOffset;
        }
    }else if (UnitBase->GetUnitState() == UnitData::Run)
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

void UActorTransformSyncProcessor::RotateTowardsTarget(AUnitBase* UnitBase, FMassEntityManager& EntityManager, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    // Proceed if we have either a resolved target entity OR a valid target flag with a non-zero last known location, and rotation to enemy is enabled
    const bool bHasUsableTarget = TargetFrag.TargetEntity.IsSet() || (TargetFrag.bHasValidTarget && !TargetFrag.LastKnownLocation.IsNearlyZero());
    if (!bHasUsableTarget || !Char.RotatesToEnemy)
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
    
    FQuat DesiredQuat = Dir.ToOrientationQuat();

    if (!UnitBase->bUseSkeletalMovement && !UnitBase->bUseIsmWithActorMovement)
        DesiredQuat *= UnitBase->MeshRotationOffset;
    
    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    
    const FQuat NewQuat = (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
        ? FMath::QInterpConstantTo(InOutMassTransform.GetRotation(), DesiredQuat, ActualDeltaTime, FMath::DegreesToRadians(RotationSpeedDeg))
        : DesiredQuat;
    
    InOutMassTransform.SetRotation(NewQuat);
}

bool UActorTransformSyncProcessor::RotateTowardsAbility(AUnitBase* UnitBase, const FMassAITargetFragment& AbilityTarget, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    // Calculate direction from the unit to the ability's target location
    FVector Dir = AbilityTarget.AbilityTargetLocation - CurrentActorLocation;
    Dir.Z = 0.f;  // Flatten to the XY plane for rotation

    if (!Dir.Normalize())
    {
        return true; // No direction to rotate => treat as reached to prevent stuck state
    }

    FQuat DesiredQuat = Dir.ToOrientationQuat();

    if (!UnitBase->bUseSkeletalMovement && !UnitBase->bUseIsmWithActorMovement)
    {
        DesiredQuat *= UnitBase->MeshRotationOffset;
    }

    // --- Interpolate rotation ---
    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    const FQuat CurrentQuat = InOutMassTransform.GetRotation();

    // Interpolate towards the desired rotation or set it instantly if speed is negligible
    const FQuat NewQuat = (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
        ? FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, ActualDeltaTime, FMath::DegreesToRadians(RotationSpeedDeg))
        : DesiredQuat;

    InOutMassTransform.SetRotation(NewQuat);

    // Determine if we reached the desired facing (yaw only)
    const float DesiredYaw = DesiredQuat.Rotator().Yaw;
    const float NewYaw = NewQuat.Rotator().Yaw;
    const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, DesiredYaw));
    const float ReachedToleranceDeg = 2.0f; // tolerance for considering rotation reached

    return YawDelta <= ReachedToleranceDeg;
}

void UActorTransformSyncProcessor::DispatchPendingUpdates(TArray<FActorTransformUpdatePayload>&& PendingUpdates)
{
    if (PendingUpdates.IsEmpty())
    {
        return;
    }

    const bool bLog = this->bShowLogs;
    AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingUpdates), bLog]()
    {
        // Periodic diagnostic: log batch size and a sample delta on clients
        bool bLogged = false;
        for (int32 idx = 0; idx < Updates.Num(); ++idx)
        {
            if (const AActor* SampleActor = Updates[idx].ActorPtr.Get())
            {
                if (UWorld* W = SampleActor->GetWorld())
                {
                    if (W->IsNetMode(NM_Client))
                    {
                        static int32 GActorSyncDispatchCounter = 0;
                        if (((++GActorSyncDispatchCounter) % 60) == 0)
                        {
                            if (bLog)
                            {
                                const FVector CurLoc = SampleActor->GetActorLocation();
                                const FVector NewLoc = Updates[idx].NewTransform.GetLocation();
                                const FRotator CurRot = SampleActor->GetActorRotation();
                                const FRotator NewRot = Updates[idx].NewTransform.Rotator();
                                UE_LOG(LogTemp, Warning, TEXT("[Client][ActorTransformSync] Dispatching %d updates. Example %s: Loc %s -> %s | Yaw %.1f -> %.1f | Skeletal=%d"),
                                    Updates.Num(), *SampleActor->GetName(), *CurLoc.ToString(), *NewLoc.ToString(), CurRot.Yaw, NewRot.Yaw, Updates[idx].bUseSkeletal ? 1 : 0);
                            }
                        }
                        bLogged = true;
                    }
                }
                break; // logged (or attempted), proceed to apply updates
            }
        }

        for (const FActorTransformUpdatePayload& Update : Updates)
        {
            if (AActor* Actor = Update.ActorPtr.Get())
            {
                if (Update.bUseSkeletal)
                {
                    Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::None);
                }
                else if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
                {
                    if (Unit->bUseIsmWithActorMovement)
                    {
                        Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::None);
                    }
                    else
                    {
                        Unit->Multicast_UpdateISMInstanceTransform(Update.InstanceIndex, Update.NewTransform);
                    }
                }
            }
        }
    });
}

void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
	{
	    //ExecuteRepClient(EntityManager, Context);
	    static int32 GActorSyncExecTickCounter = 0;
     if ((++GActorSyncExecTickCounter % 60) == 0)
     {
         if (bShowLogs)
         {
             UE_LOG(LogTemp, Warning, TEXT("[Client][ActorTransformSync] Execute tick"));
         }
     }
		ExecuteClient(EntityManager, Context);
	}
	else
	{
		ExecuteServer(EntityManager, Context);
	}
}

void UActorTransformSyncProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (!ShouldProceedWithTick(ActualDeltaTime)) return;

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(ClientEntityQuery.GetNumMatchingEntities());

    static int32 GActorSyncClientEnterCounter = 0;
    if ((++GActorSyncClientEnterCounter % 60) == 0)
    {
        if (bShowLogs)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Client][ActorTransformSync] ExecuteClient enter"));
        }
    }
    
    ClientEntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, ActualDeltaTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
            
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassRepresentationLODFragment> LODFragments = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Mirror server: skip updates for entities not visible
            if (LODFragments[i].LOD == EMassLOD::Off) continue;

            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            const FQuat CurrentRotation = MassTransform.GetRotation();
            FVector FinalLocation = MassTransform.GetLocation();
            
            // Determine the actor's current location once
            FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            // 1. Adjust height for ground snapping or flying
            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, FinalLocation);
            
            // 2. Adjust rotation based on state (moving vs. attacking)
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const bool bIsAttackingOrPaused = DoesEntityHaveTag(EntityManager, Entity, FMassStateAttackTag::StaticStruct()) ||
                                              DoesEntityHaveTag(EntityManager, Entity, FMassStatePauseTag::StaticStruct());
            const bool bIsFrozen = DoesEntityHaveTag(EntityManager, Entity, FMassStateStopMovementTag::StaticStruct());

            if (bIsFrozen)
            {
                // Skip rotation logic for frozen units; they maintain current Mass rotation
            }
            else if (TargetList[i].bRotateTowardsAbility)
            {
                const bool bReached = RotateTowardsAbility(UnitBase, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
                if (bReached)
                {
                    TargetList[i].bRotateTowardsAbility = false;
                }
            }
            else if (!bIsAttackingOrPaused)
            {
                RotateTowardsMovement(UnitBase, VelocityList[i].Value, StatsList[i], CharList[i], StateList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            else
            {
                RotateTowardsTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            
            // 3. Apply final location and cache the result
            MassTransform.SetLocation(FinalLocation);
            CharList[i].PositionedTransform = MassTransform;

            const bool bLocationChanged = !CurrentActorLocation.Equals(FinalLocation, 0.025f);
            const bool bRotationChanged = !CurrentRotation.Equals(MassTransform.GetRotation(), 0.025f);

            if (bLocationChanged || bRotationChanged)
            {
                PendingActorUpdates.Emplace(Actor, MassTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
            }else if (UnitBase->GetUnitState() == UnitData::Run)
            {
                    UnitBase->SetUnitState(UnitData::Idle);
            }
        }
    });

    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}


void UActorTransformSyncProcessor::ExecuteRepClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (!ShouldProceedWithTick(ActualDeltaTime)) return;

    const int32 TotalMatchingEntities = ClientEntityQuery.GetNumMatchingEntities();
    //UE_LOG(LogTemp, Warning, TEXT("[Client] UActorTransformSyncProcessor Matching=%d"), TotalMatchingEntities);
    if (TotalMatchingEntities == 0)
    {
        return;
    }

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(TotalMatchingEntities);

    ClientEntityQuery.ForEachEntityChunk(Context,
        [this, ActualDeltaTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassRepresentationLODFragment> LODFragments = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Only update if visible
            if (LODFragments[i].LOD == EMassLOD::Off) continue;

            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            const FTransform& ReplicatedTransform = TransformFragments[i].GetTransform();

            // Adjust with ground/height logic on client too
            FTransform LocalTransform = ReplicatedTransform;
            FVector FinalLocation = LocalTransform.GetLocation();
            const FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, LocalTransform, FinalLocation);

            LocalTransform.SetLocation(FinalLocation);

            PendingActorUpdates.Emplace(Actor, LocalTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
        }
    });

    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}

void UActorTransformSyncProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    
    // Determine if we are on a client world. This will be used to guard our logs.
    const bool bIsClient = GetWorld() && GetWorld()->IsNetMode(NM_Client);
    
    if (!ShouldProceedWithTick(ActualDeltaTime)) return;
    
    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(EntityQuery.GetNumMatchingEntities());
    
    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, ActualDeltaTime, &PendingActorUpdates, bIsClient](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
            
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            const FQuat CurrentRotation = MassTransform.GetRotation();
            FVector FinalLocation = MassTransform.GetLocation();
            
            // Determine the actor's current location once, as it's used by multiple functions
            FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            // 1. Adjust height for ground snapping or flying
            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, FinalLocation);
            
            // 2. Adjust rotation based on state (moving vs. attacking)
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const bool bIsAttackingOrPaused = DoesEntityHaveTag(EntityManager, Entity, FMassStateAttackTag::StaticStruct()) ||
                                              DoesEntityHaveTag(EntityManager, Entity, FMassStatePauseTag::StaticStruct());
            const bool bIsFrozen = DoesEntityHaveTag(EntityManager, Entity, FMassStateStopMovementTag::StaticStruct());

            if (bIsFrozen)
            {
                // Skip rotation logic for frozen units; they maintain current Mass rotation
            }
            else if (TargetList[i].bRotateTowardsAbility)
            {
                // Pass the consolidated AI Target fragment.
                const bool bReached = RotateTowardsAbility(UnitBase, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
                if (bReached)
                {
                    TargetList[i].bRotateTowardsAbility = false;
                }
            }
            else if (!bIsAttackingOrPaused && UnitBase->GetUnitState() != UnitData::Casting)
            {
                RotateTowardsMovement(UnitBase, VelocityList[i].Value, StatsList[i], CharList[i], StateList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            else
            {
                RotateTowardsTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            
            // 3. Apply final location and cache the result
            MassTransform.SetLocation(FinalLocation);
            CharList[i].PositionedTransform = MassTransform;

            const bool bLocationChanged = !CurrentActorLocation.Equals(FinalLocation, 0.025f);
            const bool bRotationChanged = !CurrentRotation.Equals(MassTransform.GetRotation(), 0.025f);
            // 4. Queue an update to be performed on the game thread if the transform has changed
         
            if (!bIsClient)
            {
                //UE_LOG(LogTemp, Error, TEXT("Server FinalLocation %s"), *FinalLocation.ToString());
            }

            if (bLocationChanged || bRotationChanged)
            {
                PendingActorUpdates.Emplace(Actor, MassTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
            }else if (UnitBase->GetUnitState() == UnitData::Run)
            {
                    UnitBase->SetUnitState(UnitData::Idle);
            }
            
        }
    });

    // 5. Asynchronously dispatch all queued updates to the game thread
    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}