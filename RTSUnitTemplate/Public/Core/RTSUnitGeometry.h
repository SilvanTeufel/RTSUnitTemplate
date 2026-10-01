// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMassAgentCharacteristicsFragment;
class AUnitBase;
class UWorld;

/**
 * ONE geometry for a unit or building, shared by every place that asks "where is this thing?"
 * (01.10.2026): the hover pick (UMassUnitHoverProcessor), the wall snap and the energy wall
 * path check (AExtendedControllerBase).
 *
 * Before, each of them measured on its own: the hover from captured shapes on the entity
 * transform, the snap from the ACTOR's components (unreliable for ISM buildings, whose actor
 * is not where the picture is), the path check from a circle around the axis-aligned visual
 * bounds. Three answers for one building - and none of them matched the others.
 *
 * The shapes are the three captured in UMassActorBindingComponent: the real capsule, the real
 * BoxCollision box and the mesh box, each with its own measurements, combined with OR.
 *
 * Buildings are anchored on LastGroundLocation instead of the FTransformFragment: measured on
 * 24.09.2026, the transform fragment of some buildings sits at ground height without the
 * capsule offset (190.2 instead of 389.2) - which put the hover shapes one capsule half height
 * too low. LastGroundLocation is set once from the ground trace and stays stable.
 */
namespace RTSUnitGeometry
{
	struct RTSUNITTEMPLATE_API FShapeSet
	{
		bool bValid = false;

		/** Vertical cylinder around everything, cheap pre-filter for ray tests. */
		FVector BroadStart = FVector::ZeroVector;
		FVector BroadEnd = FVector::ZeroVector;
		float BroadRadius = 0.f;

		bool bCapsule = false;
		FVector CapsuleCenter = FVector::ZeroVector;
		FQuat CapsuleRotation = FQuat::Identity;
		float CapsuleRadius = 0.f;
		float CapsuleHalfHeight = 0.f;

		bool bBox = false;
		FVector BoxCenter = FVector::ZeroVector;
		FQuat BoxRotation = FQuat::Identity;
		FVector BoxExtent = FVector::ZeroVector;

		bool bMesh = false;
		FVector MeshCenter = FVector::ZeroVector;
		FQuat MeshRotation = FQuat::Identity;
		FVector MeshExtent = FVector::ZeroVector;
	};

	enum class EHitShape : uint8 { None, Capsule, Box, Mesh };

	/** Where the captured shapes are anchored: the entity location, with Z from the ground for buildings. */
	RTSUNITTEMPLATE_API FVector GetAnchor(const FMassAgentCharacteristicsFragment& CharFrag, const FTransform& EntityTransform);

	/** Shapes from the fragment, placed at the current pose of the entity. */
	RTSUNITTEMPLATE_API void Build(const FMassAgentCharacteristicsFragment& CharFrag, const FTransform& EntityTransform, FShapeSet& Out);

	/** Shapes of an actor: from its Mass entity when there is one, otherwise from its own components. */
	RTSUNITTEMPLATE_API bool BuildForUnit(const AUnitBase* Unit, FShapeSet& Out);

	/** Ray against all three shapes (OR). Dir normalized. Entry distance of the nearest hit, or -1. */
	RTSUNITTEMPLATE_API double RayTrace(const FShapeSet& Shapes, const FVector& Origin, const FVector& Dir, double MaxDistance, EHitShape& OutShape);

	/** 2D distance from a point to the footprint (XY projection of all three shapes). 0 inside. */
	RTSUNITTEMPLATE_API float DistanceToFootprint2D(const FShapeSet& Shapes, const FVector2D& Point);

	/** 2D distance from a segment to the footprint. 0 when the segment crosses it. */
	RTSUNITTEMPLATE_API float SegmentDistanceToFootprint2D(const FShapeSet& Shapes, const FVector2D& A, const FVector2D& B);

	/** Anchor of a unit's shapes (see GetAnchor), from its Mass entity. False without a captured entity. */
	RTSUNITTEMPLATE_API bool GetUnitAnchor(const AUnitBase* Unit, FVector& OutAnchor);

	/** Axis-aligned box around all three shapes. */
	RTSUNITTEMPLATE_API FBox GetBoundingBox(const FShapeSet& Shapes);

	/** Center of the footprint (XY) and the bottom of all shapes (Z). */
	RTSUNITTEMPLATE_API FVector GetFootprintCenter(const FShapeSet& Shapes);
	RTSUNITTEMPLATE_API float GetBottomZ(const FShapeSet& Shapes);

	RTSUNITTEMPLATE_API void DrawShapes(const UWorld* World, const FShapeSet& Shapes, EHitShape HitShape, float LifeTime);
	RTSUNITTEMPLATE_API void DrawBroad(const UWorld* World, const FShapeSet& Shapes, float LifeTime);
}
