// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBoxComponent;
class AActor;

/**
 * Utility class for RTS collision calculations, particularly for handling optional BoxComponents.
 */
class RTSUNITTEMPLATE_API FCollisionUtils
{
public:
    /**
     * Finds a UBoxComponent on the given actor that has the "BoxCollision" tag.
     */
    static UBoxComponent* FindTaggedBoxComponent(const AActor* Actor);

    /**
     * Finds a "BoxCollision"-tagged UBoxComponent template on a CLASS, without needing an
     * instance: checks the CDO's native components first, then walks the Blueprint SCS
     * templates up the class hierarchy (SCS components never exist on a CDO). Use this to
     * read a footprint off a TSubclassOf property (e.g. a WorkArea's BuildingClass) before
     * any actor of that class has been spawned.
     */
    static UBoxComponent* FindTaggedBoxTemplateOnClass(UClass* ActorClass);

    /**
     * Computes the impact surface point on the XY plane for a target.
     * Handles both Capsule and Box components.
     * @param Attacker The actor initiating the impact (can be null if OverrideIncomingLocation is provided).
     * @param Target The actor being impacted.
     * @param OverrideIncomingLocation Optional location to use instead of the Attacker's location.
     * @return The world-space surface impact point.
     */
    static FVector ComputeImpactSurfaceXY(const AActor* Attacker, const AActor* Target, TOptional<FVector> OverrideIncomingLocation = TOptional<FVector>());
};
