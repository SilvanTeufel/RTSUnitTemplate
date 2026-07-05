// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitVisualTweenProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Actors/WorkArea.h"
#include "MassNavigationFragments.h"
#include <type_traits>

UMassUnitVisualTweenProcessor::UMassUnitVisualTweenProcessor() {
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    bLogDebug = false;
}

void UMassUnitVisualTweenProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) {
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisualTweenFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.RegisterWithProcessor(*this);
}

void UMassUnitVisualTweenProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    static double LastLogTime = 0.0;
    static int32 LoggedThisFrame = 0;
    double CurrentTime = FPlatformTime::Seconds();
    bool bShouldLog = bLogDebug && (CurrentTime - LastLogTime) > 0.5;
    if (bShouldLog) {
        LastLogTime = CurrentTime;
        LoggedThisFrame = 0;
    }

    EntityQuery.ForEachEntityChunk(Context, ([this, &EntityManager, DeltaTime, bShouldLog](FMassExecutionContext& Context) {
        TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TArrayView<FMassVisualTweenFragment> TweenList = Context.GetMutableFragmentView<FMassVisualTweenFragment>();
        TArrayView<FMassVisualEffectFragment> EffectList = Context.GetMutableFragmentView<FMassVisualEffectFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        TConstArrayView<FMassAITargetFragment> TargetList = Context.GetFragmentView<FMassAITargetFragment>();
        bool bHasTargetFrag = (TargetList.Num() > 0);

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& Visual = VisualList[i];
            FMassVisualTweenFragment& Tween = TweenList[i];
            FMassVisualEffectFragment& Effect = EffectList[i];
            const FTransform& EntityTransform = TransformList[i].GetTransform();

            // 1. Process Tweens
            auto UpdateTween = [DeltaTime](FMassVisualTweenState& State) {
                if (State.bActive) {
                    State.Elapsed += DeltaTime;
                    if (State.Elapsed >= State.Duration) {
                        State.Elapsed = FMath::Max(State.Duration, 0.0001f);
                        State.bActive = false;
                    }
                }
            };

            auto GetTweenAlpha = [](const FMassVisualTweenState& State) -> float {
                float Alpha = FMath::Clamp(State.Elapsed / State.Duration, 0.f, 1.f);
                if (!FMath::IsNearlyEqual(State.EaseExp, 1.f)) {
                    Alpha = FMath::Pow(Alpha, State.EaseExp);
                }
                return Alpha;
            };

            UpdateTween(Tween.RotationTween);
            UpdateTween(Tween.LocationTween);
            UpdateTween(Tween.ScaleTween);

            float RotAlpha = GetTweenAlpha(Tween.RotationTween);
            float LocAlpha = GetTweenAlpha(Tween.LocationTween);
            float ScaleAlpha = GetTweenAlpha(Tween.ScaleTween);

            FQuat CurrentRotTween = FQuat::Slerp(Tween.RotationTween.StartRotation, Tween.RotationTween.TargetRotation, RotAlpha).GetNormalized();
            FVector CurrentLocTween = FMath::Lerp(Tween.LocationTween.StartLocation, Tween.LocationTween.TargetLocation, LocAlpha);
            FVector CurrentScaleTween = FMath::Lerp(Tween.ScaleTween.StartScale, Tween.ScaleTween.TargetScale, ScaleAlpha);

            // 2. Process Effects
            // Pulsate
            FVector CurrentPulsateScale = FVector::OneVector;
            if (Effect.bPulsateEnabled) {
                Effect.PulsateElapsed += DeltaTime;
                float PulsateAlpha = 0.5f * (1.f - FMath::Cos(PI * Effect.PulsateElapsed / Effect.PulsateHalfPeriod));
                CurrentPulsateScale = FMath::Lerp(Effect.PulsateMinScale, Effect.PulsateMaxScale, PulsateAlpha);
                
                if (bShouldLog && LoggedThisFrame < 10 && Effect.PulsateTargetISM.IsValid()) {
                    LoggedThisFrame++;
                }
            }

            // Continuous Rotation (Total Rotation since start)
            FQuat TotalRotation = FQuat::Identity;
            if (Effect.bRotationEnabled) {
                Effect.RotationElapsed += DeltaTime;
                float TotalAngle = Effect.RotationDegreesPerSecond * Effect.RotationElapsed;
                TotalRotation = FQuat(Effect.RotationAxis, FMath::DegreesToRadians(TotalAngle));
                
                if (Effect.RotationDuration > 0.f && Effect.RotationElapsed >= Effect.RotationDuration) {
                    Effect.bRotationEnabled = false;
                }
                
                if (bShouldLog && LoggedThisFrame < 10 && Effect.RotationTargetISM.IsValid()) {
                    LoggedThisFrame++;
                }
            }

            // Oscillation
            FVector CurrentOscOffset = FVector::ZeroVector;
            if (Effect.bOscillationEnabled) {
                Effect.OscillationElapsed += DeltaTime;
                float Omega = 2.f * PI * Effect.OscillationCyclesPerSecond;
                float OscAlpha = 0.5f * (1.f - FMath::Cos(Omega * Effect.OscillationElapsed));
                CurrentOscOffset = FMath::Lerp(Effect.OscillationOffsetA, Effect.OscillationOffsetB, OscAlpha);
                
                if (Effect.OscillationDuration > 0.f && Effect.OscillationElapsed >= Effect.OscillationDuration) {
                    Effect.bOscillationEnabled = false;
                }

                if (bShouldLog && LoggedThisFrame < 10 && Effect.OscillationTargetISM.IsValid()) {
                    LoggedThisFrame++;
                }
            }

            // Dish Rotation
            FQuat DishRotation = FQuat::Identity;
            if (Effect.bDishRotationEnabled) {
                if (Effect.DishTimeRemaining <= 0.f) {
                    Effect.DishCurrentSpeed = FMath::FRandRange(Effect.DishSpeedMin, Effect.DishSpeedMax);
                    if (FMath::RandBool()) Effect.DishCurrentSpeed *= -1.f;
                    Effect.DishTimeRemaining = FMath::FRandRange(Effect.DishDurationMin, Effect.DishDurationMax);
                }
                Effect.DishTimeRemaining -= DeltaTime;
                Effect.DishAccumulatedAngle += Effect.DishCurrentSpeed * DeltaTime;
                DishRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(Effect.DishAccumulatedAngle));
            }

            // Yaw-to-Chase
            FRotator TargetYawRot = FRotator::ZeroRotator;
            bool bHasYawChaseTarget = false;
            
            FMassEntityHandle TargetEntity = Effect.TargetEntity;
            if (!TargetEntity.IsValid() && bHasTargetFrag) {
                TargetEntity = TargetList[i].TargetEntity;
            }

            if (Effect.bYawChaseEnabled && EntityManager.IsEntityActive(TargetEntity)) {
                const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
                if (TargetTransformFrag) {
                    FVector ToTarget = TargetTransformFrag->GetTransform().GetLocation() - EntityTransform.GetLocation();
                    ToTarget.Z = 0.f;
                    if (!ToTarget.IsNearlyZero()) {
                        FRotator TargetRot = FRotationMatrix::MakeFromX(ToTarget.GetSafeNormal()).Rotator();
                        TargetRot.Yaw += Effect.YawChaseOffset;
                        // Correct relative rotation calculation: ParentInv * World
                        TargetYawRot = (EntityTransform.GetRotation().Inverse() * TargetRot.Quaternion()).Rotator();
                        bHasYawChaseTarget = true;
                    }
                }
            }

            // 2.5 Drone Simulation
            if (Effect.bDroneEnabled) {
                float DroneDeltaTime = Context.GetDeltaTimeSeconds();

                // Build progress (0..1) of this drone's construction site. Used to let the vertical scan
                // band follow the build front (buildings rise bottom-up) and to trigger the despawn.
                const AConstructionUnit* DroneCU = Cast<AConstructionUnit>(ActorList[i].Get());
                float DroneBuildProgress = 1.f;
                if (DroneCU && DroneCU->WorkArea && DroneCU->WorkArea->BuildTime > 0.f) {
                    DroneBuildProgress = FMath::Clamp(DroneCU->WorkArea->CurrentBuildTime / DroneCU->WorkArea->BuildTime, 0.f, 1.f);
                }
                // Scan ceiling (max scan height) stepped by which THIRD of the build TIME we are in:
                //   first third  (0 .. 1/3)  -> Third1 * H
                //   second third (1/3 .. 2/3)-> Third2 * H
                //   last third   (2/3 .. 1)  -> Third3 * H
                // The Stage-3 bounce peak sits at this ceiling and oscillates downward toward SafeMin.
                float DroneCeilFrac = DroneThird3HeightFraction;
                if (bDroneFollowBuildProgress) {
                    DroneCeilFrac = (DroneBuildProgress < (1.f / 3.f)) ? DroneThird1HeightFraction
                                  : (DroneBuildProgress < (2.f / 3.f)) ? DroneThird2HeightFraction
                                  :                                      DroneThird3HeightFraction;
                }
                const float DroneScanCeiling = FMath::Max(DroneTargetHeightMin, Effect.DroneBuildingHeight * DroneCeilFrac);

                // Stage Transition Log
                if (Effect.DroneStage != Effect.DroneLastStage) {
                    Effect.DroneTimer = 0.f;
                    Effect.DroneLastStage = Effect.DroneStage;
                    if (Effect.DroneStage == 0) {
                        Effect.DroneOffset = FVector(Effect.DroneOrbitRadius.X, 0.f, Effect.DroneSpawnHeight);
                        Effect.DroneRotation = FRotator(Effect.DroneSpawnPitch, 0.f, 0.f).Quaternion();
                        Effect.DroneVisualOffset = Effect.DroneOffset;
                        Effect.DroneVisualRotation = Effect.DroneRotation;
                        Effect.DroneCurrentAngle = 0.f;
                    }
                    else if (Effect.DroneStage == 3) {
                        const float SafeMin = FMath::Max(DroneMinHeightAbs, Effect.DroneBuildingHeight * DroneMinHeightFraction);
                        Effect.DroneBounceCycles   = FMath::FRandRange(DroneBounceCyclesMin, DroneBounceCyclesMax);
                        Effect.DroneBounceDuration = FMath::FRandRange(DroneBounceDurationMin, DroneBounceDurationMax);

                        // The bounce PEAK follows the build front (DroneScanCeiling, which rises with build
                        // progress): up to the middle at mid-build, up to the top at the end. The drone
                        // oscillates DOWNWARD from that peak toward SafeMin, scanning the just-built band.
                        // Amplitude spans a fraction of [SafeMin, BuildFront]; base is set so peak == BuildFront.
                        const float BuildFront = FMath::Max(SafeMin, DroneScanCeiling);
                        const float Amp = FMath::FRandRange(DroneBounceAmplitudeMinFraction, DroneBounceAmplitudeMaxFraction) * (BuildFront - SafeMin);
                        Effect.DroneBounceAmplitude  = Amp;
                        Effect.DroneBounceBaseHeight = BuildFront - Amp;             // peak (base + Amp) == build front
                        Effect.DroneTargetHeight     = Effect.DroneBounceBaseHeight; // settle at the bounce center
                    }
                }

                switch (Effect.DroneStage) {
                case 0: // Spawn / Fly down
                {
                    Effect.DroneOffset.Z = FMath::FInterpTo(Effect.DroneOffset.Z, Effect.DroneTargetHeight, DroneDeltaTime, Effect.DroneInterpSpeed);
                    FRotator CurrentRot = Effect.DroneRotation.Rotator();
                    CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, 90.f, DroneDeltaTime, Effect.DroneRotationSpeed);
                    Effect.DroneRotation = CurrentRot.Quaternion();

                    if (Effect.DroneTimer >= DroneFlyDownDuration) {
                        Effect.DroneStage = 1;
                        Effect.DroneTargetAngle = FMath::FRandRange(DroneOrbitAngleMin, DroneOrbitAngleMax);
                    }
                }
                break;
                case 1: // Orbit
                {
                    Effect.DroneCurrentAngle += Effect.DroneOrbitSpeed * DroneDeltaTime;
                    float Rad = FMath::DegreesToRadians(Effect.DroneCurrentAngle);
                    
                    FVector TargetOffset = Effect.DroneOffset;
                    TargetOffset.X = FMath::Cos(Rad) * Effect.DroneOrbitRadius.X;
                    TargetOffset.Y = FMath::Sin(Rad) * Effect.DroneOrbitRadius.Y;
                    
                    // Use interpolation for smoother movement during orbit
                    Effect.DroneOffset = FMath::VInterpTo(Effect.DroneOffset, TargetOffset, DroneDeltaTime, Effect.DroneInterpSpeed);

                    // Compute world rotation relative to building
                    // We want the drone to face "forward" in its orbit
                    FRotator TargetRot = FRotator(0.f, Effect.DroneCurrentAngle + 90.f, 0.f);
                    
                    // DroneRotation is relative to Actor.
                    Effect.DroneRotation = FMath::QInterpTo(Effect.DroneRotation, TargetRot.Quaternion(), DroneDeltaTime, Effect.DroneRotationSpeed);

                    float EstimatedTime = Effect.DroneTargetAngle / Effect.DroneOrbitSpeed;
                    if (Effect.DroneTimer >= EstimatedTime) {
                        Effect.DroneStage = 2;
                    }
                }
                break;
                case 2: // Focus
                {
                    FVector Direction = -Effect.DroneOffset;
                    Direction.Z = 0.f;
                    FRotator TargetRot = Direction.Rotation();
                    Effect.DroneRotation = FMath::QInterpTo(Effect.DroneRotation, TargetRot.Quaternion(), DroneDeltaTime, Effect.DroneRotationSpeed);

                    if (Effect.DroneTimer >= DroneFocusDuration) {
                        Effect.DroneStage = 3;
                        // The scan height (bounce peak + settle) is derived from the build front on Stage-3 entry.
                    }
                }
                break;
                case 3: // Vertical Move — bounce up/down a few times, then settle at a random height
                {
                    const float SafeMinHeight = FMath::Max(DroneMinHeightAbs, Effect.DroneBuildingHeight * DroneMinHeightFraction);

                    if (Effect.DroneTimer < Effect.DroneBounceDuration)
                    {
                        // Smooth sine bounce around the base height. Lowest point clamped above ground.
                        const float Omega = 2.f * PI * Effect.DroneBounceCycles / FMath::Max(0.001f, Effect.DroneBounceDuration);
                        const float BounceZ = Effect.DroneBounceBaseHeight + Effect.DroneBounceAmplitude * FMath::Sin(Omega * Effect.DroneTimer);
                        Effect.DroneOffset.Z = FMath::Max(BounceZ, SafeMinHeight);
                    }
                    else
                    {
                        // Bounces finished: settle smoothly at the random target height (kept above ground).
                        const float SettleZ = FMath::Max(Effect.DroneTargetHeight, SafeMinHeight);
                        Effect.DroneOffset.Z = FMath::FInterpTo(Effect.DroneOffset.Z, SettleZ, DroneDeltaTime, Effect.DroneInterpSpeed);
                    }

                    FVector Direction = -Effect.DroneOffset;
                    Direction.Z = 0.f;
                    FRotator TargetRot = Direction.Rotation();
                    Effect.DroneRotation = FMath::QInterpTo(Effect.DroneRotation, TargetRot.Quaternion(), DroneDeltaTime, Effect.DroneRotationSpeed);

                    // Transition only after the bounces AND a short settle window. Near the end of the
                    // build, DON'T start another reorient/orbit loop (Stage 4 -> 1): stay in the vertical
                    // move (keep hovering over the site, "weiterlaufen") so the drone doesn't fly off on a
                    // fresh orbit right before finishing — the autonomous despawn check below then lifts it
                    // straight into Stage 5 once DroneDespawnBuildFraction is reached.
                    if (Effect.DroneTimer >= Effect.DroneBounceDuration + DroneSettleWindow &&
                        DroneBuildProgress < DroneSkipReorientBuildFraction) {
                        Effect.DroneStage = 4;
                    }
                }
                break;
                case 4: // Reorient
                {
                    FRotator TargetRot = FRotator(0.f, Effect.DroneCurrentAngle + 90.f, 0.f);
                    Effect.DroneRotation = FMath::QInterpTo(Effect.DroneRotation, TargetRot.Quaternion(), DroneDeltaTime, Effect.DroneRotationSpeed);

                    if (Effect.DroneTimer >= DroneReorientDuration) {
                        Effect.DroneStage = 1;
                        Effect.DroneTargetAngle = FMath::FRandRange(DroneOrbitAngleMin, DroneOrbitAngleMax);
                    }
                }
                break;
                case 5: // Despawn — rise upward and fly out of sight
                {
                    // Interpolate toward a FIXED, high target Z so the remaining distance stays large and
                    // the ascent keeps moving (recomputing a target just above the current Z would make
                    // VInterpTo crawl). Speed / rise-height / duration are editable on the processor so the
                    // exit can be tuned slower (DroneDespawnAscentSpeed) and started later
                    // (DroneDespawnBuildFraction).
                    const float DespawnTargetZ = Effect.DroneSpawnHeight + DroneDespawnRiseHeight; // far above the scene
                    Effect.DroneOffset.Z = FMath::FInterpTo(Effect.DroneOffset.Z, DespawnTargetZ, DroneDeltaTime, DroneDespawnAscentSpeed);

                    FRotator CurrentRot = Effect.DroneRotation.Rotator();
                    CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, 90.f, DroneDeltaTime, Effect.DroneRotationSpeed + DroneDespawnPitchSpeedBonus);
                    Effect.DroneRotation = CurrentRot.Quaternion();

                    if (Effect.DroneTimer >= DroneDespawnDuration) {
                        Effect.bDroneEnabled = false;
                    }
                }
                break;
                }

                // Autonome Despawn Erkennung. This is the authoritative trigger for the VISIBLE fly-up
                // (Effect.DroneStage is never synced from the actor's Rep_DroneStage; CurrentBuildTime is
                // replicated so this runs on every machine). Raise DroneDespawnBuildFraction to make the
                // drone leave later.
                if (Effect.DroneStage < 5 && DroneCU && DroneCU->WorkArea && DroneCU->WorkArea->BuildTime > 0.f &&
                    DroneBuildProgress > DroneDespawnBuildFraction) {
                    Effect.DroneStage = 5;
                }

                // Interpolate visual representation to match the simulated authoritative state
                // This smooths out any jitter from replication or physics-sync
                if (Effect.DroneVisualOffset.IsZero()) {
                    Effect.DroneVisualOffset = Effect.DroneOffset;
                    Effect.DroneVisualRotation = Effect.DroneRotation;
                } else {
                    Effect.DroneVisualOffset = FMath::VInterpTo(Effect.DroneVisualOffset, Effect.DroneOffset, DroneDeltaTime, Effect.DroneInterpSpeed);
                    Effect.DroneVisualRotation = FMath::QInterpTo(Effect.DroneVisualRotation, Effect.DroneRotation, DroneDeltaTime, Effect.DroneRotationSpeed);
                }

                Effect.DroneTimer += DroneDeltaTime;
            }

            // 3. Apply everything to instances
            for (int32 InstanceIdx = 0; InstanceIdx < Visual.VisualInstances.Num(); ++InstanceIdx) {
                FMassUnitVisualInstance& Instance = Visual.VisualInstances[InstanceIdx];
                UInstancedStaticMeshComponent* InstanceTemplate = Instance.TemplateISM.Get();
                if (!InstanceTemplate) continue;

                // Start with the base offset from the Blueprint/Fragment
                FTransform NewTransform = Instance.BaseOffset;

                // --- Apply Tweens as Overrides ---
                // Rotation
                if (Tween.RotationTween.bActive || Tween.RotationTween.Elapsed > 0.f) {
                    bool bShouldApply = (!Tween.RotationTween.TargetISM.IsValid() || Tween.RotationTween.TargetISM == InstanceTemplate);
                    if (bShouldApply) {
                         NewTransform.SetRotation(CurrentRotTween);
                    }
                }
                // Location
                if (Tween.LocationTween.bActive || Tween.LocationTween.Elapsed > 0.f) {
                    bool bShouldApply = (!Tween.LocationTween.TargetISM.IsValid() || Tween.LocationTween.TargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        NewTransform.SetLocation(CurrentLocTween);
                    }
                }
                // Scale
                if (Tween.ScaleTween.bActive || Tween.ScaleTween.Elapsed > 0.f) {
                    bool bShouldApply = (!Tween.ScaleTween.TargetISM.IsValid() || Tween.ScaleTween.TargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        NewTransform.SetScale3D(CurrentScaleTween);
                    }
                }

                // --- Apply Effects ---
                // Pulsate (Pulsate values are usually absolute scales)
                if (Effect.bPulsateEnabled) {
                    bool bShouldApply = (!Effect.PulsateTargetISM.IsValid() || Effect.PulsateTargetISM == InstanceTemplate);
                    if (bShouldApply) {
                         NewTransform.SetScale3D(CurrentPulsateScale);
                    }
                }

                // Continuous Rotation (Combine with base rotation)
                if (Effect.bRotationEnabled) {
                    bool bShouldApply = (!Effect.RotationTargetISM.IsValid() || Effect.RotationTargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        NewTransform.SetRotation(TotalRotation * NewTransform.GetRotation());
                    }
                }

                // Oscillation (Additively to location)
                if (Effect.bOscillationEnabled) {
                    bool bShouldApply = (!Effect.OscillationTargetISM.IsValid() || Effect.OscillationTargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        NewTransform.AddToTranslation(CurrentOscOffset);
                    }
                }

                // Dish Rotation
                if (Effect.bDishRotationEnabled) {
                    bool bShouldApply = (!Effect.DishTargetISM.IsValid() || Effect.DishTargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        NewTransform.SetRotation(DishRotation * NewTransform.GetRotation());
                    }
                }

                // Yaw-to-Chase (Slerp towards target relative rotation)
                if (Effect.bYawChaseEnabled && bHasYawChaseTarget) {
                    bool bShouldApply = (!Effect.YawChaseTargetISM.IsValid() || Effect.YawChaseTargetISM == InstanceTemplate);
                    if (bShouldApply) {
                        FQuat TargetQuat = TargetYawRot.Quaternion();
                        if (Effect.bYawChaseTeleport || Effect.YawChaseDuration <= 0.f) {
                            NewTransform.SetRotation(TargetQuat);
                        } else {
                            // Slerp from CURRENT rotation (previous frame) to TARGET rotation
                            FQuat StartQuat = Instance.CurrentRelativeTransform.GetRotation();
                            float Alpha = FMath::Clamp(DeltaTime / Effect.YawChaseDuration, 0.f, 1.f);
                            if (!FMath::IsNearlyEqual(Effect.YawChaseEaseExp, 1.f)) {
                                Alpha = FMath::Pow(Alpha, Effect.YawChaseEaseExp);
                            }
                            NewTransform.SetRotation(FQuat::Slerp(StartQuat, TargetQuat, Alpha).GetNormalized());
                        }
                    }
                }

                // Drone Behavior
                if (Effect.bDroneEnabled) {
                    const bool bIsDronePlane =
                        Effect.DronePlaneTemplateISM.IsValid() &&
                        Instance.TemplateISM == Effect.DronePlaneTemplateISM;

                    if (bIsDronePlane) {
                        // The DronePlane projection is a rigid child of the drone: compose its relative
                        // Blueprint offset UNDERNEATH the drone's simulated transform (transform
                        // multiplication, matching how the placement processor later applies the entity
                        // transform: FinalTransform = CurrentRelativeTransform * BaseTransform). This keeps
                        // the plane at its relative offset AND rotates/scales that offset with the drone,
                        // instead of merely adding an un-rotated world offset.
                        const FTransform DroneTransform(
                            Effect.DroneVisualRotation,
                            Effect.DroneVisualOffset + Effect.DroneOrbitCenter,
                            Effect.DroneBaseScale * Effect.DronePlaneScale);
                        NewTransform = NewTransform * DroneTransform;

                        // Visible only during the vertical-move stage (Stage 3). When flicker is enabled,
                        // gate visibility on an irregular overlapping-sine pattern for an organic strobe.
                        bool bPlaneVisible = (Effect.DroneStage == 3);
                        if (bPlaneVisible && Effect.bDronePlaneFlicker) {
                            const float FlickerVal =
                                FMath::Sin(Effect.DroneTimer * 25.f * Effect.DronePlaneFlickerSpeed) +
                                FMath::Sin(Effect.DroneTimer * 13.f * Effect.DronePlaneFlickerSpeed) +
                                FMath::Sin(Effect.DroneTimer * 37.f * Effect.DronePlaneFlickerSpeed);
                            // Threshold sets the on/off duty cycle (-0.5 keeps it "on" more than "off").
                            bPlaneVisible = (FlickerVal > -0.5f);
                        }

                        // Hide by zeroing scale (position kept). When visible, leave the composed scale
                        // (DroneBaseScale * the plane's BaseOffset scale) untouched so the plane's authored
                        // size is preserved rather than forced to DroneBaseScale.
                        if (!bPlaneVisible) {
                            NewTransform.SetScale3D(FVector::ZeroVector);
                        }
                    }
                    else {
                        // Main mesh / other instances keep the original actor-local additive placement, so
                        // concurrent oscillation / location tweens / non-identity base offsets are
                        // unaffected by the rigid-child composition above.
                        NewTransform.AddToTranslation(Effect.DroneVisualOffset + Effect.DroneOrbitCenter);
                        NewTransform.SetRotation(Effect.DroneVisualRotation * NewTransform.GetRotation());
                        NewTransform.SetScale3D(Effect.DroneBaseScale * NewTransform.GetScale3D());
                    }
                }

                Instance.CurrentRelativeTransform = NewTransform;
            }

            // Set dirty flag when any tween or effect is active so PlacementProcessor updates the ISM
            const bool bHasActiveTweenOrEffect =
                Tween.RotationTween.bActive || Tween.LocationTween.bActive || Tween.ScaleTween.bActive ||
                Effect.bPulsateEnabled || Effect.bRotationEnabled || Effect.bOscillationEnabled ||
                Effect.bDishRotationEnabled || (Effect.bYawChaseEnabled && bHasYawChaseTarget) || Effect.bDroneEnabled;

            if (bHasActiveTweenOrEffect)
            {
                CharList[i].bTransformDirty = true;
            }
        }
    }));
}
