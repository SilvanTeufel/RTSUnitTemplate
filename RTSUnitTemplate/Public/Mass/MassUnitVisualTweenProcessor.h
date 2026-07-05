// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassUnitVisualTweenProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassUnitVisualTweenProcessor : public UMassProcessor {
    GENERATED_BODY()

public:
    UMassUnitVisualTweenProcessor();

protected:
    UPROPERTY(EditAnywhere, Category = "Mass")
    bool bLogDebug = false;

    // ---- Drone visual-simulation tuning (global for every DroneBehavior construction site) ----
    // These were previously hardcoded in MassUnitVisualTweenProcessor.cpp. They are read from `this`
    // inside Execute(), so editing them on the processor class defaults retunes the drone build
    // animation without touching code. All heights are relative to the building BASE (the drone Z
    // anchor is the mesh bounds bottom, see UnitActorToFragmentSyncProcessor::SyncVisualEffect).

    // Stage timings (seconds).
    UPROPERTY(EditAnywhere, Category = "Drone|Timing", meta = (ClampMin = "0.0"))
    float DroneFlyDownDuration = 3.0f;   // Stage 0: spawn fly-down before orbiting
    UPROPERTY(EditAnywhere, Category = "Drone|Timing", meta = (ClampMin = "0.0"))
    float DroneFocusDuration = 1.5f;     // Stage 2: focus/aim before the vertical move
    UPROPERTY(EditAnywhere, Category = "Drone|Timing", meta = (ClampMin = "0.0"))
    float DroneReorientDuration = 1.0f;  // Stage 4: reorient before returning to orbit
    UPROPERTY(EditAnywhere, Category = "Drone|Timing", meta = (ClampMin = "0.0"))
    float DroneSettleWindow = 1.0f;      // Stage 3: extra settle time after the bounce ends

    // Orbit angle sweep per pass (degrees); a random value in [Min,Max] is drawn each orbit/reorient.
    UPROPERTY(EditAnywhere, Category = "Drone|Orbit", meta = (ClampMin = "0.0"))
    float DroneOrbitAngleMin = 60.f;
    UPROPERTY(EditAnywhere, Category = "Drone|Orbit", meta = (ClampMin = "0.0"))
    float DroneOrbitAngleMax = 180.f;

    // Vertical move (Stage 3). SafeMin is the hard floor above the ground; the settle target is a
    // random height in [TargetMin, MaxFraction * BuildingHeight].
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneMinHeightAbs = 50.f;              // absolute lower clamp above ground
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneMinHeightFraction = 0.15f;        // ... or this fraction of building height, whichever is larger
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneTargetHeightMin = 10.f;           // lowest random settle height (near ground)
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneTargetHeightMaxFraction = 1.0f;   // highest settle height = this * building height

    // Bounce shape (Stage 3): a random number of up/down cycles over a random duration, with an
    // amplitude drawn as a fraction of the building height. The bounce is centered ON the settle
    // target and its amplitude is clamped to the target-to-floor headroom so the lowest point never
    // clips through the ground.
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceCyclesMin = 1.f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceCyclesMax = 3.f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceDurationMin = 3.f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceDurationMax = 5.f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceAmplitudeMinFraction = 0.30f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceAmplitudeMaxFraction = 0.55f;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneBounceReferenceHeightMin = 150.f; // amplitude uses Max(BuildingHeight, this) as reference

    // Despawn (Stage 5): the drone rises up and out of sight once the build is nearly done.
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DroneDespawnBuildFraction = 0.95f;     // start the fly-up at this fraction of build completion (higher = later)
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0"))
    float DroneDespawnAscentSpeed = 1.5f;        // FInterpTo speed of the upward exit (lower = slower / gentler)
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0"))
    float DroneDespawnRiseHeight = 10000.f;      // how far above DroneSpawnHeight the exit aims
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0"))
    float DroneDespawnDuration = 5.0f;           // seconds of ascent before the drone visual is disabled
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0"))
    float DroneDespawnPitchSpeedBonus = 3.f;    // extra pitch-interp speed during the exit

    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
