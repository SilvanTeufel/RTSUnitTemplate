// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitVisualTweenProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
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
                    UE_LOG(LogTemp, Log, TEXT("Entity %s: Pulsating ISM %s - Scale: %s"), *Context.GetEntity(i).DebugGetDescription(), *Effect.PulsateTargetISM->GetName(), *CurrentPulsateScale.ToString());
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
                    UE_LOG(LogTemp, Log, TEXT("Entity %s: Rotating ISM %s - Total Angle: %f, Elapsed: %f"), *Context.GetEntity(i).DebugGetDescription(), *Effect.RotationTargetISM->GetName(), TotalAngle, Effect.RotationElapsed);
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
                    UE_LOG(LogTemp, Log, TEXT("Entity %s: Oscillating ISM %s - Offset: %s"), *Context.GetEntity(i).DebugGetDescription(), *Effect.OscillationTargetISM->GetName(), *CurrentOscOffset.ToString());
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

            if (Effect.bYawChaseEnabled && TargetEntity.IsValid()) {
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

                Instance.CurrentRelativeTransform = NewTransform;
            }

            // Set dirty flag when any tween or effect is active so PlacementProcessor updates the ISM
            const bool bHasActiveTweenOrEffect =
                Tween.RotationTween.bActive || Tween.LocationTween.bActive || Tween.ScaleTween.bActive ||
                Effect.bPulsateEnabled || Effect.bRotationEnabled || Effect.bOscillationEnabled ||
                Effect.bDishRotationEnabled || (Effect.bYawChaseEnabled && bHasYawChaseTarget);

            if (bHasActiveTweenOrEffect)
            {
                CharList[i].bTransformDirty = true;
            }
        }
    }));
}
