// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/RTSUnitGeometry.h"

#include "Characters/Unit/MassUnitBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "Mass/UnitMassTag.h"

namespace RTSUnitGeometry
{
	namespace
	{
		double RaySphere(const FVector& Origin, const FVector& Dir, const FVector& Center, double Radius)
		{
			const FVector OC = Origin - Center;
			const double B = FVector::DotProduct(OC, Dir);
			const double C = FVector::DotProduct(OC, OC) - Radius * Radius;
			const double H = B * B - C;
			if (H < 0.0) return -1.0;
			const double T = -B - FMath::Sqrt(H);
			return T >= 0.0 ? T : -1.0;
		}

		/** Ray against capsule (axis A-B, radius R). A ray starting inside counts as a hit at 0. */
		double RayCapsule(const FVector& Origin, const FVector& Dir, const FVector& A, const FVector& B, double Radius)
		{
			if (Radius <= KINDA_SMALL_NUMBER) return -1.0;
			if (FMath::PointDistToSegmentSquared(Origin, A, B) <= Radius * Radius) return 0.0;

			const FVector BA = B - A;
			const FVector OA = Origin - A;
			const double BABA = FVector::DotProduct(BA, BA);
			const double BARD = FVector::DotProduct(BA, Dir);
			const double BAOA = FVector::DotProduct(BA, OA);
			const double RDOA = FVector::DotProduct(Dir, OA);
			const double OAOA = FVector::DotProduct(OA, OA);
			const double ACoef = BABA - BARD * BARD;

			// Ray (almost) parallel to the axis, or capsule degenerated to a sphere: end spheres only.
			if (BABA <= KINDA_SMALL_NUMBER || ACoef <= 1e-6 * BABA)
			{
				const double TA = RaySphere(Origin, Dir, A, Radius);
				const double TB = RaySphere(Origin, Dir, B, Radius);
				if (TA < 0.0) return TB;
				if (TB < 0.0) return TA;
				return FMath::Min(TA, TB);
			}

			const double BCoef = BABA * RDOA - BAOA * BARD;
			const double CCoef = BABA * OAOA - BAOA * BAOA - Radius * Radius * BABA;
			const double H = BCoef * BCoef - ACoef * CCoef;
			if (H < 0.0) return -1.0;

			const double TCylinder = (-BCoef - FMath::Sqrt(H)) / ACoef;
			const double Y = BAOA + TCylinder * BARD;
			if (Y > 0.0 && Y < BABA)
			{
				return TCylinder >= 0.0 ? TCylinder : -1.0;
			}
			return RaySphere(Origin, Dir, Y <= 0.0 ? A : B, Radius);
		}

		/** Ray against a rotated box (slab test). Entry distance or -1. */
		double RayOrientedBox(const FVector& Origin, const FVector& Dir, const FVector& Center, const FQuat& Rotation, const FVector& Extent)
		{
			const FVector LocalOrigin = Rotation.UnrotateVector(Origin - Center);
			const FVector LocalDir = Rotation.UnrotateVector(Dir);

			double TNear = -TNumericLimits<double>::Max();
			double TFar = TNumericLimits<double>::Max();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double O = LocalOrigin[Axis];
				const double D = LocalDir[Axis];
				const double E = Extent[Axis];
				if (FMath::Abs(D) < 1e-8)
				{
					if (O < -E || O > E) return -1.0;
					continue;
				}
				double T1 = (-E - O) / D;
				double T2 = (E - O) / D;
				if (T1 > T2) Swap(T1, T2);
				TNear = FMath::Max(TNear, T1);
				TFar = FMath::Min(TFar, T2);
				if (TNear > TFar) return -1.0;
			}
			if (TFar < 0.0) return -1.0;
			return FMath::Max(TNear, 0.0);
		}

		/** A box seen from above: center, unit X axis and half extents, all in XY. */
		struct FBox2DOriented
		{
			FVector2D Center = FVector2D::ZeroVector;
			FVector2D AxisX = FVector2D(1.f, 0.f);
			FVector2D AxisY = FVector2D(0.f, 1.f);
			FVector2D Extent = FVector2D::ZeroVector;
		};

		/**
		 * Projects a rotated 3D box onto the ground plane. Only the yaw survives: the boxes of
		 * buildings stand upright, a slight ground alignment tilt does not change the footprint
		 * noticeably.
		 */
		FBox2DOriented ToFootprint(const FVector& Center, const FQuat& Rotation, const FVector& Extent)
		{
			FBox2DOriented Out;
			Out.Center = FVector2D(Center.X, Center.Y);
			FVector2D Forward(Rotation.GetAxisX().X, Rotation.GetAxisX().Y);
			if (!Forward.Normalize())
			{
				Forward = FVector2D(1.f, 0.f);
			}
			Out.AxisX = Forward;
			Out.AxisY = FVector2D(-Forward.Y, Forward.X);
			Out.Extent = FVector2D(Extent.X, Extent.Y);
			return Out;
		}

		FVector2D ToLocal(const FBox2DOriented& Box, const FVector2D& P)
		{
			const FVector2D D = P - Box.Center;
			return FVector2D(FVector2D::DotProduct(D, Box.AxisX), FVector2D::DotProduct(D, Box.AxisY));
		}

		float PointToBox2D(const FBox2DOriented& Box, const FVector2D& P)
		{
			const FVector2D L = ToLocal(Box, P);
			const FVector2D Outside(FMath::Max(FMath::Abs(L.X) - Box.Extent.X, 0.f), FMath::Max(FMath::Abs(L.Y) - Box.Extent.Y, 0.f));
			return Outside.Size();
		}

		bool SegmentCrossesBox2D(const FBox2DOriented& Box, const FVector2D& A, const FVector2D& B)
		{
			// Slab test in box space, restricted to t in [0, 1].
			const FVector2D LA = ToLocal(Box, A);
			const FVector2D LB = ToLocal(Box, B);
			const FVector2D D = LB - LA;
			double TNear = 0.0;
			double TFar = 1.0;
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const double O = LA[Axis];
				const double Dir = D[Axis];
				const double E = Box.Extent[Axis];
				if (FMath::Abs(Dir) < 1e-8)
				{
					if (O < -E || O > E) return false;
					continue;
				}
				double T1 = (-E - O) / Dir;
				double T2 = (E - O) / Dir;
				if (T1 > T2) Swap(T1, T2);
				TNear = FMath::Max(TNear, T1);
				TFar = FMath::Min(TFar, T2);
				if (TNear > TFar) return false;
			}
			return true;
		}

		float PointToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
		{
			const FVector2D AB = B - A;
			const float Len2 = AB.SizeSquared();
			if (Len2 <= KINDA_SMALL_NUMBER)
			{
				return (P - A).Size();
			}
			const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Len2, 0.f, 1.f);
			return (P - (A + AB * T)).Size();
		}

		float SegmentToBox2D(const FBox2DOriented& Box, const FVector2D& A, const FVector2D& B)
		{
			if (SegmentCrossesBox2D(Box, A, B))
			{
				return 0.f;
			}
			// Apart: the closest pair involves an end of the segment or a corner of the box.
			float Best = FMath::Min(PointToBox2D(Box, A), PointToBox2D(Box, B));
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const float SX = (Corner & 1) ? 1.f : -1.f;
				const float SY = (Corner & 2) ? 1.f : -1.f;
				const FVector2D C = Box.Center + Box.AxisX * (Box.Extent.X * SX) + Box.AxisY * (Box.Extent.Y * SY);
				Best = FMath::Min(Best, PointToSegment2D(C, A, B));
			}
			return Best;
		}

		void FinishBroad(FShapeSet& Out)
		{
			// Pre-filter cylinder around everything, from the placed shapes.
			const FVector Axis = Out.bCapsule ? Out.CapsuleCenter : (Out.bBox ? Out.BoxCenter : Out.MeshCenter);
			float Radius = 0.f;
			float Bottom = TNumericLimits<float>::Max();
			float Top = -TNumericLimits<float>::Max();
			auto AddBox = [&](const FVector& Center, const FQuat& Rotation, const FVector& Extent)
			{
				for (int32 Corner = 0; Corner < 8; ++Corner)
				{
					const FVector Sign((Corner & 1) ? 1.f : -1.f, (Corner & 2) ? 1.f : -1.f, (Corner & 4) ? 1.f : -1.f);
					const FVector P = Center + Rotation.RotateVector(Extent * Sign);
					Radius = FMath::Max(Radius, (float)FVector::Dist2D(P, Axis));
					Bottom = FMath::Min(Bottom, (float)P.Z);
					Top = FMath::Max(Top, (float)P.Z);
				}
			};
			if (Out.bCapsule) AddBox(Out.CapsuleCenter, FQuat::Identity, FVector(Out.CapsuleRadius, Out.CapsuleRadius, Out.CapsuleHalfHeight));
			if (Out.bBox) AddBox(Out.BoxCenter, Out.BoxRotation, Out.BoxExtent);
			if (Out.bMesh) AddBox(Out.MeshCenter, Out.MeshRotation, Out.MeshExtent);

			if (Bottom > Top)
			{
				Out.bValid = false;
				return;
			}
			// Small margin: a ground-aligned (tilted) entity turns corners slightly out of the cylinder.
			const float Margin = 0.05f * FMath::Max(Top - Bottom, Radius) + 10.f;
			Out.BroadStart = FVector(Axis.X, Axis.Y, Bottom - Margin);
			Out.BroadEnd = FVector(Axis.X, Axis.Y, Top + Margin);
			Out.BroadRadius = Radius + Margin;
			Out.bValid = true;
		}
	}

	FVector GetAnchor(const FMassAgentCharacteristicsFragment& CharFrag, const FTransform& EntityTransform)
	{
		FVector Anchor = EntityTransform.GetLocation();
		if (CharFrag.bHoverGroundAnchored && !CharFrag.bIsFlying)
		{
			// Same rule the binding component uses to place a building: ground + capsule half height.
			// Flying buildings keep the transform - their ground value is not tracked while they travel.
			Anchor.Z = CharFrag.LastGroundLocation + CharFrag.CapsuleHeight;
		}
		return Anchor;
	}

	void Build(const FMassAgentCharacteristicsFragment& CharFrag, const FTransform& EntityTransform, FShapeSet& Out)
	{
		Out = FShapeSet();
		const FVector Location = GetAnchor(CharFrag, EntityTransform);
		const FQuat Rotation = EntityTransform.GetRotation();

		if (!CharFrag.bHoverShapesCaptured)
		{
			// Without captured shapes: only the capsule from the fragment.
			Out.bCapsule = CharFrag.CapsuleRadius > KINDA_SMALL_NUMBER;
			Out.CapsuleCenter = Location;
			Out.CapsuleRotation = Rotation;
			Out.CapsuleRadius = CharFrag.CapsuleRadius;
			Out.CapsuleHalfHeight = FMath::Max(CharFrag.CapsuleHeight, CharFrag.CapsuleRadius);
			FinishBroad(Out);
			return;
		}

		// Scale change since the capture, per axis.
		FVector Ratio(1.f);
		const FVector CurrentScale = EntityTransform.GetScale3D();
		const FVector CaptureScale(CharFrag.HoverCaptureScale);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(CaptureScale[Axis]) > KINDA_SMALL_NUMBER && FMath::Abs(CurrentScale[Axis]) > KINDA_SMALL_NUMBER)
			{
				Ratio[Axis] = FMath::Abs(CurrentScale[Axis]) / FMath::Abs(CaptureScale[Axis]);
			}
		}
		const float RatioXY = (float)FMath::Max(Ratio.X, Ratio.Y);
		const float RatioZ = (float)Ratio.Z;

		Out.bCapsule = CharFrag.HoverCapsuleRadius > KINDA_SMALL_NUMBER;
		Out.CapsuleCenter = Location + Rotation.RotateVector(FVector(CharFrag.HoverCapsuleCenter) * Ratio);
		Out.CapsuleRotation = Rotation;
		Out.CapsuleRadius = CharFrag.HoverCapsuleRadius * RatioXY;
		Out.CapsuleHalfHeight = FMath::Max(CharFrag.HoverCapsuleHalfHeight * RatioZ, Out.CapsuleRadius);

		Out.bBox = CharFrag.bHoverHasBox;
		Out.BoxCenter = Location + Rotation.RotateVector(FVector(CharFrag.HoverBoxCenter) * Ratio);
		Out.BoxRotation = Rotation * FQuat(CharFrag.HoverBoxRotation);
		Out.BoxExtent = FVector(CharFrag.HoverBoxExtent) * Ratio;

		// MESH-BOX VORERST AUS (Nutzervorgabe 01.10.2026): nur Kapsel und BoxCollision zaehlen. Die
		// Mesh-Box ist die Huelle des ganzen Modells und deckt bei Antennen/Masten viel Leerraum ab.
		// Erfasst wird sie weiterhin (CharFrag.bHoverHasMesh) - zum Wiedereinschalten hier entkommentieren.
		// Out.bMesh = CharFrag.bHoverHasMesh;
		// Out.MeshCenter = Location + Rotation.RotateVector(FVector(CharFrag.HoverMeshCenter) * Ratio);
		// Out.MeshRotation = Rotation;
		// Out.MeshExtent = FVector(CharFrag.HoverMeshExtent) * Ratio;

		FinishBroad(Out);
	}

	bool BuildForUnit(const AUnitBase* Unit, FShapeSet& Out)
	{
		Out = FShapeSet();
		if (!IsValid(Unit))
		{
			return false;
		}

		// The Mass entity is the truth for ISM units: the actor of an ISM building is not where its picture is.
		if (const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit))
		{
			const FMassEntityManager* EntityManager = nullptr;
			FMassEntityHandle EntityHandle;
			if (MassUnit->GetMassEntityData(EntityManager, EntityHandle) && EntityManager && EntityManager->IsEntityValid(EntityHandle))
			{
				const FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle);
				const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
				if (CharFrag && TransformFrag && CharFrag->bHoverShapesCaptured)
				{
					Build(*CharFrag, TransformFrag->GetTransform(), Out);
					return Out.bValid;
				}
			}
		}

		// No entity (yet): measure the actor's own components.
		// const FTransform ActorFrame(Unit->GetActorQuat(), Unit->GetActorLocation()); // nur fuer die Mesh-Box
		if (const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent())
		{
			Out.bCapsule = Capsule->GetScaledCapsuleRadius() > KINDA_SMALL_NUMBER;
			Out.CapsuleCenter = Capsule->GetComponentLocation();
			Out.CapsuleRotation = Unit->GetActorQuat();
			Out.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
			Out.CapsuleHalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(), Out.CapsuleRadius);
		}
		if (const UBoxComponent* Box = Unit->BoxCollisionComponent)
		{
			const FVector Extent = Box->GetScaledBoxExtent();
			if (Extent.X > KINDA_SMALL_NUMBER && Extent.Y > KINDA_SMALL_NUMBER && Extent.Z > KINDA_SMALL_NUMBER)
			{
				Out.bBox = true;
				Out.BoxCenter = Box->GetComponentLocation();
				Out.BoxRotation = Box->GetComponentQuat();
				Out.BoxExtent = Extent;
			}
		}
		// MESH-BOX VORERST AUS (Nutzervorgabe 01.10.2026), siehe Build().
		// const FBox MeshLocal = AConstructionUnit::ComputeVisualBounds(Unit, &ActorFrame);
		// if (MeshLocal.IsValid)
		// {
		// 	const FVector Extent = MeshLocal.GetExtent();
		// 	if (Extent.X > KINDA_SMALL_NUMBER && Extent.Y > KINDA_SMALL_NUMBER && Extent.Z > KINDA_SMALL_NUMBER)
		// 	{
		// 		Out.bMesh = true;
		// 		Out.MeshCenter = ActorFrame.TransformPosition(MeshLocal.GetCenter());
		// 		Out.MeshRotation = Unit->GetActorQuat();
		// 		Out.MeshExtent = Extent;
		// 	}
		// }
		FinishBroad(Out);
		return Out.bValid;
	}

	double RayTrace(const FShapeSet& Shapes, const FVector& Origin, const FVector& Dir, double MaxDistance, EHitShape& OutShape)
	{
		double Best = -1.0;
		OutShape = EHitShape::None;
		auto Take = [&Best, &OutShape, MaxDistance](double T, EHitShape Shape)
		{
			if (T >= 0.0 && T <= MaxDistance && (Best < 0.0 || T < Best))
			{
				Best = T;
				OutShape = Shape;
			}
		};

		if (Shapes.bCapsule)
		{
			const FVector Up = Shapes.CapsuleRotation.GetAxisZ();
			const float HalfSegment = FMath::Max(Shapes.CapsuleHalfHeight - Shapes.CapsuleRadius, 0.f);
			Take(RayCapsule(Origin, Dir, Shapes.CapsuleCenter - Up * HalfSegment, Shapes.CapsuleCenter + Up * HalfSegment, Shapes.CapsuleRadius), EHitShape::Capsule);
		}
		if (Shapes.bBox)
		{
			Take(RayOrientedBox(Origin, Dir, Shapes.BoxCenter, Shapes.BoxRotation, Shapes.BoxExtent), EHitShape::Box);
		}
		if (Shapes.bMesh)
		{
			Take(RayOrientedBox(Origin, Dir, Shapes.MeshCenter, Shapes.MeshRotation, Shapes.MeshExtent), EHitShape::Mesh);
		}
		return Best;
	}

	float DistanceToFootprint2D(const FShapeSet& Shapes, const FVector2D& Point)
	{
		float Best = TNumericLimits<float>::Max();
		if (Shapes.bCapsule)
		{
			Best = FMath::Min(Best, FMath::Max(0.f, (float)(Point - FVector2D(Shapes.CapsuleCenter.X, Shapes.CapsuleCenter.Y)).Size() - Shapes.CapsuleRadius));
		}
		if (Shapes.bBox)
		{
			Best = FMath::Min(Best, PointToBox2D(ToFootprint(Shapes.BoxCenter, Shapes.BoxRotation, Shapes.BoxExtent), Point));
		}
		if (Shapes.bMesh)
		{
			Best = FMath::Min(Best, PointToBox2D(ToFootprint(Shapes.MeshCenter, Shapes.MeshRotation, Shapes.MeshExtent), Point));
		}
		return Best;
	}

	float SegmentDistanceToFootprint2D(const FShapeSet& Shapes, const FVector2D& A, const FVector2D& B)
	{
		float Best = TNumericLimits<float>::Max();
		if (Shapes.bCapsule)
		{
			const FVector2D C(Shapes.CapsuleCenter.X, Shapes.CapsuleCenter.Y);
			Best = FMath::Min(Best, FMath::Max(0.f, PointToSegment2D(C, A, B) - Shapes.CapsuleRadius));
		}
		if (Shapes.bBox)
		{
			Best = FMath::Min(Best, SegmentToBox2D(ToFootprint(Shapes.BoxCenter, Shapes.BoxRotation, Shapes.BoxExtent), A, B));
		}
		if (Shapes.bMesh)
		{
			Best = FMath::Min(Best, SegmentToBox2D(ToFootprint(Shapes.MeshCenter, Shapes.MeshRotation, Shapes.MeshExtent), A, B));
		}
		return Best;
	}

	bool GetUnitAnchor(const AUnitBase* Unit, FVector& OutAnchor)
	{
		const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit);
		if (!IsValid(MassUnit))
		{
			return false;
		}
		const FMassEntityManager* EntityManager = nullptr;
		FMassEntityHandle EntityHandle;
		if (!MassUnit->GetMassEntityData(EntityManager, EntityHandle) || !EntityManager || !EntityManager->IsEntityValid(EntityHandle))
		{
			return false;
		}
		const FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle);
		const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
		if (!CharFrag || !TransformFrag || !CharFrag->bHoverShapesCaptured)
		{
			return false;
		}
		OutAnchor = GetAnchor(*CharFrag, TransformFrag->GetTransform());
		return true;
	}

	FBox GetBoundingBox(const FShapeSet& Shapes)
	{
		FBox Box(ForceInit);
		auto AddBox = [&Box](const FVector& Center, const FQuat& Rotation, const FVector& Extent)
		{
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Sign((Corner & 1) ? 1.f : -1.f, (Corner & 2) ? 1.f : -1.f, (Corner & 4) ? 1.f : -1.f);
				Box += Center + Rotation.RotateVector(Extent * Sign);
			}
		};
		if (Shapes.bCapsule) AddBox(Shapes.CapsuleCenter, FQuat::Identity, FVector(Shapes.CapsuleRadius, Shapes.CapsuleRadius, Shapes.CapsuleHalfHeight));
		if (Shapes.bBox) AddBox(Shapes.BoxCenter, Shapes.BoxRotation, Shapes.BoxExtent);
		if (Shapes.bMesh) AddBox(Shapes.MeshCenter, Shapes.MeshRotation, Shapes.MeshExtent);
		return Box;
	}

	FVector GetFootprintCenter(const FShapeSet& Shapes)
	{
		const FVector Axis = (Shapes.BroadStart + Shapes.BroadEnd) * 0.5f;
		return FVector(Axis.X, Axis.Y, GetBottomZ(Shapes));
	}

	float GetBottomZ(const FShapeSet& Shapes)
	{
		float Bottom = TNumericLimits<float>::Max();
		if (Shapes.bCapsule) Bottom = FMath::Min(Bottom, (float)Shapes.CapsuleCenter.Z - Shapes.CapsuleHalfHeight);
		if (Shapes.bBox) Bottom = FMath::Min(Bottom, (float)(Shapes.BoxCenter.Z - Shapes.BoxExtent.Z));
		if (Shapes.bMesh) Bottom = FMath::Min(Bottom, (float)(Shapes.MeshCenter.Z - Shapes.MeshExtent.Z));
		return Bottom == TNumericLimits<float>::Max() ? (float)Shapes.BroadStart.Z : Bottom;
	}

	void DrawShapes(const UWorld* World, const FShapeSet& Shapes, EHitShape HitShape, float LifeTime)
	{
#if ENABLE_DRAW_DEBUG
		if (Shapes.bCapsule)
		{
			DrawDebugCapsule(World, Shapes.CapsuleCenter, Shapes.CapsuleHalfHeight, Shapes.CapsuleRadius, Shapes.CapsuleRotation,
				FColor::Green, false, LifeTime, 0, HitShape == EHitShape::Capsule ? 4.f : 1.f);
		}
		if (Shapes.bBox)
		{
			DrawDebugBox(World, Shapes.BoxCenter, Shapes.BoxExtent, Shapes.BoxRotation,
				FColor::Blue, false, LifeTime, 0, HitShape == EHitShape::Box ? 4.f : 1.f);
		}
		if (Shapes.bMesh)
		{
			DrawDebugBox(World, Shapes.MeshCenter, Shapes.MeshExtent, Shapes.MeshRotation,
				FColor::Yellow, false, LifeTime, 0, HitShape == EHitShape::Mesh ? 4.f : 1.f);
		}
#endif
	}

	void DrawBroad(const UWorld* World, const FShapeSet& Shapes, float LifeTime)
	{
#if ENABLE_DRAW_DEBUG
		DrawDebugCylinder(World, Shapes.BroadStart, Shapes.BroadEnd, Shapes.BroadRadius, 16, FColor(128, 128, 128), false, LifeTime, 0, 1.f);
#endif
	}
}
