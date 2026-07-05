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

    // Vertical move (Stage 3). SafeMin is the hard floor above the ground; the drone scans the band
    // [SafeMin, scan ceiling], where the ceiling is set per build-time third (see below).
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneMinHeightAbs = 50.f;              // absolute lower clamp above ground
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneMinHeightFraction = 0.15f;        // ... or this fraction of building height, whichever is larger
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneTargetHeightMin = 10.f;           // absolute floor for the scan ceiling

    // The drone's top scan height FOLLOWS THE BUILD in three phases (thirds of the build TIME). In each
    // third the drone scans from the ground (SafeMin) up to a fraction of the building height: first
    // third up to Third1*H, second third up to Third2*H, last third up to Third3*H. (Defaults
    // 0.35 / 0.65 / 1.0 = lower third, then up to two thirds, then the whole facade.) The Stage-3 bounce
    // peak sits at the current third's ceiling and oscillates DOWNWARD toward SafeMin. Set bFollow=false
    // for a constant Third3*H cap.
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical")
    bool bDroneFollowBuildProgress = true;
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneThird1HeightFraction = 0.5f;     // 0 .. 1/3 of build time: scan up to this * building height
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneThird2HeightFraction = 1.0f;     // 1/3 .. 2/3 of build time: scan up to this * building height
    UPROPERTY(EditAnywhere, Category = "Drone|Vertical", meta = (ClampMin = "0.0"))
    float DroneThird3HeightFraction = 1.0f;     // 2/3 .. 1 of build time: scan up to this * building height

    // Bounce shape (Stage 3): a random number of up/down cycles over a random duration. The bounce PEAK
    // sits at the current build-front height (see above) and it oscillates DOWNWARD from there; the
    // amplitude fraction sets how deep below the front it scans (of the front-to-SafeMin span). The
    // trough is clamped to SafeMin so it never clips the ground.
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

    // Despawn (Stage 5): the drone rises up and out of sight once the build is nearly done.
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DroneDespawnBuildFraction = 0.95f;     // start the fly-up at this fraction of build completion (higher = later)
    // Near the end of the build, stop looping through reorient/orbit (Stage 4 -> 1): once build progress
    // reaches this fraction the drone stays in the vertical move (hovering) until the despawn fly-up
    // triggers. Keep this < DroneDespawnBuildFraction so there is a "hold" window before it leaves.
    UPROPERTY(EditAnywhere, Category = "Drone|Despawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DroneSkipReorientBuildFraction = 0.85f;
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
