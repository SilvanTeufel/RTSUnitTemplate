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

    EntityQuery.ForEachEntityChunk(Context, ([&EntityManager, DeltaTime, bShouldLog](FMassExecutionContext& Context) {
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
                        // Randomize the vertical bounce for this entry: 1..3 up/down bounces over 3..5s.
                        const float SafeMin = FMath::Max(50.f, Effect.DroneBuildingHeight * 0.15f);
                        Effect.DroneBounceCycles    = FMath::FRandRange(1.f, 3.f);   // number of up/down bounces
                        Effect.DroneBounceDuration  = FMath::FRandRange(3.f, 5.f);   // over this many seconds
                        Effect.DroneBounceAmplitude = FMath::FRandRange(0.30f, 0.55f) * FMath::Max(Effect.DroneBuildingHeight, 150.f);
                        // Oscillate around the current height, raised so the lowest point (base - amplitude)
                        // stays safely above the ground.
                        Effect.DroneBounceBaseHeight = FMath::Max(Effect.DroneOffset.Z, SafeMin + Effect.DroneBounceAmplitude);
                    }
                }

                switch (Effect.DroneStage) {
                case 0: // Spawn / Fly down
                {
                    Effect.DroneOffset.Z = FMath::FInterpTo(Effect.DroneOffset.Z, Effect.DroneTargetHeight, DroneDeltaTime, Effect.DroneInterpSpeed);
                    FRotator CurrentRot = Effect.DroneRotation.Rotator();
                    CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, 90.f, DroneDeltaTime, Effect.DroneRotationSpeed);
                    Effect.DroneRotation = CurrentRot.Quaternion();

                    if (Effect.DroneTimer >= 3.0f) {
                        Effect.DroneStage = 1;
                        Effect.DroneTargetAngle = FMath::FRandRange(60.f, 180.f);
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

                    if (Effect.DroneTimer >= 1.5f) {
                        Effect.DroneStage = 3;
                        Effect.DroneTargetHeight = FMath::FRandRange(10.f, Effect.DroneBuildingHeight);
                    }
                }
                break;
                case 3: // Vertical Move — bounce up/down a few times, then settle at a random height
                {
                    const float SafeMinHeight = FMath::Max(50.f, Effect.DroneBuildingHeight * 0.15f);

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

                    // Transition only after the bounces AND a short settle window.
                    if (Effect.DroneTimer >= Effect.DroneBounceDuration + 1.0f) {
                        Effect.DroneStage = 4;
                    }
                }
                break;
                case 4: // Reorient
                {
                    FRotator TargetRot = FRotator(0.f, Effect.DroneCurrentAngle + 90.f, 0.f);
                    Effect.DroneRotation = FMath::QInterpTo(Effect.DroneRotation, TargetRot.Quaternion(), DroneDeltaTime, Effect.DroneRotationSpeed);

                    if (Effect.DroneTimer >= 1.0f) {
                        Effect.DroneStage = 1;
                        Effect.DroneTargetAngle = FMath::FRandRange(60.f, 180.f);
                    }
                }
                break;
                case 5: // Despawn — accelerate upward and fly out of sight
                {
                    // Interpolate toward a FIXED, very high target Z so the remaining distance stays
                    // large and the ascent keeps real speed. (Previously TargetOffset.Z was recomputed
                    // each frame just above the current Z, so VInterpTo never built up distance and
                    // crawled.) A solid interp speed gives a smooth start that quickly accelerates.
                    const float DespawnTargetZ    = Effect.DroneSpawnHeight + 10000.f; // far above the scene
                    const float DespawnInterpSpeed = Effect.DroneInterpSpeed + 3.0f;    // solid, snappy ascent
                    Effect.DroneOffset.Z = FMath::FInterpTo(Effect.DroneOffset.Z, DespawnTargetZ, DroneDeltaTime, DespawnInterpSpeed);

                    FRotator CurrentRot = Effect.DroneRotation.Rotator();
                    CurrentRot.Pitch = FMath::FInterpTo(CurrentRot.Pitch, 90.f, DroneDeltaTime, Effect.DroneRotationSpeed + 3.f);
                    Effect.DroneRotation = CurrentRot.Quaternion();

                    if (Effect.DroneTimer >= 5.0f) {
                        Effect.bDroneEnabled = false;
                    }
                }
                break;
                }

                // Autonome Despawn Erkennung
                if (Effect.DroneStage < 5) {
                    if (const AConstructionUnit* CU = Cast<AConstructionUnit>(ActorList[i].Get())) {
                        if (CU->WorkArea && CU->WorkArea->CurrentBuildTime > CU->WorkArea->BuildTime * 0.9f) {
                            Effect.DroneStage = 5;
                        }
                    }
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
                            Effect.DroneBaseScale);
                        NewTransform = NewTransform * DroneTransform;

                        // Only visible during the vertical-move stage (Stage 3). Hide it (zero scale,
                        // position kept) in every other stage so it pops back in at the right place.
                        if (Effect.DroneStage != 3) {
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
