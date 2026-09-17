// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PCGClearingBlueprintLibrary.generated.h"

/**
 * Clears PCG-generated instances (vegetation, rocks) out of a footprint at runtime -- e.g. where a
 * building was just placed.
 *
 * Why this exists instead of calling PCGComponent::Generate: PCG only re-runs a graph in response to
 * a world change while the editor is running. The whole change-tracking pipeline (FPCGTrackingManager,
 * UPCGComponent::Refresh, the bDirtyGenerated path in ShouldGenerate) is WITH_EDITOR-only, so a
 * building placed during play never carves the graph in a packaged build. Forcing Generate(bForce) at
 * runtime would work, but it tears down and re-executes every graph on every partition actor in range
 * -- far too expensive to do per building placement in an RTS.
 *
 * Removing the instances directly is O(instances in the footprint), costs no graph execution, and
 * behaves identically in editor, PIE and a cooked build.
 *
 * Deliberately does NOT depend on the PCG module: PCG-spawned components are found via the component
 * tag PCG tags them with (PCGHelpers::DefaultPCGTag == "PCG Generated Component"), so RTSUnitTemplate
 * stays free of a PCG dependency.
 */
UCLASS()
class RTSUNITTEMPLATE_API UPCGClearingBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Removes every PCG-generated instance whose origin lies within Radius of Center.
	 *
	 * Only touches InstancedStaticMeshComponents carrying PCG's own "PCG Generated Component" tag, so
	 * it can never eat unit/building ISMs. Purely visual and local -- run it on every machine (it is
	 * not replicated); it does not affect collision or navigation, which PCG vegetation does not have.
	 *
	 * @param WorldContextObject  Any object in the world to operate on.
	 * @param Center              World-space centre of the footprint to clear.
	 * @param Radius              Clear radius in cm. Values <= 0 are ignored.
	 * @param bIncludeVertical    If false (default) the test is horizontal (XY) only, so instances are
	 *                            cleared regardless of terrain height under the building. If true, the
	 *                            test is a true 3D sphere.
	 * @return Number of instances removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|PCG", meta = (WorldContext = "WorldContextObject"))
	static int32 ClearPCGInstancesInRadius(
		const UObject* WorldContextObject,
		FVector Center,
		float Radius,
		bool bIncludeVertical = false);

	/**
	 * Removes every PCG-generated instance inside an ORIENTED box.
	 *
	 * Why a box and not the radius version above: a radius fits a building, which is roughly as wide
	 * as it is deep. It does not fit an energy wall, which is a long thin thing spanning the gap
	 * between two towers. A radius large enough to cover the span would also strip everything to the
	 * sides of it; one small enough not to would leave the middle of the wall overgrown.
	 *
	 * This is also why the actor tag "Obstacle" alone does not solve it for a wall: the PCG graph
	 * subtracts the actor's BOUNDS, and the wall's own bounds are not what blocks the ground - the
	 * NavObstacleBox that stretches between the towers is.
	 *
	 * Same constraints as the radius version: only touches ISMs carrying PCG's "PCG Generated
	 * Component" tag, purely visual, not replicated, run it on every machine.
	 *
	 * @param WorldContextObject  Any object in the world to operate on.
	 * @param BoxTransform        World transform of the box centre. Pass it WITHOUT scale - the test
	 *                            inverts this transform, and an inverse that carries scale would
	 *                            divide it back out of the point while BoxExtent still carries it.
	 *                            At scale 1 that is invisible; at any other scale the cleared strip
	 *                            is wrong by exactly that factor, and Padding is no longer in cm.
	 *                            From a UBoxComponent: FTransform(GetComponentQuat(), GetComponentLocation()).
	 * @param BoxExtent           HALF-extent in WORLD units, i.e. UBoxComponent::GetScaledBoxExtent().
	 * @param Padding             Extra cm added to the extent on every axis. Negative values shrink.
	 * @param bIncludeVertical    If false (default) the height axis is ignored, so instances are
	 *                            cleared regardless of terrain height under the wall. See the radius
	 *                            version for why that is the useful default.
	 * @return Number of instances removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|PCG", meta = (WorldContext = "WorldContextObject"))
	static int32 ClearPCGInstancesInBox(
		const UObject* WorldContextObject,
		FTransform BoxTransform,
		FVector BoxExtent,
		float Padding = 0.f,
		bool bIncludeVertical = false);
};
