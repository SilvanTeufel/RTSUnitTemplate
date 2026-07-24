// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


// HUDBase.h (Corresponding Header)
#include "Hud/HUDBase.h"

// Engine Headers
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Net/UnrealNetwork.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "MassEntitySubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Core/CollisionUtils.h"
#include "Components/BoxComponent.h"

// Project-Specific Headers
#include "GAS/AttributeSetBase.h"
#include "Characters/Unit/HealingUnit.h"
#include "Elements/Actor/ActorElementData.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Actors/WorkArea.h"
#include "Actors/Waypoint.h"


void AHUDBase::DrawDashedLine3D(const FVector& InStart, const FVector& InEnd, float DashLen, float GapLen, FColor Color, float Thickness, float ZOffset)
{
	FVector Start = InStart; Start.Z += ZOffset;
	FVector End = InEnd; End.Z += ZOffset;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas) return;

	FVector2D ScreenStart, ScreenEnd;
	// Nur Endpunkte projizieren für maximale Performance
	if (!PC->ProjectWorldLocationToScreen(Start, ScreenStart) || !PC->ProjectWorldLocationToScreen(End, ScreenEnd))
	{
		return;
	}

	const FVector2D Dir = (ScreenEnd - ScreenStart);
	const float TotalScreenLen = Dir.Size();
	if (TotalScreenLen <= KINDA_SMALL_NUMBER) return;

	// Umrechnungsfaktor von 3D zu Screen (grob für die Dash-Länge)
	const float Total3DLen = FVector::Dist(Start, End);
	const float Scale = (Total3DLen > KINDA_SMALL_NUMBER) ? (TotalScreenLen / Total3DLen) : 1.f;

	const FVector2D StepDir = Dir / TotalScreenLen;
	const float ScreenDashLen = FMath::Max(1.f, FMath::IsFinite(DashLen) ? DashLen * Scale : 0.f);
	const float ScreenGapLen = FMath::Max(0.f, FMath::IsFinite(GapLen) ? GapLen * Scale : 0.f);
	const float Period = ScreenDashLen + ScreenGapLen; // ScreenDashLen >= 1px, so always advances

	float Traveled = 0.f;
	FLinearColor LineColor(Color);
	LineColor.A = (Color.A / 255.f);

	// Hard backstop on the segment count: an extreme projection (an endpoint near the
	// camera) can make TotalScreenLen huge while the period stays ~1px, which would
	// otherwise flood Canvas->DrawItem with thousands of items every frame.
	const int32 MaxSegments = FMath::Clamp(FMath::CeilToInt(TotalScreenLen / Period), 0, 4094) + 2;

	for (int32 i = 0; i < MaxSegments && Traveled < TotalScreenLen; ++i)
	{
		const float ThisDash = FMath::Min(ScreenDashLen, TotalScreenLen - Traveled);
		const FVector2D SegStart = ScreenStart + StepDir * Traveled;
		const FVector2D SegEnd = ScreenStart + StepDir * (Traveled + ThisDash);

		FCanvasLineItem LineItem(SegStart, SegEnd);
		LineItem.LineThickness = Thickness;
		LineItem.SetColor(LineColor);
		LineItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LineItem);

		Traveled += Period;
	}
}

void AHUDBase::DrawDashedLine2D(const FVector2D& Start, const FVector2D& End, float DashLen, float GapLen, FLinearColor Color, float Thickness)
{
	if (!Canvas) return;

	const FVector2D Dir = (End - Start);
	const float TotalLen = Dir.Size();
	if (TotalLen <= KINDA_SMALL_NUMBER) return;

	// Sanitize inputs: a non-positive (or non-finite) dash+gap period would never
	// advance Traveled, turning the loop into an unbounded Canvas->DrawItem flood that
	// exhausts memory. NaN/Inf are routed into the solid-line fallback below.
	DashLen = FMath::IsFinite(DashLen) ? FMath::Max(0.f, DashLen) : 0.f;
	GapLen  = FMath::IsFinite(GapLen)  ? FMath::Max(0.f, GapLen)  : 0.f;
	const float Period = DashLen + GapLen;

	const FVector2D StepDir = Dir / TotalLen;

	// Degenerate pattern (no length to advance by): fall back to a solid line so the
	// border is still visible instead of either drawing nothing or hanging.
	if (Period <= KINDA_SMALL_NUMBER)
	{
		FCanvasLineItem LineItem(Start, End);
		LineItem.LineThickness = Thickness;
		LineItem.SetColor(Color);
		LineItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LineItem);
		return;
	}

	// Hard backstop on the segment count so bad data can never blow up allocation.
	const int32 MaxSegments = FMath::Clamp(FMath::CeilToInt(TotalLen / Period), 0, 4094) + 2;

	float Traveled = 0.f;
	for (int32 i = 0; i < MaxSegments && Traveled < TotalLen; ++i)
	{
		const float ThisDash = FMath::Min(DashLen, TotalLen - Traveled);
		if (ThisDash > 0.f)
		{
			const FVector2D SegStart = Start + StepDir * Traveled;
			const FVector2D SegEnd = Start + StepDir * (Traveled + ThisDash);

			FCanvasLineItem LineItem(SegStart, SegEnd);
			LineItem.LineThickness = Thickness;
			LineItem.SetColor(Color);
			LineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(LineItem);
		}

		Traveled += Period;
	}
}

void AHUDBase::DrawDashDottedLine2D(const FVector2D& Start, const FVector2D& End, float DashLen, float GapLen, FLinearColor Color, float Thickness)
{
	if (!Canvas) return;

	const FVector2D Dir = (End - Start);
	const float TotalLen = Dir.Size();
	if (TotalLen <= KINDA_SMALL_NUMBER) return;

	// Sanitize inputs: a non-positive (or non-finite) dash+dot+gap period would never
	// advance Traveled, turning the loop into an unbounded Canvas->DrawItem flood that
	// exhausts memory. NaN/Inf are routed into the solid-line fallback below.
	DashLen = FMath::IsFinite(DashLen) ? FMath::Max(0.f, DashLen) : 0.f;
	GapLen  = FMath::IsFinite(GapLen)  ? FMath::Max(0.f, GapLen)  : 0.f;
	const float DotLen = FMath::IsFinite(Thickness) ? FMath::Max(0.f, Thickness) : 0.f;
	const float Period = DashLen + DotLen + 2.f * GapLen;

	const FVector2D StepDir = Dir / TotalLen;

	// Degenerate pattern (no length to advance by): fall back to a solid line so the
	// border is still visible instead of either drawing nothing or hanging.
	if (Period <= KINDA_SMALL_NUMBER)
	{
		FCanvasLineItem LineItem(Start, End);
		LineItem.LineThickness = Thickness;
		LineItem.SetColor(Color);
		LineItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LineItem);
		return;
	}

	// Hard backstop on the segment count so bad data can never blow up allocation.
	const int32 MaxSegments = FMath::Clamp(FMath::CeilToInt(TotalLen / Period), 0, 4094) + 2;

	float Traveled = 0.f;
	for (int32 i = 0; i < MaxSegments && Traveled < TotalLen; ++i)
	{
		// Dash
		const float ThisDash = FMath::Min(DashLen, TotalLen - Traveled);
		if (ThisDash > 0.f)
		{
			FCanvasLineItem LineItem(Start + StepDir * Traveled, Start + StepDir * (Traveled + ThisDash));
			LineItem.LineThickness = Thickness;
			LineItem.SetColor(Color);
			LineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(LineItem);
			Traveled += ThisDash;
		}

		// Gap
		Traveled += GapLen;
		if (Traveled >= TotalLen) break;

		// Dot
		const float ThisDot = FMath::Min(DotLen, TotalLen - Traveled);
		if (ThisDot > 0.f)
		{
			FCanvasLineItem LineItem(Start + StepDir * Traveled, Start + StepDir * (Traveled + ThisDot));
			LineItem.LineThickness = Thickness;
			LineItem.SetColor(Color);
			LineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(LineItem);
			Traveled += ThisDot;
		}

		// Gap
		Traveled += GapLen;
	}
}

void AHUDBase::AddClickIndicator(FVector Location, FColor Color, float LifeTime, float Radius, UMaterialInterface* MaterialOverride)
{
	FClickIndicator NewIndicator;
	NewIndicator.Location   = Location;
	NewIndicator.Color      = Color;

	const float Now         = GetWorld()->GetTimeSeconds();
	NewIndicator.StartTime  = Now;
	NewIndicator.ExpiryTime = Now + (LifeTime < 0.f ? ClickIndicatorLifeTime : LifeTime);
	NewIndicator.Radius     = (Radius   < 0.f ? ClickIndicatorRadius   : Radius);

	// Per-call override wins, else the HUD-wide default. Null => solid-ring fallback.
	UMaterialInterface* Mat = MaterialOverride ? MaterialOverride : ClickIndicatorMaterial.Get();
	if (Mat)
	{
		// One MID per click — created here, reused every frame of this indicator's life (no per-frame churn).
		NewIndicator.MID = UMaterialInstanceDynamic::Create(Mat, this);
		if (NewIndicator.MID)
		{
			NewIndicator.MID->SetScalarParameterValue(ClickIndicatorProgressParamName, 0.f);
			NewIndicator.MID->SetScalarParameterValue(ClickIndicatorRadiusParamName, NewIndicator.Radius);
			NewIndicator.MID->SetVectorParameterValue(ClickIndicatorColorParamName, FLinearColor(Color));
		}
	}

	ClickIndicators.Add(NewIndicator);
}

void AHUDBase::DrawProjectedCircle(const FVector& Location, float Radius, FColor Color, float Thickness, int32 InSegments, bool bDisableSizeCulling)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas) return;

	FVector2D ScreenCenter, ScreenAxisX, ScreenAxisY;
	FVector ZOff(0, 0, 15.f);
	if (!PC->ProjectWorldLocationToScreen(Location + ZOff, ScreenCenter)) return;
    
	PC->ProjectWorldLocationToScreen(Location + ZOff + FVector(Radius, 0, 0), ScreenAxisX);
	PC->ProjectWorldLocationToScreen(Location + ZOff + FVector(0, Radius, 0), ScreenAxisY);

	FVector2D VecX = ScreenAxisX - ScreenCenter;
	FVector2D VecY = ScreenAxisY - ScreenCenter;

	// Size Culling
	if (!bDisableSizeCulling && VecX.SizeSquared() < 16.f && VecY.SizeSquared() < 16.f) return;

	const int32 Segments = FMath::Clamp((InSegments > 0) ? InSegments : 16, 3, 256);
	const float AngleStep = 2.0f * PI / Segments;
	
	float CosStep, SinStep;
	FMath::SinCos(&SinStep, &CosStep, AngleStep);
	
	float CosCurrent = 1.0f;
	float SinCurrent = 0.0f;

	FVector2D PrevPoint;
	bool bPrevValid = false;

	float ThicknessToUse = (Thickness < 0.f) ? ClickIndicatorThickness : Thickness;

	FLinearColor LineColor(Color);
	LineColor.A = (Color.A / 255.f);

	for (int32 i = 0; i <= Segments; i++)
	{
		FVector2D ScreenPoint = ScreenCenter + (CosCurrent * VecX) + (SinCurrent * VecY);

		if (bPrevValid)
		{
			FCanvasLineItem LineItem(PrevPoint, ScreenPoint);
			LineItem.LineThickness = ThicknessToUse;
			LineItem.SetColor(LineColor);
			LineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(LineItem);
		}
		PrevPoint = ScreenPoint;
		bPrevValid = true;

		float NextCos = CosCurrent * CosStep - SinCurrent * SinStep;
		float NextSin = SinCurrent * CosStep + CosCurrent * SinStep;
		CosCurrent = NextCos;
		SinCurrent = NextSin;
	}
}

void AHUDBase::DrawMaterialDisc(const FClickIndicator& Indicator)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas || !Indicator.MID) return;

	const float R = Indicator.Radius;
	const FVector Base = Indicator.Location + FVector(0, 0, 2.f); // tiny lift, mirrors DrawProjectedCircle's Z bias

	// Four ground-plane corners (XY quad) -> screen. Ground projection gives true camera foreshortening.
	FVector2D S00, S10, S11, S01;
	if (!PC->ProjectWorldLocationToScreen(Base + FVector(-R, -R, 0), S00)) return;
	if (!PC->ProjectWorldLocationToScreen(Base + FVector( R, -R, 0), S10)) return;
	if (!PC->ProjectWorldLocationToScreen(Base + FVector( R,  R, 0), S11)) return;
	if (!PC->ProjectWorldLocationToScreen(Base + FVector(-R,  R, 0), S01)) return;

	FCanvasUVTri Tri0;
	Tri0.V0_Pos = S00; Tri0.V0_UV = FVector2D(0, 0);
	Tri0.V1_Pos = S10; Tri0.V1_UV = FVector2D(1, 0);
	Tri0.V2_Pos = S11; Tri0.V2_UV = FVector2D(1, 1);
	Tri0.V0_Color = Tri0.V1_Color = Tri0.V2_Color = FLinearColor::White;

	FCanvasUVTri Tri1;
	Tri1.V0_Pos = S00; Tri1.V0_UV = FVector2D(0, 0);
	Tri1.V1_Pos = S11; Tri1.V1_UV = FVector2D(1, 1);
	Tri1.V2_Pos = S01; Tri1.V2_UV = FVector2D(0, 1);
	Tri1.V0_Color = Tri1.V1_Color = Tri1.V2_Color = FLinearColor::White;

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(2);
	Tris.Add(Tri0);
	Tris.Add(Tri1);

	Canvas->K2_DrawMaterialTriangle(Indicator.MID, Tris);
}

void AHUDBase::EmitRibbonStrip(const TArray<FVector2D>& Pts, const TArray<float>& CumU,
                               UMaterialInterface* Material, const FLinearColor& Color, float WidthPx)
{
	if (!Canvas || !Material || Pts.Num() < 2 || Pts.Num() != CumU.Num()) return;

	const int32 N   = Pts.Num();
	const float Half = FMath::Max(0.5f, WidthPx * 0.5f);

	// Rotate a screen-space direction 90 deg -> normal.
	auto Perp = [](const FVector2D& D) { return FVector2D(-D.Y, D.X); };

	// Per-vertex miter offset (already scaled by Half).
	TArray<FVector2D, TInlineAllocator<64>> Offset;
	Offset.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D dPrev = (i > 0)     ? (Pts[i]   - Pts[i-1]).GetSafeNormal() : FVector2D::ZeroVector;
		const FVector2D dNext = (i < N - 1) ? (Pts[i+1] - Pts[i]  ).GetSafeNormal() : FVector2D::ZeroVector;
		const FVector2D nA = Perp(dPrev);
		const FVector2D nB = Perp(dNext);

		FVector2D M;
		float MiterScale = 1.f;
		if      (i == 0)     M = nB;                 // start cap
		else if (i == N - 1) M = nA;                 // end cap
		else
		{
			M = (nA + nB).GetSafeNormal();
			if (M.IsNearlyZero()) { M = nB; }        // ~180 deg fold -> just use one side
			else
			{
				const float Cos = FVector2D::DotProduct(M, nB);   // == cos(theta/2)
				MiterScale = (FMath::Abs(Cos) > KINDA_SMALL_NUMBER) ? (1.f / Cos) : 1.f;
				MiterScale = FMath::Clamp(MiterScale, 1.f, 4.f);  // clamp spikes at sharp corners
			}
		}
		Offset[i] = M * (Half * MiterScale);
	}

	RibbonScratch.Reset();
	RibbonScratch.Reserve((N - 1) * 2);
	for (int32 i = 0; i < N - 1; ++i)
	{
		const FVector2D L0 = Pts[i]   + Offset[i];
		const FVector2D R0 = Pts[i]   - Offset[i];
		const FVector2D L1 = Pts[i+1] + Offset[i+1];
		const FVector2D R1 = Pts[i+1] - Offset[i+1];
		const float U0 = CumU[i];
		const float U1 = CumU[i+1];

		// Tri A: L0 -> R0 -> R1  (consistent CCW; Canvas tri raster uses CM_None, so not culled)
		FCanvasUVTri A;
		A.V0_Pos = L0; A.V0_UV = FVector2D(U0, 0.f); A.V0_Color = Color;
		A.V1_Pos = R0; A.V1_UV = FVector2D(U0, 1.f); A.V1_Color = Color;
		A.V2_Pos = R1; A.V2_UV = FVector2D(U1, 1.f); A.V2_Color = Color;
		RibbonScratch.Add(A);

		// Tri B: L0 -> R1 -> L1
		FCanvasUVTri B;
		B.V0_Pos = L0; B.V0_UV = FVector2D(U0, 0.f); B.V0_Color = Color;
		B.V1_Pos = R1; B.V1_UV = FVector2D(U1, 1.f); B.V1_Color = Color;
		B.V2_Pos = L1; B.V2_UV = FVector2D(U1, 0.f); B.V2_Color = Color;
		RibbonScratch.Add(B);
	}

	if (RibbonScratch.Num() == 0) return;

	FCanvasTriangleItem TriItem(RibbonScratch, (const FTexture*)nullptr);
	TriItem.MaterialRenderProxy = Material->GetRenderProxy();  // FMaterialRenderProxy* -> const* OK
	TriItem.BlendMode = SE_BLEND_Translucent;                  // ignored on material path; harmless
	Canvas->DrawItem(TriItem);
}

void AHUDBase::DrawRibbonLine3D(const TArray<FVector>& WorldPoints, UMaterialInterface* Material,
                                const FLinearColor& Color, float WidthPx, float ZOffset)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas || !Material || WorldPoints.Num() < 2) return;

	const bool  bWorldTiling = (RibbonTilingMode == ERibbonTilingMode::WorldSpace);
	const float SafeTiling   = (FMath::IsFinite(RibbonMaterialTiling) && RibbonMaterialTiling > 1.f)
	                           ? RibbonMaterialTiling : 64.f;

	TArray<FVector2D> Run;
	TArray<float>     RunU;
	Run.Reserve(WorldPoints.Num());
	RunU.Reserve(WorldPoints.Num());

	float     Accum   = 0.f;
	FVector2D PrevS   = FVector2D::ZeroVector;
	FVector   PrevW   = FVector::ZeroVector;
	bool      bHavePrev = false;

	auto FlushRun = [&]()
	{
		if (Run.Num() >= 2) EmitRibbonStrip(Run, RunU, Material, Color, WidthPx);
		Run.Reset(); RunU.Reset(); Accum = 0.f; bHavePrev = false;
	};

	const int32 MaxPts = FMath::Min(WorldPoints.Num(), FMath::Max(2, RibbonMaxPoints));
	for (int32 i = 0; i < MaxPts; ++i)
	{
		FVector W = WorldPoints[i]; W.Z += ZOffset;
		FVector2D S;
		// Break the run on any non-projectable (behind-camera) or non-finite point.
		if (!PC->ProjectWorldLocationToScreen(W, S) || S.ContainsNaN())
		{
			FlushRun();
			continue;
		}

		if (bHavePrev)
		{
			const float d = bWorldTiling ? FVector::Dist(PrevW, W)
			                             : FVector2D::Distance(PrevS, S);
			Accum += d / SafeTiling;
		}
		Run.Add(S);
		RunU.Add(Accum);
		PrevS = S; PrevW = W; bHavePrev = true;
	}
	FlushRun();
}

void AHUDBase::DrawRibbonLine2D(const TArray<FVector2D>& ScreenPoints, UMaterialInterface* Material,
                                const FLinearColor& Color, float WidthPx)
{
	if (!Canvas || !Material || ScreenPoints.Num() < 2) return;
	const float SafeTiling = (FMath::IsFinite(RibbonMaterialTiling) && RibbonMaterialTiling > 1.f)
	                         ? RibbonMaterialTiling : 64.f;

	const int32 N = FMath::Min(ScreenPoints.Num(), FMath::Max(2, RibbonMaxPoints));
	TArray<float> CumU; CumU.SetNumUninitialized(N);
	CumU[0] = 0.f;
	for (int32 i = 1; i < N; ++i)
		CumU[i] = CumU[i-1] + FVector2D::Distance(ScreenPoints[i-1], ScreenPoints[i]) / SafeTiling;

	TArray<FVector2D> Pts(ScreenPoints.GetData(), N);
	EmitRibbonStrip(Pts, CumU, Material, Color, WidthPx);
}

FVector AHUDBase::ResolveBuildingWaypointOrigin(const AUnitBase* Unit, const ABuildingBase* Config, bool bAllowSocket) const
{
	if (!IsValid(Unit))
	{
		return FVector::ZeroVector;
	}

	const FVector ActorLoc = Unit->GetActorLocation();
	const FRotator ActorRot = Unit->GetActorRotation();

	// No config source -> HUD global default in the unit's actor frame.
	if (!Config)
	{
		return ActorLoc + ActorRot.RotateVector(WPLineDefaultOriginOffset);
	}

	// 1) Socket path (finished buildings only). GetMesh() is the unit's OWN mesh, so the socket
	//    location comes back already in world space at the correct instance.
	if (bAllowSocket && Config->WaypointLineOriginSocket != NAME_None)
	{
		if (const USkeletalMeshComponent* MeshComp = Unit->GetMesh())
		{
			if (MeshComp->DoesSocketExist(Config->WaypointLineOriginSocket))
			{
				return MeshComp->GetSocketLocation(Config->WaypointLineOriginSocket);
			}
		}
		// Socket named but unresolved (e.g. ISM-only building) -> fall through to offset.
	}

	// 2) Offset path. Per-building offset wins when authored; otherwise HUD global default.
	const FVector LocalOffset = Config->WaypointLineOriginOffset.IsNearlyZero()
		? WPLineDefaultOriginOffset
		: Config->WaypointLineOriginOffset;

	return ActorLoc + ActorRot.RotateVector(LocalOffset);
}

void AHUDBase::DrawSelectedBuildingWaypointLinks()
{
	if (SelectedUnits.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayerController();
	if (!World || !PC)
	{
		return;
	}

	for (AUnitBase* Unit : SelectedUnits)
	{
		if (!IsValid(Unit)) continue;

		// Frustum Culling: Gebäude im Bild?
		FVector2D Dummy;
		if (!PC->ProjectWorldLocationToScreen(Unit->GetActorLocation(), Dummy)) continue;

		// Draw the rally line for finished buildings (HasWaypoint) and for construction sites
		// whose finished building has HasWaypoint (the rally the site carries until handoff).
		// WaypointConfig carries the origin/target offset + socket settings; for a construction
		// site that config is the finished building's CDO (its own mesh is not the final mesh).
		bool bDrawWaypoint = false;
		const ABuildingBase* WaypointConfig = nullptr;
		bool bAllowSocketOrigin = false;

		if (const ABuildingBase* Building = Cast<ABuildingBase>(Unit))
		{
			bDrawWaypoint       = Building->HasWaypoint;
			WaypointConfig      = Building;
			bAllowSocketOrigin  = true;
		}
		else if (Unit->bIsConstructionUnit)
		{
			if (const AConstructionUnit* Site = Cast<AConstructionUnit>(Unit))
			{
				if (Site->WorkArea && Site->WorkArea->BuildingClass)
				{
					if (const ABuildingBase* BuildingCDO = Site->WorkArea->BuildingClass->GetDefaultObject<ABuildingBase>())
					{
						bDrawWaypoint       = BuildingCDO->HasWaypoint;
						WaypointConfig      = BuildingCDO;
						bAllowSocketOrigin  = false; // CDO mesh is not in-world / not the site mesh
					}
				}
			}
		}
		if (!bDrawWaypoint)
		{
			continue;
		}

		AWaypoint* WP = Unit->NextWaypoint;
		if (!IsValid(WP) || WP->IsHidden())
		{
			continue;
		}

		// Designer-controlled origin (socket or local offset) instead of the raw actor pivot.
		const FVector Start = ResolveBuildingWaypointOrigin(Unit, WaypointConfig, bAllowSocketOrigin);

		// Optional symmetric target offset on the waypoint end (default zero = legacy).
		FVector End = WP->GetActorLocation();
		if (WaypointConfig && !WaypointConfig->WaypointLineTargetOffset.IsNearlyZero())
		{
			End += WP->GetActorRotation().RotateVector(WaypointConfig->WaypointLineTargetOffset);
		}

		// Trace to see if the line clips through the landscape
		// We use an offset to avoid hitting the ground immediately at the start/end points
		const FVector TraceEnd = End + FVector(0.f, 0.f, WPLineZOffset);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Unit);
		Params.AddIgnoredActor(WP);

		TArray<FVector> Points;
		Points.Add(Start);

		FVector CurrentTraceStart = Start + FVector(0.f, 0.f, WPLineZOffset);

		for (int32 i = 0; i < WPLineMaxIterations; ++i)
		{
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, CurrentTraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				// If we hit something, add a midpoint above the hit location
				const FVector MidPoint = Hit.Location + FVector(0.f, 0.f, WPLineCollisionZOffset);
				Points.Add(MidPoint);
				
				// Update next trace start point
				CurrentTraceStart = MidPoint + FVector(0.f, 0.f, WPLineZOffset);
			}
			else
			{
				break;
			}
		}

		Points.Add(End);

		// Draw all segments
		if (WPLineMaterial)
		{
			DrawRibbonLine3D(Points, WPLineMaterial, FLinearColor(WPLineColor), WPLineRibbonWidth, WPLineZOffset);
		}
		else
		{
			for (int32 i = 0; i < Points.Num() - 1; ++i)
			{
				DrawDashedLine3D(Points[i], Points[i + 1], WPLineDashLen, WPLineGapLen, WPLineColor, WPLineThickness, WPLineZOffset);
			}
		}
	}
}

void AHUDBase::DrawSelectedUnitsMovementLines()
{
	if (SelectedUnits.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayerController();
	if (!World || !PC)
	{
		return;
	}

	// Compute group center
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (AUnitBase* Unit : SelectedUnits)
	{
		if (IsValid(Unit))
		{
			Sum += Unit->GetActorLocation();
			++Count;
		}
	}
	if (Count <= 0)
	{
		return;
	}
	const FVector GroupCenter = Sum / float(Count);

	// Frustum Culling: Zentrum im Bild?
	FVector2D Dummy;
	if (!PC->ProjectWorldLocationToScreen(GroupCenter, Dummy))
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	const FMassUnitPathFragment* FoundPath = nullptr;

	for (AUnitBase* Unit : SelectedUnits)
	{
		if (!IsValid(Unit) || !Unit->bIsMassUnit) continue;
		const FMassEntityHandle Handle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		if (!EntityManager.IsEntityValid(Handle)) continue;
		const FMassUnitPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FMassUnitPathFragment>(Handle);
		if (PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex)
		{
			FoundPath = PathFrag;
			break;
		}
	}

	if (!FoundPath)
	{
		return;
	}

	TArray<FVector> Points;
	Points.Reserve(FoundPath->Waypoints.Num() - FoundPath->CurrentIndex + 1);
	Points.Add(GroupCenter);
	for (int32 i = FoundPath->CurrentIndex; i < FoundPath->Waypoints.Num(); ++i)
	{
		Points.Add(FoundPath->Waypoints[i]);
	}

	const FColor LineColor = FoundPath->bAttackMoveDuringPath ? UnitWPLineColorAttackMove : UnitWPLineColorMove;

	if (UnitWPLineMaterial)
	{
		DrawRibbonLine3D(Points, UnitWPLineMaterial, FLinearColor(LineColor), UnitWPLineRibbonWidth, UnitWPLineZOffset);
	}
	else
	{
		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			DrawDashedLine3D(Points[i], Points[i + 1], UnitWPLineDashLen, UnitWPLineGapLen, LineColor, UnitWPLineThickness, UnitWPLineZOffset);
		}
	}
}

void AHUDBase::SetExtensionPreviewLine(FVector Start, FVector End, FColor Color, float HeightOffset)
{
	ExtensionPreviewLine.Start = Start;
	ExtensionPreviewLine.End = End;
	ExtensionPreviewLine.Color = Color;
	ExtensionPreviewLine.HeightOffset = HeightOffset;
	ExtensionPreviewLine.bIsActive = true;
}

void AHUDBase::DrawSelectionIndicator(AUnitBase* Unit, const FVector& Location, float RadiusX, float RadiusY, const FRotator& Rotation, const FSelectionSettings& Settings, bool bDisableOcclusionOverride, int32 InSegments)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerCameraManager || !Canvas) return;

	// 1. Projektions-Basis berechnen (Nur 3 Calls statt 64!)
	FVector2D ScreenCenter, ScreenAxisX, ScreenAxisY;
	FVector ZOff(0, 0, 10.f);
	if (!PC->ProjectWorldLocationToScreen(Location + ZOff, ScreenCenter)) return;
    
	FRotator YawRot(0, Rotation.Yaw, 0);
	PC->ProjectWorldLocationToScreen(Location + ZOff + YawRot.RotateVector(FVector(RadiusX, 0, 0)), ScreenAxisX);
	PC->ProjectWorldLocationToScreen(Location + ZOff + YawRot.RotateVector(FVector(0, RadiusY, 0)), ScreenAxisY);

	FVector2D VecX = ScreenAxisX - ScreenCenter;
	FVector2D VecY = ScreenAxisY - ScreenCenter;

	// --- OPTIMIERUNG: Size Culling ---
	// Wenn der Kreis kleiner als ein paar Pixel ist, abbrechen
	if (VecX.SizeSquared() < 16.f && VecY.SizeSquared() < 16.f) return;

	// 2. Occlusion-Vorbereitung
	float CamAngleRad = 0.f;
	bool bOcclusionActive = Settings.bEnableOcclusion && !bDisableOcclusionOverride;
	if (bOcclusionActive) {
		FVector DirToCam = (PC->PlayerCameraManager->GetCameraLocation() - Location).GetSafeNormal2D();
		CamAngleRad = FMath::Atan2(DirToCam.Y, DirToCam.X);
	}

	// Höhere Segmentanzahl für rotierende Stile für weiches Fading erzwingen
	int32 Segments = FMath::Clamp((InSegments > 0) ? InSegments : 32, 3, 256);
	if (InSegments <= 0) {
		if (Settings.Style == ESelectionIndicatorStyle::RotatingPartialCircle || Settings.Style == ESelectionIndicatorStyle::RotatingOctagon)
			Segments = 64;
		else if (Settings.Style == ESelectionIndicatorStyle::Octagon)
			Segments = 8;
	}
    
	float AngleStep = 2.0f * PI / Segments;
	float RotOffset = (Settings.Style == ESelectionIndicatorStyle::RotatingPartialCircle || Settings.Style == ESelectionIndicatorStyle::RotatingOctagon) 
					  ? GetWorld()->GetTimeSeconds() * FMath::DegreesToRadians(Settings.RotatingSpeed) : 0.0f;

	// --- OPTIMIERUNG: Iterative Trigonometrie Vorbereitung ---
	float CosStep, SinStep;
	FMath::SinCos(&SinStep, &CosStep, AngleStep);
    
	float CosCurrent, SinCurrent;
	FMath::SinCos(&SinCurrent, &CosCurrent, RotOffset);

	FVector2D PrevPoint;
	float PrevAlphaMult = 0.f;
	bool bPrevValid = false;

	for (int32 i = 0; i <= Segments; i++) {
		float WorldAngle = (i * AngleStep) + RotOffset + FMath::DegreesToRadians(Rotation.Yaw);
		float AlphaMult = 1.0f;

		// 3. Occlusion Fade (Exponential)
		if (bOcclusionActive) {
			float Diff = FMath::Abs(FMath::UnwindRadians(WorldAngle - CamAngleRad));
			const float MaxOccl = FMath::DegreesToRadians(120.0f); 
			const float FadeZone = FMath::DegreesToRadians(30.0f); // 30 Grad Übergangszone
            
			if (Diff > MaxOccl) AlphaMult = 0.0f;
			else if (Diff > MaxOccl - FadeZone) {
				float T = 1.0f - (Diff - (MaxOccl - FadeZone)) / FadeZone;
				AlphaMult *= (T * T);
			}
		}

		// 4. Clipping Fade für Partial Circle (Exponential)
		if (Settings.Style == ESelectionIndicatorStyle::RotatingPartialCircle) {
			float Deg = FMath::Fmod(FMath::RadiansToDegrees(i * AngleStep), 360.0f);
			if (Deg > 240.0f) AlphaMult = 0.0f;
			else if (Deg > 200.0f) {
				float T = 1.0f - (Deg - 200.0f) / 40.0f;
				AlphaMult *= (T * T);
			}
		}

		// 5. Screen-Space Berechnung
		float CosA, SinA;
		if (Settings.Style == ESelectionIndicatorStyle::Octagon || Settings.Style == ESelectionIndicatorStyle::RotatingOctagon) {
			float OctStep = 2.0f * PI / 8.0f;
			int32 CornerIdx = FMath::FloorToInt((i * AngleStep) / OctStep);
			float A1 = CornerIdx * OctStep + RotOffset;
			float A2 = (CornerIdx + 1) * OctStep + RotOffset;
			float Alpha = ((i * AngleStep) + RotOffset - A1) / (A2 - A1);
			CosA = FMath::Lerp(FMath::Cos(A1), FMath::Cos(A2), Alpha);
			SinA = FMath::Lerp(FMath::Sin(A1), FMath::Sin(A2), Alpha);
		} else {
			CosA = CosCurrent;
			SinA = SinCurrent;
		}

		FVector2D ScreenPoint = ScreenCenter + (CosA * VecX) + (SinA * VecY);

		// 6. Zeichnen mit gemitteltem Alpha/Thickness
		if (bPrevValid && (AlphaMult > 0.001f || PrevAlphaMult > 0.001f)) {
			float AvgAlpha = (AlphaMult + PrevAlphaMult) * 0.5f;
			FCanvasLineItem LineItem(PrevPoint, ScreenPoint);
			LineItem.LineThickness = Settings.Thickness * AvgAlpha;
			FLinearColor LineColor = Settings.Color;
			LineColor.A *= AvgAlpha;
			LineItem.SetColor(LineColor);
			LineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(LineItem);
		}
		
		PrevPoint = ScreenPoint; 
		PrevAlphaMult = AlphaMult;
		bPrevValid = true;

		// --- OPTIMIERUNG: Berechne nächsten Cos/Sin über Rotation-Matrix ---
		float NextCos = CosCurrent * CosStep - SinCurrent * SinStep;
		float NextSin = SinCurrent * CosStep + CosCurrent * SinStep;
		CosCurrent = NextCos;
		SinCurrent = NextSin;
	}
}

void AHUDBase::DrawAllSelectedUnitsIndicators()
{
	if (!bEnableStandardSelection) return;

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerCameraManager || SelectedUnits.Num() == 0) return;

	// Bestimmung der Genauigkeit basierend auf der Anzahl der Einheiten
	int32 GlobalSegments = 64; // Höchste Genauigkeit (bis 25 Einheiten)
	if (SelectedUnits.Num() > 50)
	{
		GlobalSegments = 12;   // Niedrigste Genauigkeit (über 50 Einheiten)
	}
	else if (SelectedUnits.Num() > 25)
	{
		GlobalSegments = 32;   // Mittlere Genauigkeit (26 bis 50 Einheiten)
	}

	for (int32 i = SelectedUnits.Num() - 1; i >= 0; --i)
	{
		AUnitBase* Unit = SelectedUnits[i];
		if (!IsValid(Unit)) { 
			SelectedUnits.RemoveAtSwap(i); 
			SelectedUnitsSet.Remove(Unit);
			continue; 
		}

		FVector DrawLocation = Unit->GetActorLocation();

		// 1. Frustum Culling (Grobe Prüfung ob Einheit im Bild)
		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(DrawLocation, ScreenPos)) continue;

		FRotator UnitRotation = Unit->GetActorRotation();
		float FinalRadiusX = 60.f;
		float FinalRadiusY = 60.f;
		bool bIsFlying = false;

		// 1. Größe aus Fragment beziehen
		if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit))
		{
			if (MassUnit->MassActorBindingComponent)
			{
				FMassEntityHandle Entity = MassUnit->MassActorBindingComponent->GetEntityHandle();
				if (EM.IsEntityValid(Entity))
				{
					if (const FMassAgentCharacteristicsFragment* Frag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity))
					{
						bIsFlying = Frag->bIsFlying;
						// Auf Boden projizieren
						DrawLocation.Z = Frag->LastGroundLocation;

						// Construction sites: draw the FINISHED building's footprint (HUD-only
						// override seeded at spawn). Gameplay reach/hover keep using the unit's
						// own capsule/box below.
						if (Frag->IndicatorFootprintOverride.X > KINDA_SMALL_NUMBER &&
							Frag->IndicatorFootprintOverride.Y > KINDA_SMALL_NUMBER)
						{
							FinalRadiusX = Frag->IndicatorFootprintOverride.X;
							FinalRadiusY = Frag->IndicatorFootprintOverride.Y;
						}
						// Unterscheidung Box vs. Capsule
						else if (Frag->bUseBoxComponent)
						{
							FinalRadiusX = Frag->BoxExtent.X;
							FinalRadiusY = Frag->BoxExtent.Y;
						}
						else
						{
							FinalRadiusX = FinalRadiusY = Frag->CapsuleRadius;
						}
					}
				}
			}
		}

		FSelectionSettings EffectiveSettings;
		if (!Unit->CanMove) EffectiveSettings = BuildingSelectionSettings;
		else if (bIsFlying) EffectiveSettings = FlyingSelectionSettings;
		else EffectiveSettings = GroundSelectionSettings;

		DrawSelectionIndicator(
			Unit,
			DrawLocation, 
			FinalRadiusX * EffectiveSettings.SizeMultiplier, 
			FinalRadiusY * EffectiveSettings.SizeMultiplier, 
			UnitRotation, 
			EffectiveSettings,
			bIsFlying,
			GlobalSegments
		);
	}
}

void AHUDBase::HandleSelectionRectangle()
{
	if (!bSelectFriendly) return;

	DeselectAllUnits();
	SelectedUnits.Empty();
	SelectedUnitsSet.Empty();
	CurrentPoint = GetMousePos2D();

	if (FMath::Abs(InitialPoint.X - CurrentPoint.X) >= 2)
	{
		// Zentrum und volle Größe berechnen
		FVector2D Center = (InitialPoint + CurrentPoint) * 0.5f;
		FVector2D FullSize = CurrentPoint - InitialPoint;

		// Skaliertes Rechteck berechnen
		FVector2D ScaledSize = FullSize * RectangleScaleSelectionFactor;
		FVector2D InitialSelectionPoint = Center - (ScaledSize * 0.5f);
		FVector2D CurrentSelectionPoint = Center + (ScaledSize * 0.5f);

		// 1. Zeichnen des Rechtecks (Nur das Selektions-Rechteck behalten)
		if (SelectionRectSettings.bDrawFill)
		{
			DrawRect(SelectionRectSettings.FillColor, InitialSelectionPoint.X, InitialSelectionPoint.Y,
				CurrentSelectionPoint.X - InitialSelectionPoint.X,
				CurrentSelectionPoint.Y - InitialSelectionPoint.Y);
		}

		if (SelectionRectSettings.bDrawBorder)
		{
			// Zeichne die 4 Linien des Rechtecks
			const FVector2D TopLeft = InitialSelectionPoint;
			const FVector2D TopRight(CurrentSelectionPoint.X, InitialSelectionPoint.Y);
			const FVector2D BottomRight = CurrentSelectionPoint;
			const FVector2D BottomLeft(InitialSelectionPoint.X, CurrentSelectionPoint.Y);

			if (SelectionRectSettings.BorderType == Line)
			{
				DrawLine(TopLeft.X, TopLeft.Y, TopRight.X, TopRight.Y, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				DrawLine(TopRight.X, TopRight.Y, BottomRight.X, BottomRight.Y, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				DrawLine(BottomRight.X, BottomRight.Y, BottomLeft.X, BottomLeft.Y, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				DrawLine(BottomLeft.X, BottomLeft.Y, TopLeft.X, TopLeft.Y, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
			}
			else
			{
				float Dash = SelectionRectSettings.DashLength;
				float Gap = SelectionRectSettings.GapLength;

				if (SelectionRectSettings.BorderType == Dotted)
				{
					Dash = SelectionRectSettings.BorderThickness;
					Gap = SelectionRectSettings.GapLength;

					DrawDashedLine2D(TopLeft, TopRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(TopRight, BottomRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(BottomRight, BottomLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(BottomLeft, TopLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				}
				else if (SelectionRectSettings.BorderType == DashDotted)
				{
					DrawDashDottedLine2D(TopLeft, TopRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashDottedLine2D(TopRight, BottomRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashDottedLine2D(BottomRight, BottomLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashDottedLine2D(BottomLeft, TopLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				}
				else // Dashed
				{
					DrawDashedLine2D(TopLeft, TopRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(TopRight, BottomRight, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(BottomRight, BottomLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
					DrawDashedLine2D(BottomLeft, TopLeft, Dash, Gap, SelectionRectSettings.BorderColor, SelectionRectSettings.BorderThickness);
				}
			}
		}

		// 2. Selektions-Logik
		TArray<AUnitBase*> NewUnitBases;
		GetActorsInSelectionRectangle<AUnitBase>(InitialSelectionPoint, CurrentSelectionPoint, NewUnitBases, false, false);

		ACameraControllerBase* Controller = Cast<ACameraControllerBase>(GetOwningPlayerController());
		const FVector2D SelectionRectMin(FMath::Min(InitialSelectionPoint.X, CurrentSelectionPoint.X), FMath::Min(InitialSelectionPoint.Y, CurrentSelectionPoint.Y));
		const FVector2D SelectionRectMax(FMath::Max(InitialSelectionPoint.X, CurrentSelectionPoint.X), FMath::Max(InitialSelectionPoint.Y, CurrentSelectionPoint.Y));

		for (AUnitBase* Unit : NewUnitBases) {
			if (!Unit) continue;
			//const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Unit);
			if (Controller && Unit->CanBeSelected && Unit->bUseSkeletalMovement && (Unit->TeamId == Controller->SelectableTeamId || Controller->SelectableTeamId == 0)) //  && !SUnit
			{
				FVector2D ScreenLocation;
				if (Controller->ProjectWorldLocationToScreen(Unit->GetActorLocation(), ScreenLocation))
				{
					if (ScreenLocation.X >= SelectionRectMin.X && ScreenLocation.X <= SelectionRectMax.X &&
						ScreenLocation.Y >= SelectionRectMin.Y && ScreenLocation.Y <= SelectionRectMax.Y)
					{
						//if (Unit->GetOwner() == nullptr) Unit->SetOwner(Controller);
						Unit->SetSelected();
						SelectedUnits.Emplace(Unit);
						SelectedUnitsSet.Add(Unit);
						SelectUnitsFromSameSquad(Unit);
					}
				}
			}
		}

		NewUnitBases.Empty();
		if (Controller) Controller->AbilityArrayIndex = 0;
	}

	// Mass-Units/ISM Logik
	SelectISMUnitsInRectangle(InitialPoint, CurrentPoint);
}

void AHUDBase::DrawMaterialWall(const FVector& Start, const FVector& End, float Height, UMaterialInterface* Material, const FLinearColor& Color)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas || !Material) return;

	const FVector Up(0.f, 0.f, FMath::Max(1.f, Height));

	// 4 world corners: bottom Start/End, then up to the top. Project all; bail if any is behind camera.
	FVector2D S0, S1, S2, S3;
	if (!PC->ProjectWorldLocationToScreen(Start,      S0)) return; // bottom A
	if (!PC->ProjectWorldLocationToScreen(End,        S1)) return; // bottom B
	if (!PC->ProjectWorldLocationToScreen(End + Up,   S2)) return; // top B
	if (!PC->ProjectWorldLocationToScreen(Start + Up, S3)) return; // top A
	if (S0.ContainsNaN() || S1.ContainsNaN() || S2.ContainsNaN() || S3.ContainsNaN()) return;

	FCanvasUVTri T0;
	T0.V0_Pos = S0; T0.V0_UV = FVector2D(0.f, 1.f); T0.V0_Color = Color;
	T0.V1_Pos = S1; T0.V1_UV = FVector2D(1.f, 1.f); T0.V1_Color = Color;
	T0.V2_Pos = S2; T0.V2_UV = FVector2D(1.f, 0.f); T0.V2_Color = Color;

	FCanvasUVTri T1;
	T1.V0_Pos = S0; T1.V0_UV = FVector2D(0.f, 1.f); T1.V0_Color = Color;
	T1.V1_Pos = S2; T1.V1_UV = FVector2D(1.f, 0.f); T1.V1_Color = Color;
	T1.V2_Pos = S3; T1.V2_UV = FVector2D(0.f, 0.f); T1.V2_Color = Color;

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(2);
	Tris.Add(T0);
	Tris.Add(T1);

	FCanvasTriangleItem TriItem(Tris, (const FTexture*)nullptr);
	TriItem.MaterialRenderProxy = Material->GetRenderProxy();
	TriItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(TriItem);
}

void AHUDBase::DrawHUD()
{
	Super::DrawHUD();

	DrawAllSelectedUnitsIndicators();
	DrawAllHealthBars();

	if (ExtensionPreviewLine.bIsActive)
	{
		if (ExtensionWallMaterial)
		{
			// Translucent vertical "wall" between the two pillars, tinted by the validity color.
			// Drop the base below the pivots so the wall starts at the ground, not mid-height.
			const float WallH = (ExtensionWallHeight > 0.f) ? ExtensionWallHeight : (ExtensionPreviewLine.HeightOffset * 2.f);
			const FVector BaseDrop(0.f, 0.f, ExtensionWallBaseDrop);
			FLinearColor WallColor(ExtensionPreviewLine.Color);
			WallColor.A = ExtensionWallOpacity;
			DrawMaterialWall(ExtensionPreviewLine.Start - BaseDrop, ExtensionPreviewLine.End - BaseDrop, WallH, ExtensionWallMaterial, WallColor);
		}
		else if (ExtensionLineMaterial)
		{
			TArray<FVector> P = { ExtensionPreviewLine.Start, ExtensionPreviewLine.End };
			DrawRibbonLine3D(P, ExtensionLineMaterial, FLinearColor(ExtensionPreviewLine.Color), ExtensionLineRibbonWidth, 0.f);
		}
		else
		{
			DrawDashedLine3D(ExtensionPreviewLine.Start, ExtensionPreviewLine.End, ExtensionLineDashLen, ExtensionLineGapLen, ExtensionPreviewLine.Color, ExtensionLineThickness, 0.f);
			DrawDashedLine3D(ExtensionPreviewLine.Start, ExtensionPreviewLine.End, ExtensionLineDashLen, ExtensionLineGapLen, ExtensionPreviewLine.Color, ExtensionLineThickness, ExtensionPreviewLine.HeightOffset * 2.f);
		}
		ExtensionPreviewLine.bIsActive = false;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	for (int32 i = ClickIndicators.Num() - 1; i >= 0; --i)
	{
		FClickIndicator& CI = ClickIndicators[i];
		if (CurrentTime >= CI.ExpiryTime)
		{
			ClickIndicators.RemoveAtSwap(i);   // element (and its MID ref) drops -> MID becomes GC-eligible
			continue;
		}

		if (CI.MID)
		{
			// Feed a per-click 0..1 phase. SetScalarParameterValue is a no-op if the param is absent, so this is safe.
			const float Total = FMath::Max(CI.ExpiryTime - CI.StartTime, KINDA_SMALL_NUMBER);
			const float Age01 = FMath::Clamp((CurrentTime - CI.StartTime) / Total, 0.f, 1.f);
			CI.MID->SetScalarParameterValue(ClickIndicatorProgressParamName, Age01);
			DrawMaterialDisc(CI);
		}
		else
		{
			DrawProjectedCircle(CI.Location, CI.Radius, CI.Color, -1.f, -1, true);   // unchanged fallback
		}
	}

	// Draw dashed links between selected buildings and their waypoints each frame
	DrawSelectedBuildingWaypointLinks();
	DrawSelectedUnitsMovementLines();

	HandleSelectionRectangle();
}

void AHUDBase::SelectISMUnitsInRectangle(const FVector2D& RectMin, const FVector2D& RectMax)
{
	//UE_LOG(LogTemp, Error, TEXT("SelectISMUnitsInRectangle!!!!!!!!!"));
    APlayerController* PC = GetOwningPlayerController();
    ACameraControllerBase* Controller = Cast<ACameraControllerBase>(PC);
    if (!Controller)
        return;

    // Iterate all MassUnitBase actors (which includes UnitBase)
	for (TActorIterator<AMassUnitBase> It(GetWorld()); It; ++It)
    {
        AMassUnitBase* Unit = *It;
        if (!Unit) continue;

        if (Unit->bUseSkeletalMovement || (Controller->SelectableTeamId != 0 && Unit->TeamId != Controller->SelectableTeamId))
            continue;

        if (AUnitBase* UB = Cast<AUnitBase>(Unit))
        {
            if (!UB->CanBeSelected) continue;
        }

        // Test each instance from VisualInstances in fragment
        const FMassUnitVisualFragment* VisualFrag = Unit->GetVisualFragment();
        if (!VisualFrag) continue;

        for (const FMassUnitVisualInstance& VisualInst : VisualFrag->VisualInstances)
        {
            UInstancedStaticMeshComponent* ISM = VisualInst.TargetISM.Get();
            if (!ISM) continue;
            int32 Index = VisualInst.InstanceIndex;

        {
            FTransform Xform;
            ISM->GetInstanceTransform(Index, Xform, /*bWorldSpace=*/true);
            FVector WorldLoc = Xform.GetLocation();


            FVector2D ScreenLoc;
            if (!PC->ProjectWorldLocationToScreen(WorldLoc, ScreenLoc))
                continue;

            // Screen‑space AABB test
            if (ScreenLoc.X >= FMath::Min(RectMin.X, RectMax.X) &&
                ScreenLoc.X <= FMath::Max(RectMin.X, RectMax.X) &&
                ScreenLoc.Y >= FMath::Min(RectMin.Y, RectMax.Y) &&
                ScreenLoc.Y <= FMath::Max(RectMin.Y, RectMax.Y))
            {
                // Select this unit only if not already selected to reduce network traffic
                if (AUnitBase* UnitBase = Cast<AUnitBase>(Unit))
                {
                    if (!SelectedUnits.Contains(UnitBase))
                    {
                        if (UnitBase->GetOwner() == nullptr)
                        {
                            UnitBase->SetOwner(Controller);
                        }
                        UnitBase->SetSelected();
                        SelectedUnits.AddUnique(UnitBase);
                        SelectedUnitsSet.Add(UnitBase);
                        SelectUnitsFromSameSquad(UnitBase);
                    }
                }
                break;  // one hit is enough
            }
        }
    }
    }
}

void AHUDBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(GameMode)
	{
		MoveUnitsThroughWayPoints(FriendlyUnits);
		IsSpeakingUnitClose(FriendlyUnits, GameMode->SpeakingUnits);
	}
}



void AHUDBase::BeginPlay()
{
	Super::BeginPlay();
	
	AddUnitsToArray();
}

void AHUDBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AHUDBase::SelectUnitsFromSameSquad(AUnitBase* SelectedUnit)
{
	if(!bSelectFullSquad || !SelectedUnit)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[HUD] SelectUnitsFromSameSquad aborted. bSelectFullSquad=%d SelectedUnit=%s"), bSelectFullSquad ? 1 : 0, SelectedUnit ? *SelectedUnit->GetName() : TEXT("NULL"));
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	//UE_LOG(LogTemp, Log, TEXT("[HUD] Forwarding SelectUnitsFromSameSquad for %s to PC server RPC. PC=%s HasAuthority=%d"), *SelectedUnit->GetName(), PC ? *PC->GetName() : TEXT("NULL"), HasAuthority());
	if(ACameraControllerBase* CamPC = Cast<ACameraControllerBase>(PC))
	{
		CamPC->Server_SelectUnitsFromSameSquad(SelectedUnit);
	}
	else
	{
		//UE_LOG(LogTemp, Error, TEXT("[HUD] CameraControllerBase not found on owning PC. Cannot request same-squad selection."));
	}
}

void AHUDBase::AddUnitsToArray()
{
	if (!HasAuthority()) return; // On client, units register themselves
	
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(GameMode)
	for(int i = 0; i < GameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
		RegisterUnit(Unit);
	}
}

void AHUDBase::RegisterUnit(AUnitBase* Unit)
{
	if (!Unit) return;

	// In RTS, we might not know the team immediately or it might change.
	// But for the sake of these arrays, we use the TeamId.
	// In the template TeamId > 0 is usually friendly, TeamId == 0 is enemy.
	if (Unit->TeamId > 0)
	{
		FriendlyUnits.AddUnique(Unit);
		EnemyUnitBases.Remove(Unit);
	}
	else
	{
		EnemyUnitBases.AddUnique(Unit);
		FriendlyUnits.Remove(Unit);
	}
}

void AHUDBase::UnregisterUnit(AUnitBase* Unit)
{
	if (!Unit) return;
	FriendlyUnits.Remove(Unit);
	EnemyUnitBases.Remove(Unit);
	SelectedUnits.Remove(Unit);
	SelectedUnitsSet.Remove(Unit);
}


FVector2D AHUDBase::GetMousePos2D()
{
	float PosX;
	float PosY;
	GetOwningPlayerController()->GetMousePosition(PosX, PosY);
	return FVector2D(PosX, PosY);
}

void AHUDBase::MoveUnitsThroughWayPoints(TArray <AUnitBase*> Units)
{
	for (int32 i = 0; i < Units.Num(); i++) {

		if(Units[i])
		if (Units[i]->RunLocationArray.Num()) {

			FVector ActorLocation = Units[i]->GetActorLocation();
			const float DistSq = FVector::DistSquared2D(ActorLocation, Units[i]->RunLocation);

			if (DistSq <= FMath::Square(Units[i]->MovementAcceptanceRadius)) { 
				if (Units[i]->RunLocationArrayIterator < Units[i]->RunLocationArray.Num()) {
					
					Units[i]->RunLocation = Units[i]->RunLocationArray[Units[i]->RunLocationArrayIterator];
					Units[i]->SetUnitState(UnitData::Run);
				}
				else {
					Units[i]->RunLocationArray.Empty();
					Units[i]->RunLocationArrayIterator = 0;
				}
				Units[i]->RunLocationArrayIterator++;
			}
		}


	}
}

void AHUDBase::IsSpeakingUnitClose(TArray <AUnitBase*> Units, TArray <ASpeakingUnit*> SpeakUnits)
{
	for (int32 i = 0; i < SpeakUnits.Num(); i++)
	{
	
		
		if(SpeakUnits[i]){
			bool FoundUnit = false;
			for (int32 u = 0; u < Units.Num(); u++) {

				const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Units[u]);
				
				if(Units[u] && Units[u]->TeamId && !SUnit)
				{
					const FVector UnitLocation = Units[u]->GetActorLocation();
					const FVector SpeakingUnitLocation = SpeakUnits[i]->GetActorLocation();
					const float MinSpeakDistSq = FMath::Square(SpeakUnits[i]->MinSpeakDistance);
					const float DistSq = FVector::DistSquared2D(UnitLocation, SpeakingUnitLocation);
					
					if(DistSq < MinSpeakDistSq && SpeakUnits[i]->SpeechBubble && SpeakUnits[i]->SpeechBubble->WidgetIndex != 2)
					{
						SpeakUnits[i]->SpeechBubble->WidgetIndex = 1;
						SpeakUnits[i]->SpeechBubble->WidgetSwitcher->SetActiveWidgetIndex(1);
						FoundUnit = true;
					}else if(SpeakUnits[i]->SpeechBubble->WidgetIndex != 2 && !FoundUnit)
					{
						SpeakUnits[i]->SpeechBubble->WidgetIndex = 0;
						SpeakUnits[i]->SpeechBubble->WidgetSwitcher->SetActiveWidgetIndex(0);
					}
				}
			}
		}
	}
}

void AHUDBase::PatrolUnitsThroughWayPoints(TArray <AUnitBase*> Units)
{
	for (int32 i = 0; i < Units.Num(); i++) {

		if(Units[i])
		if (Units[i]->RunLocationArray.Num()) {

			FVector ActorLocation = Units[i]->GetActorLocation();
			const float DistSq = FVector::DistSquared2D(ActorLocation, Units[i]->RunLocation);

			if (DistSq <= FMath::Square(Units[i]->MovementAcceptanceRadius)) { // || DistanceY <= Units[i]->StopRunToleranceY 
				if (Units[i]->RunLocationArrayIterator < Units[i]->RunLocationArray.Num()) {
					
					Units[i]->RunLocation = Units[i]->RunLocationArray[Units[i]->RunLocationArrayIterator];
					Units[i]->SetUEPathfinding = true;
					Units[i]->SetUnitState(UnitData::Patrol);
				}
				else {
					Units[i]->RunLocationArray.Empty();
					Units[i]->RunLocationArrayIterator = 0;
				}
				Units[i]->RunLocationArrayIterator++;
			}
		}


	}
}

void AHUDBase::SetUnitSelected(AUnitBase* Unit, bool bIsAi)
{
	if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayerController()))
	{
		PC->Batch_RemoveRotateToMouseTag();
	}

	// Deselect and prune any invalid pointers first
	for (int32 i = SelectedUnits.Num() - 1; i >= 0; --i)
	{
		AUnitBase* Sel = SelectedUnits.IsValidIndex(i) ? SelectedUnits[i] : nullptr;
		if (IsValid(Sel))
		{
			Sel->SetDeselected();
		}
		else
		{
			SelectedUnits.RemoveAtSwap(i);
			SelectedUnitsSet.Remove(Sel);
		}
	}

	SelectedUnits.Empty();
	SelectedUnitsSet.Empty();

	// Add and select the new unit if valid
	if (IsValid(Unit))
	{
		SelectedUnits.Add(Unit);
		SelectedUnitsSet.Add(Unit);
		if (!bIsAi) Unit->SetSelected();
		SelectUnitsFromSameSquad(Unit);
	}
}

void AHUDBase::DeselectAllUnits()
{
	if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayerController()))
	{
		PC->Batch_RemoveRotateToMouseTag();
	}

	if (CharacterIsUnSelectable)
	{
		for (int32 i = SelectedUnits.Num() - 1; i >= 0; --i)
		{
			AUnitBase* Sel = SelectedUnits.IsValidIndex(i) ? SelectedUnits[i] : nullptr;
			if (IsValid(Sel))
			{
				Sel->SetDeselected();
			}
			else
			{
				SelectedUnits.RemoveAtSwap(i);
				SelectedUnitsSet.Remove(Sel);
			}
		}

		SelectedUnits.Empty();
		SelectedUnitsSet.Empty();
	}
}

bool AHUDBase::IsActorInsideRec(FVector InPoint, FVector CuPoint, FVector ALocation)
{	
	if(InPoint.X < ALocation.X && ALocation.X < CuPoint.X && InPoint.Y < ALocation.Y && ALocation.Y < CuPoint.Y) return true;
	if(InPoint.X > ALocation.X && ALocation.X > CuPoint.X && InPoint.Y > ALocation.Y && ALocation.Y > CuPoint.Y) return true;
	if(InPoint.X < ALocation.X && ALocation.X < CuPoint.X && InPoint.Y > ALocation.Y && ALocation.Y > CuPoint.Y) return true;
	if(InPoint.X > ALocation.X && ALocation.X > CuPoint.X && InPoint.Y < ALocation.Y && ALocation.Y < CuPoint.Y) return true;

	return false;
}

void AHUDBase::DrawAllHealthBars()
{
	if (!bEnableHealthBars || !Canvas) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager* EM = EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;

	// 1. Predictive LOD basierend auf dem letzten Frame
	float SegmentScale = 1.f;
	if (LastFrameVisibleCount >= 50) SegmentScale = 0.25f;
	else if (LastFrameVisibleCount >= 25) SegmentScale = 0.5f;

	// 2. Kamera-Daten cachen
	const FVector RightV = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector);
	const FVector UpV = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::UpVector);
	int32 ViewX = 0, ViewY = 0;
	PC->GetViewportSize(ViewX, ViewY);

	int32 CurrentVisibleCount = 0;

	auto DrawBarsForArray = [&](const TArray<AUnitBase*>& Units) {
		for (int32 i = Units.Num() - 1; i >= 0; --i)
		{
			AUnitBase* Unit = Units[i];
			if (!IsValid(Unit) || !Unit->Attributes || Unit->Attributes->GetHealth() <= 0.f) continue;

			const bool bIsSelected = SelectedUnitsSet.Contains(Unit);

			// Early viewport check as requested
			if (!Unit->IsOnViewport) continue;

			// Sichtbarkeits-Check (Nur Team oder sichtbare Gegner mit kleiner Hysterese)
			const float CurrentTime = GetWorld()->GetTimeSeconds();
			const bool bVisible = Unit->IsMyTeam || Unit->IsVisibleEnemy || (CurrentTime - Unit->LastVisibleTime < 0.2f);

			// Allow selected units, or OWN-TEAM units under "show all", to bypass the visibility gate.
			// The permanent show-all must NOT reveal fogged/hidden ENEMY bars, so it is restricted to
			// this player's own team (Unit->IsMyTeam is computed local-player-relative by the Mass
			// visibility processor); enemies still require bVisible (revealed) or bIsSelected.
			if (!bVisible && !bIsSelected && !(bShowAllHealthBarsPermanent && Unit->IsMyTeam)) continue;

			// Einstellungs-Auswahl (Vorläufige Bestimmung)
			bool bIsBuilding = !Unit->CanMove;
			bool bIsFlying = Unit->IsFlying;
			float FinalRadiusX = 60.f;
			float FinalRadiusY = 60.f;
			float FinalRadiusZ = 60.f;
			float LastGroundLocationZ = Unit->GetActorLocation().Z;

			// Direkter Komponenten-Zugriff für Größe
			if (UBoxComponent* BoxComp = Unit->BoxCollisionComponent)
			{
				const FVector Extent = BoxComp->GetUnscaledBoxExtent();
				FinalRadiusX = Extent.X;
				FinalRadiusY = Extent.Y;
				FinalRadiusZ = Extent.Z;
			}
			else if (ACharacter* Character = Cast<ACharacter>(Unit))
			{
				FinalRadiusX = FinalRadiusY = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
				FinalRadiusZ = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}

			if (EM)
			{
				if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit))
				{
					if (MassUnit->MassActorBindingComponent)
					{
						FMassEntityHandle Entity = MassUnit->MassActorBindingComponent->GetEntityHandle();
						if (EM->IsEntityValid(Entity))
						{
							if (DoesEntityHaveTag(*EM, Entity, FMassStateDeadTag::StaticStruct())) continue;

							if (const FMassAgentCharacteristicsFragment* Frag = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity))
							{
								bIsFlying = Frag->bIsFlying;
								LastGroundLocationZ = Frag->LastGroundLocation;
								if (Frag->bUseBoxComponent)
								{
									FinalRadiusX = Frag->BoxExtent.X;
									FinalRadiusY = Frag->BoxExtent.Y;
									FinalRadiusZ = Frag->BoxExtent.Z;
								}
								else
								{
									FinalRadiusX = FinalRadiusY = Frag->CapsuleRadius;
									FinalRadiusZ = Frag->CapsuleHeight * 0.5f;
								}
							}
						}
					}
				}
			}

			FHealthBarSettings EffectiveSettings;
			if (Unit->bIsConstructionUnit) EffectiveSettings = ConstructionHealthBarSettings;
			else if (bIsBuilding) EffectiveSettings = BuildingHealthBarSettings;
			else if (bIsFlying) EffectiveSettings = FlyingHealthBarSettings;
			else EffectiveSettings = GroundHealthBarSettings;

			// Under permanent "show all", draw own-team units always AND currently-revealed enemies
			// (Unit->IsVisibleEnemy is a client-local, per-frame FOW signal). Gate 1 above already culled
			// fogged enemies, so adding IsVisibleEnemy here makes an out-of-FOW enemy show a permanent bar
			// and re-hide when it re-enters the fog — without needing a damage/level popup.
			if (!(bShowAllHealthBarsPermanent && (Unit->IsMyTeam || Unit->IsVisibleEnemy)) && !Unit->OpenHealthWidget && !Unit->bShowLevelOnly && !(EffectiveSettings.bShowHealthbarOnSelected && bIsSelected)) continue;

			// 3. Einzige Projektion für diesen Frame
			FVector2D ScreenPos;
			FVector BaseLoc = Unit->GetActorLocation();
			if (EffectiveSettings.Style == EHealthBarStyle::SemiCircle)
			{
				BaseLoc.Z = LastGroundLocationZ;
			}
			BaseLoc.Z += EffectiveSettings.HeightOffset;

			if (!PC->ProjectWorldLocationToScreen(BaseLoc, ScreenPos)) continue;
			
			// Screen-Culling
			if (ScreenPos.X < 0.f || ScreenPos.X > (float)ViewX || ScreenPos.Y < 0.f || ScreenPos.Y > (float)ViewY) continue;

			CurrentVisibleCount++;

			// LOD anwenden
			int32 BaseSegments = EffectiveSettings.Segments;
			if (EffectiveSettings.Style == EHealthBarStyle::SemiCircle && BaseSegments <= 0) BaseSegments = 32;
			if (BaseSegments > 0)
			{
				// Clamp the upper bound too: an unclamped (e.g. Blueprint-set) Segments would
				// otherwise drive billions of per-unit, per-frame Canvas->DrawItem calls.
				EffectiveSettings.Segments = FMath::Clamp(FMath::FloorToInt(BaseSegments * SegmentScale), 4, 256);
			}

			float MaxRadius = FMath::Max(FinalRadiusX, FinalRadiusY);

			if (EffectiveSettings.Style == EHealthBarStyle::SemiCircle)
			{
				// Kreis parallel zum Boden: Welt-Basisvektoren verwenden
				FVector BasisX = FVector(1, 0, 0);
				FVector BasisY = FVector(0, 1, 0);

				DrawSemiCircleHealthBar(Unit, BaseLoc, ScreenPos, FinalRadiusX, FinalRadiusY, bIsFlying, EffectiveSettings, BasisX, BasisY);
			}
			else if (EffectiveSettings.Style == EHealthBarStyle::Stacked)
			{
				DrawStackedHealthBar(Unit, BaseLoc, ScreenPos, MaxRadius, EffectiveSettings, RightV);
			}
			else if (EffectiveSettings.Style == EHealthBarStyle::SideBrackets)
			{
				DrawSideBracketsHealthBar(Unit, BaseLoc, ScreenPos, FinalRadiusZ, MaxRadius, EffectiveSettings, RightV, UpV);
			}
		}
	};

	DrawBarsForArray(FriendlyUnits);
	DrawBarsForArray(EnemyUnitBases);

	LastFrameVisibleCount = CurrentVisibleCount;
}

void AHUDBase::DrawStackedHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float WorldRadius, const FHealthBarSettings& Settings, const FVector& RightV)
{
	ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit);
	const bool bIsSelected = SelectedUnitsSet.Contains(Unit);

	if (LevelUnit && LevelUnit->bShowLevelOnly && !bShowAllHealthBarsPermanent && !(Settings.bShowHealthbarOnSelected && bIsSelected))
	{
		DrawLevelText(Unit, ScreenPos, Settings);
		return;
	}

	UAttributeSetBase* Attr = Unit->Attributes;
	float Health = Attr->GetHealth();
	float MaxHealth = Attr->GetMaxHealth();
	float Shield = Attr->GetShield();
	float MaxShield = Attr->GetMaxShield();
	float Mana = Attr->GetMana();
	float MaxMana = Attr->GetMaxMana();

	if (MaxHealth <= 0.f) return;

	float HealthPct = FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Shield / MaxShield, 0.f, 1.f) : 0.f;
	float ManaPct = (MaxMana > 0.f) ? FMath::Clamp(Mana / MaxMana, 0.f, 1.f) : 0.f;

	HealthPct = GetHysteresisPct(HealthPct, Unit->DisplayedHealthPct, Settings);
	ShieldPct = (MaxShield > 0.f) ? GetHysteresisPct(ShieldPct, Unit->DisplayedShieldPct, Settings) : 0.f;
	ManaPct = (MaxMana > 0.f) ? GetHysteresisPct(ManaPct, Unit->DisplayedManaPct, Settings) : 0.f;

	APlayerController* PC = GetOwningPlayerController();
	float ProjectedSize = 60.f;
	if (PC)
	{
		FVector2D Edge;
		// BaseLoc bereits mit HeightOffset
		if (PC->ProjectWorldLocationToScreen(BaseLoc + RightV * WorldRadius, Edge))
		{
			ProjectedSize = (Edge - ScreenPos).Size() * 2.0f;
		}
	}

	ProjectedSize = FMath::Clamp(ProjectedSize * Settings.Scale, Settings.MinScreenSize, Settings.MaxScreenSize);

	float Thickness = Settings.Thickness * Settings.Scale;
	float Outline = Settings.bShowOutline ? (Settings.OutlineThickness * Settings.Scale) : 0.f;

	auto DrawSegmentedBar = [&](FVector2D InPos, float Pct, FColor Color)
	{
		if (Settings.bShowOutline)
		{
			FCanvasTileItem OutlineItem(InPos - FVector2D(Outline, Outline), FVector2D(ProjectedSize + Outline * 2.f, Thickness + Outline * 2.f), FLinearColor(Settings.OutlineColor));
			OutlineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(OutlineItem);
		}

		// Hintergrund der Bar (deckt das massive Outline innen ab)
		FCanvasTileItem BackgroundItem(InPos, FVector2D(ProjectedSize, Thickness), FLinearColor(Settings.BackgroundColor));
		BackgroundItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(BackgroundItem);

		if (Settings.Segments > 0)
		{
			float TotalSize = ProjectedSize;
			float SegSize = TotalSize / Settings.Segments;
			int32 FilledSegs = FMath::CeilToInt(Settings.Segments * Pct);
			float ScaledSpace = Settings.SegmentSpace * Settings.Scale;

			for (int32 i = 0; i < FilledSegs; i++)
			{
				// Beim letzten Segment der Bar keinen Abstand nach rechts lassen
				float CurrentGap = (i < Settings.Segments - 1) ? ScaledSpace : 0.f;
				FVector2D SegPos = InPos + FVector2D(i * SegSize, 0);
				FVector2D SegDim = FVector2D(FMath::Max(1.0f, SegSize - CurrentGap), Thickness);

				FCanvasTileItem SegItem(SegPos, SegDim, FLinearColor(Color));
				SegItem.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(SegItem);
			}
		}
		else
		{
			FVector2D FillDim = FVector2D(ProjectedSize * Pct, Thickness);
			FCanvasTileItem FillItem(InPos, FillDim, FLinearColor(Color));
			FillItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(FillItem);
		}
	};

	// Klassisch Horizontal übereinander
	FVector2D Pos = ScreenPos;
	Pos.X -= ProjectedSize * 0.5f;
	Pos.Y -= 60.f;

	if (MaxShield > 0.f)
	{
		DrawSegmentedBar(Pos, ShieldPct, Settings.ShieldColor);
		Pos.Y += Thickness + (Settings.BarPadding * Settings.Scale) + (Outline * 2.f);
	}
	DrawSegmentedBar(Pos, HealthPct, Settings.HealthColor);

	if (MaxMana > 0.f && !Settings.bDisableManaBar)
	{
		Pos.Y += Thickness + (Settings.BarPadding * Settings.Scale) + (Outline * 2.f);
		DrawSegmentedBar(Pos, ManaPct, Settings.ManaColor);
	}

	if (LevelUnit)
	{
		FVector2D LevelPos = ScreenPos;
		LevelPos.X -= (60.f * Settings.Scale) * 0.5f;
		LevelPos.Y -= 65.f * Settings.Scale;
		DrawLevelText(Unit, LevelPos, Settings);
	}
}

void AHUDBase::DrawSemiCircleHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float RadiusX, float RadiusY, bool bIsFlying, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV)
{
	ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit);
	const bool bIsSelected = SelectedUnitsSet.Contains(Unit);

	if (LevelUnit && LevelUnit->bShowLevelOnly && !bShowAllHealthBarsPermanent && !(Settings.bShowHealthbarOnSelected && bIsSelected))
	{
		DrawLevelText(Unit, ScreenPos, Settings);
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas) return;

	UAttributeSetBase* Attr = Unit->Attributes;
	float HealthPct = FMath::Clamp(Attr->GetHealth() / Attr->GetMaxHealth(), 0.f, 1.f);
	float MaxShield = Attr->GetMaxShield();
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Attr->GetShield() / MaxShield, 0.f, 1.f) : 0.f;
	float MaxMana = Attr->GetMaxMana();
	float ManaPct = (MaxMana > 0.f) ? FMath::Clamp(Attr->GetMana() / MaxMana, 0.f, 1.f) : 0.f;

	HealthPct = GetHysteresisPct(HealthPct, Unit->DisplayedHealthPct, Settings);
	ShieldPct = (MaxShield > 0.f) ? GetHysteresisPct(ShieldPct, Unit->DisplayedShieldPct, Settings) : 0.f;
	ManaPct = (MaxMana > 0.f) ? GetHysteresisPct(ManaPct, Unit->DisplayedManaPct, Settings) : 0.f;

	float BaseRadiusX = RadiusX * Settings.RadiusMultiplier;
	float BaseRadiusY = RadiusY * Settings.RadiusMultiplier;
	float Thickness = Settings.Thickness * Settings.Scale;
	float Outline = Settings.bShowOutline ? (Settings.OutlineThickness * Settings.Scale) : 0.f;
	
	FVector Loc = BaseLoc;
	FVector ZOff(0, 0, 10.f);

	FVector2D Right2D, Up2D;
	if (PC->ProjectWorldLocationToScreen(Loc + ZOff + RightV * BaseRadiusX, Right2D) &&
		PC->ProjectWorldLocationToScreen(Loc + ZOff + UpV * BaseRadiusY, Up2D))
	{
		FVector2D VecX_Base = (Right2D - ScreenPos) * Settings.Scale;
		FVector2D VecY_Base = (Up2D - ScreenPos) * Settings.Scale;

		// Size Culling / Clamping basierend auf der längeren Achse
		float MaxProjSize = FMath::Max(VecX_Base.Size(), VecY_Base.Size());
		if (MaxProjSize < 1.f) return;

		float ClampedSize = FMath::Clamp(MaxProjSize, Settings.MinScreenSize * 0.5f, Settings.MaxScreenSize * 0.5f);
		float SizeScale = ClampedSize / MaxProjSize;
		VecX_Base *= SizeScale;
		VecY_Base *= SizeScale;

		const float CamAngleRad = Settings.bFaceCamera ?
			FMath::Atan2(PC->PlayerCameraManager->GetCameraLocation().Y - Loc.Y, PC->PlayerCameraManager->GetCameraLocation().X - Loc.X) : 0.f;
		const float StartAngleRad = FMath::DegreesToRadians(Settings.RotationOffset) + (Settings.bFaceCamera ? (CamAngleRad - PI * 0.5f) : 0.f);

		auto DrawArc2D = [&](float RadiusMult, float Pct, FColor Color)
		{
			FVector2D EX = VecX_Base * RadiusMult;
			FVector2D EY = VecY_Base * RadiusMult;

			int32 Segments = Settings.Segments > 0 ? Settings.Segments : 32;
			const float TotalAngle = PI;
			const float SegAngle = TotalAngle / (float)Segments;
			
			// Gap-Berechnung basierend auf Screen-Space Radius (grob gemittelt)
			float AvgRadius = (EX.Size() + EY.Size()) * 0.5f;
			float ScaledGapSpace = Settings.SegmentSpace * Settings.Scale * 2.0f;
			float GapAngle = (AvgRadius > 0.f) ? (ScaledGapSpace / AvgRadius) : 0.f;
			GapAngle = FMath::Clamp(GapAngle, 0.035f, SegAngle * 0.4f);
			const float DrawAngle = SegAngle - GapAngle;

			// Hintergrund-Bogen (und Outline)
			for (int32 s = 0; s < Segments; ++s)
			{
				const float A0 = StartAngleRad + s * SegAngle;
				const float CurrentAngle = (s < Segments - 1) ? DrawAngle : SegAngle;
				const float A1 = A0 + CurrentAngle;
				const FVector2D P0 = ScreenPos + EX * FMath::Cos(A0) + EY * FMath::Sin(A0);
				const FVector2D P1 = ScreenPos + EX * FMath::Cos(A1) + EY * FMath::Sin(A1);

				if (Settings.bShowOutline)
				{
					FCanvasLineItem OutlineItem(P0, P1);
					OutlineItem.LineThickness = Thickness + Outline * 2.f;
					OutlineItem.SetColor(FLinearColor(Settings.OutlineColor));
					Canvas->DrawItem(OutlineItem);
				}

				FCanvasLineItem BackgroundItem(P0, P1);
				BackgroundItem.LineThickness = Thickness;
				BackgroundItem.SetColor(FLinearColor(Settings.BackgroundColor));
				Canvas->DrawItem(BackgroundItem);
			}

			// Gefüllte Segmente
			int32 FilledSegs = FMath::CeilToInt(Segments * Pct);
			for (int32 s = 0; s < FilledSegs; ++s)
			{
				const float A0 = StartAngleRad + s * SegAngle;
				const float CurrentAngle = (s < Segments - 1) ? DrawAngle : SegAngle;
				const float A1 = A0 + CurrentAngle;
				const FVector2D P0 = ScreenPos + EX * FMath::Cos(A0) + EY * FMath::Sin(A0);
				const FVector2D P1 = ScreenPos + EX * FMath::Cos(A1) + EY * FMath::Sin(A1);

				FCanvasLineItem LineItem(P0, P1);
				LineItem.LineThickness = Thickness;
				LineItem.SetColor(FLinearColor(Color));
				Canvas->DrawItem(LineItem);
			}
		};

		if (MaxMana > 0.f && !Settings.bDisableManaBar)
		{
			float AvgRadius = (VecX_Base.Size() + VecY_Base.Size()) * 0.5f;
			float ManaRadiusOffset = Thickness + (Settings.BarPadding * Settings.Scale) + (Outline * 2.f);
			float ManaMult = (AvgRadius > 0.f) ? (AvgRadius - ManaRadiusOffset) / AvgRadius : 0.9f;
			DrawArc2D(ManaMult, ManaPct, Settings.ManaColor);
		}

		DrawArc2D(1.0f, HealthPct, Settings.HealthColor);
		if (MaxShield > 0.f)
		{
			// Für den Shield-Bogen berechnen wir den Radius-Multiplikator basierend auf der Screen-Dicke
			float AvgRadius = (VecX_Base.Size() + VecY_Base.Size()) * 0.5f;
			float ShieldRadiusOffset = Thickness + (Settings.BarPadding * Settings.Scale) + (Outline * 2.f);
			float ShieldMult = (AvgRadius > 0.f) ? (AvgRadius + ShieldRadiusOffset) / AvgRadius : 1.1f;
			
			DrawArc2D(ShieldMult, ShieldPct, Settings.ShieldColor);
		}

		if (LevelUnit)
		{
			DrawLevelText(Unit, ScreenPos, Settings);
		}
	}
}

void AHUDBase::DrawSideBracketsHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float WorldRadius, float WorldWidthRadius, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV)
{
	ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit);
	const bool bIsSelected = SelectedUnitsSet.Contains(Unit);

	if (LevelUnit && LevelUnit->bShowLevelOnly && !bShowAllHealthBarsPermanent && !(Settings.bShowHealthbarOnSelected && bIsSelected))
	{
		DrawLevelText(Unit, ScreenPos, Settings);
		return;
	}

	UAttributeSetBase* Attr = Unit->Attributes;
	float HealthPct = FMath::Clamp(Attr->GetHealth() / Attr->GetMaxHealth(), 0.f, 1.f);
	float MaxShield = Attr->GetMaxShield();
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Attr->GetShield() / MaxShield, 0.f, 1.f) : 0.f;
	float MaxMana = Attr->GetMaxMana();
	float ManaPct = (MaxMana > 0.f) ? FMath::Clamp(Attr->GetMana() / MaxMana, 0.f, 1.f) : 0.f;

	HealthPct = GetHysteresisPct(HealthPct, Unit->DisplayedHealthPct, Settings);
	ShieldPct = (MaxShield > 0.f) ? GetHysteresisPct(ShieldPct, Unit->DisplayedShieldPct, Settings) : 0.f;
	ManaPct = (MaxMana > 0.f) ? GetHysteresisPct(ManaPct, Unit->DisplayedManaPct, Settings) : 0.f;

	APlayerController* PC = GetOwningPlayerController();
	float WorldWidth = 40.f;
	float ProjectedHeight = 60.f;

	if (PC)
	{
		FVector2D EdgeWidth, EdgeHeight;
		FVector Loc = BaseLoc;
		
		if (PC->ProjectWorldLocationToScreen(Loc + RightV * WorldWidthRadius, EdgeWidth))
		{
			WorldWidth = (EdgeWidth - ScreenPos).Size() * Settings.Scale;
		}

		if (PC->ProjectWorldLocationToScreen(Loc + UpV * WorldRadius, EdgeHeight))
		{
			ProjectedHeight = (EdgeHeight - ScreenPos).Size() * 2.0f * Settings.Scale;
		}
	}

	ProjectedHeight = FMath::Clamp(ProjectedHeight, Settings.MinScreenSize, Settings.MaxScreenSize);
	WorldWidth = FMath::Max(WorldWidth, Settings.MinScreenSize * 0.5f);

	float BracketHeight = ProjectedHeight;
	float OffsetX = WorldWidth + (Settings.BarPadding * Settings.Scale);
	float Thickness = Settings.Thickness * Settings.Scale;
	float Outline = Settings.bShowOutline ? (Settings.OutlineThickness * Settings.Scale) : 0.f;

	auto DrawSegmentedBracket = [&](FVector2D Top, float Pct, FColor Color)
	{
		if (Settings.bShowOutline)
		{
			FCanvasTileItem OutlineItem(Top - FVector2D(Outline, Outline), FVector2D(Thickness + Outline * 2.f, BracketHeight + Outline * 2.f), FLinearColor(Settings.OutlineColor));
			OutlineItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(OutlineItem);
		}

		// Hintergrund
		FCanvasTileItem BackgroundItem(Top, FVector2D(Thickness, BracketHeight), FLinearColor(Settings.BackgroundColor));
		BackgroundItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(BackgroundItem);

		if (Settings.Segments > 0)
		{
			float SegHeight = BracketHeight / Settings.Segments;
			int32 FilledSegs = FMath::CeilToInt(Settings.Segments * Pct);
			float ScaledSpace = Settings.SegmentSpace * Settings.Scale;

			for (int32 i = 0; i < FilledSegs; i++)
			{
				// Beim untersten Segment (i=0) keinen Abstand nach unten lassen
				float CurrentGap = (i == 0) ? 0.f : ScaledSpace;
				FVector2D SegPos = Top + FVector2D(0, BracketHeight - (i + 1) * SegHeight);
				FVector2D SegDim = FVector2D(Thickness, FMath::Max(1.0f, SegHeight - CurrentGap));
				
				FCanvasTileItem SegItem(SegPos, SegDim, FLinearColor(Color));
				SegItem.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(SegItem);
			}
		}
		else
		{
			// Fill von unten (Hintergrund wurde bereits gezeichnet)
			float FillHeight = BracketHeight * Pct;
			FCanvasTileItem FillItem(Top + FVector2D(0, BracketHeight - FillHeight), FVector2D(Thickness, FillHeight), FLinearColor(Color));
			FillItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(FillItem);
		}
	};

	// Links: Health [
	FVector2D L_Top(ScreenPos.X - OffsetX, ScreenPos.Y - BracketHeight * 0.5f);
	DrawSegmentedBracket(L_Top, HealthPct, Settings.HealthColor);

	// Rechts: Shield ]
	if (MaxShield > 0.f)
	{
		FVector2D R_Top(ScreenPos.X + OffsetX, ScreenPos.Y - BracketHeight * 0.5f);
		DrawSegmentedBracket(R_Top, ShieldPct, Settings.ShieldColor);
		
		if (MaxMana > 0.f && !Settings.bDisableManaBar)
		{
			float ManaOffsetX = OffsetX + Thickness + (Settings.BarPadding * Settings.Scale) + (Outline * 2.f);
			FVector2D R_ManaTop(ScreenPos.X + ManaOffsetX, ScreenPos.Y - BracketHeight * 0.5f);
			DrawSegmentedBracket(R_ManaTop, ManaPct, Settings.ManaColor);
		}
	}
	else if (MaxMana > 0.f && !Settings.bDisableManaBar)
	{
		FVector2D R_Top(ScreenPos.X + OffsetX, ScreenPos.Y - BracketHeight * 0.5f);
		DrawSegmentedBracket(R_Top, ManaPct, Settings.ManaColor);
	}

	if (LevelUnit)
	{
		DrawLevelText(Unit, ScreenPos + FVector2D(0, BracketHeight * 0.5f + 10.f * Settings.Scale), Settings);
	}
}

void AHUDBase::DrawLevelText(AUnitBase* Unit, const FVector2D& ScreenPos, const FHealthBarSettings& Settings)
{
	if (!Settings.bShowLevel || !LevelFont || !Unit) return;

	ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit);
	if (!LevelUnit) return;

	FString LevelStr = LevelUnit->CachedLevelString;
	if (LevelStr.IsEmpty())
	{
		LevelStr = FString::FromInt(LevelUnit->LevelData.CharacterLevel);
	}

	float FontScale = Settings.Scale * Settings.LevelTextScale;
	FVector2D FinalPos = ScreenPos + (Settings.LevelOffset * Settings.Scale);

	FCanvasTextItem TextItem(FinalPos, FText::FromString(LevelStr), LevelFont, Settings.LevelColor);
	TextItem.Scale = FVector2D(FontScale, FontScale);
	TextItem.bOutlined = true;
	TextItem.OutlineColor = FLinearColor::Black;
	TextItem.BlendMode = SE_BLEND_Translucent;

	Canvas->DrawItem(TextItem);
}

float AHUDBase::GetHysteresisPct(float ActualPct, float& DisplayedPct, const FHealthBarSettings& Settings)
{
	if (DisplayedPct < 0.f)
	{
		DisplayedPct = ActualPct;
		return ActualPct;
	}

	if (ActualPct < DisplayedPct)
	{
		// Sinkt sofort
		DisplayedPct = ActualPct;
	}
	else if (ActualPct > DisplayedPct)
	{
		if (Settings.Segments > 0)
		{
			float OneSegmentPct = 1.0f / (float)Settings.Segments;
			float Threshold = OneSegmentPct * Settings.SegmentRefillThreshold;

			// Nächstes Segment Grenze
			float NextSegmentThreshold = FMath::CeilToFloat(DisplayedPct * (float)Settings.Segments) / (float)Settings.Segments;

			if (FMath::IsNearlyEqual(DisplayedPct, NextSegmentThreshold, 0.0001f))
			{
				NextSegmentThreshold += OneSegmentPct;
			}

			if (ActualPct >= NextSegmentThreshold + Threshold || ActualPct >= 1.0f)
			{
				DisplayedPct = ActualPct;
			}
		}
		else
		{
			// Kontinuierlich
			if (ActualPct >= DisplayedPct + 0.01f || ActualPct >= 1.0f)
			{
				DisplayedPct = ActualPct;
			}
		}
	}

	return DisplayedPct;
}


