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
};
