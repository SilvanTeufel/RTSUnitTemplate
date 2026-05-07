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
	float Total3DLen = FVector::Dist(Start, End);
	float Scale = TotalScreenLen / Total3DLen;

	const FVector2D StepDir = Dir / TotalScreenLen;
	const float ScreenDashLen = FMath::Max(1.f, DashLen * Scale);
	const float ScreenGapLen = FMath::Max(0.f, GapLen * Scale);

	float Traveled = 0.f;
	FLinearColor LineColor(Color);
	LineColor.A = (Color.A / 255.f);

	while (Traveled < TotalScreenLen)
	{
		const float ThisDash = FMath::Min(ScreenDashLen, TotalScreenLen - Traveled);
		const FVector2D SegStart = ScreenStart + StepDir * Traveled;
		const FVector2D SegEnd = ScreenStart + StepDir * (Traveled + ThisDash);

		FCanvasLineItem LineItem(SegStart, SegEnd);
		LineItem.LineThickness = Thickness;
		LineItem.SetColor(LineColor);
		LineItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LineItem);

		Traveled += ThisDash + ScreenGapLen;
	}
}

void AHUDBase::AddClickIndicator(FVector Location, FColor Color, float LifeTime, float Radius)
{
	FClickIndicator NewIndicator;
	NewIndicator.Location = Location;
	NewIndicator.Color = Color;
	NewIndicator.ExpiryTime = GetWorld()->GetTimeSeconds() + (LifeTime < 0.f ? ClickIndicatorLifeTime : LifeTime);
	NewIndicator.Radius = (Radius < 0.f ? ClickIndicatorRadius : Radius);
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

	const int32 Segments = (InSegments > 0) ? InSegments : 16;
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

		ABuildingBase* Building = Cast<ABuildingBase>(Unit);
		if (!Building || !Building->HasWaypoint)
		{
			continue; 
		}

		AWaypoint* WP = Building->NextWaypoint;
		if (!IsValid(WP) || WP->IsHidden())
		{
			continue;
		}

		const FVector Start = Building->GetActorLocation();
		const FVector End = WP->GetActorLocation();

		// Trace to see if the line clips through the landscape
		// We use an offset to avoid hitting the ground immediately at the start/end points
		const FVector TraceEnd = End + FVector(0.f, 0.f, WPLineZOffset);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Building);
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
		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			DrawDashedLine3D(Points[i], Points[i + 1], WPLineDashLen, WPLineGapLen, WPLineColor, WPLineThickness, WPLineZOffset);
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

	for (int32 i = 0; i < Points.Num() - 1; ++i)
	{
		DrawDashedLine3D(Points[i], Points[i + 1], UnitWPLineDashLen, UnitWPLineGapLen, LineColor, UnitWPLineThickness, UnitWPLineZOffset);
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

void AHUDBase::DrawSelectionIndicator(AUnitBase* Unit, const FVector& Location, float RadiusX, float RadiusY, const FRotator& Rotation, FLinearColor Color, float Thickness, bool bDisableOcclusion, int32 InSegments)
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
	bool bOcclusionActive = bEnableOcclusion && !bDisableOcclusion;
	if (bOcclusionActive) {
		FVector DirToCam = (PC->PlayerCameraManager->GetCameraLocation() - Location).GetSafeNormal2D();
		CamAngleRad = FMath::Atan2(DirToCam.Y, DirToCam.X);
	}

	// Höhere Segmentanzahl für rotierende Stile für weiches Fading erzwingen
	int32 Segments = (InSegments > 0) ? InSegments : 32;
	if (InSegments <= 0) {
		if (SelectionStyle == ESelectionIndicatorStyle::RotatingPartialCircle || SelectionStyle == ESelectionIndicatorStyle::RotatingOctagon)
			Segments = 64;
		else if (SelectionStyle == ESelectionIndicatorStyle::Octagon)
			Segments = 8;
	}
    
	float AngleStep = 2.0f * PI / Segments;
	float RotOffset = (SelectionStyle == ESelectionIndicatorStyle::RotatingPartialCircle || SelectionStyle == ESelectionIndicatorStyle::RotatingOctagon) 
					  ? GetWorld()->GetTimeSeconds() * FMath::DegreesToRadians(RotatingCircleSpeed) : 0.0f;

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
		if (SelectionStyle == ESelectionIndicatorStyle::RotatingPartialCircle) {
			float Deg = FMath::Fmod(FMath::RadiansToDegrees(i * AngleStep), 360.0f);
			if (Deg > 240.0f) AlphaMult = 0.0f;
			else if (Deg > 200.0f) {
				float T = 1.0f - (Deg - 200.0f) / 40.0f;
				AlphaMult *= (T * T);
			}
		}

		// 5. Screen-Space Berechnung
		float CosA, SinA;
		if (SelectionStyle == ESelectionIndicatorStyle::Octagon || SelectionStyle == ESelectionIndicatorStyle::RotatingOctagon) {
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
			LineItem.LineThickness = Thickness * AvgAlpha;
			FLinearColor LineColor = Color;
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

						// Unterscheidung Box vs. Capsule
						if (Frag->bUseBoxComponent)
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

		DrawSelectionIndicator(
			Unit,
			DrawLocation, 
			FinalRadiusX * SelectionSizeMultiplier, 
			FinalRadiusY * SelectionSizeMultiplier, 
			UnitRotation, 
			FLinearColor(SelectionColor), 
			SelectionThickness,
			bIsFlying,
			GlobalSegments
		);
	}
}

void AHUDBase::DrawHUD()
{
	Super::DrawHUD();

	DrawAllSelectedUnitsIndicators();
	DrawAllHealthBars();

	if (ExtensionPreviewLine.bIsActive)
	{
		DrawDashedLine3D(ExtensionPreviewLine.Start, ExtensionPreviewLine.End, ExtensionLineDashLen, ExtensionLineGapLen, ExtensionPreviewLine.Color, ExtensionLineThickness, 0.f);
		DrawDashedLine3D(ExtensionPreviewLine.Start, ExtensionPreviewLine.End, ExtensionLineDashLen, ExtensionLineGapLen, ExtensionPreviewLine.Color, ExtensionLineThickness, ExtensionPreviewLine.HeightOffset * 2.f);
		ExtensionPreviewLine.bIsActive = false;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	for (int32 i = ClickIndicators.Num() - 1; i >= 0; --i)
	{
		if (CurrentTime >= ClickIndicators[i].ExpiryTime)
		{
			ClickIndicators.RemoveAtSwap(i);
			continue;
		}
		DrawProjectedCircle(ClickIndicators[i].Location, ClickIndicators[i].Radius, ClickIndicators[i].Color, -1.f, -1, true);
	}

	// Draw dashed links between selected buildings and their waypoints each frame
	DrawSelectedBuildingWaypointLinks();
	DrawSelectedUnitsMovementLines();

	if (bSelectFriendly) {
    
       DeselectAllUnits();
       SelectedUnits.Empty();
       
       CurrentPoint = GetMousePos2D();
       
       if (abs(InitialPoint.X - CurrentPoint.X) >= 2) {

          // This is the visual rectangle the player sees.
          DrawRect(FLinearColor(0, 0, 1, .15f),
             InitialPoint.X,
             InitialPoint.Y,
             CurrentPoint.X - InitialPoint.X,
             CurrentPoint.Y - InitialPoint.Y);

          // ... (The complex calculation for the scaled selection rectangle remains the same)
          const float LengthLineA = abs(InitialPoint.Y - CurrentPoint.Y);
          const float LengthLineB = abs(InitialPoint.X - CurrentPoint.X);
      		  FVector2D LineCenterPointA;
      		  FVector2D LineCenterPointB;
      		  FVector2D InitialSelectionPoint = FVector2D::ZeroVector;
      		  FVector2D CurrentSelectionPoint = FVector2D::ZeroVector;

          if (InitialPoint.Y < CurrentPoint.Y && InitialPoint.X < CurrentPoint.X) {
             LineCenterPointA.X = InitialPoint.X;
             LineCenterPointB.Y = CurrentPoint.Y;
             LineCenterPointA.Y = InitialPoint.Y + (LengthLineA / 2);
             LineCenterPointB.X = CurrentPoint.X - (LengthLineB / 2);
             InitialSelectionPoint.X = LineCenterPointB.X - ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             InitialSelectionPoint.Y = LineCenterPointA.Y - ((LengthLineA * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.X = LineCenterPointB.X + ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.Y = LineCenterPointA.Y + ((LengthLineA * RectangleScaleSelectionFactor) / 2);
          }
          else if (InitialPoint.Y < CurrentPoint.Y && InitialPoint.X > CurrentPoint.X) {
             LineCenterPointA.X = InitialPoint.X;
             LineCenterPointB.Y = CurrentPoint.Y;
             LineCenterPointA.Y = InitialPoint.Y + (LengthLineA / 2);
             LineCenterPointB.X = CurrentPoint.X + (LengthLineB / 2);
             InitialSelectionPoint.X = LineCenterPointB.X + ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             InitialSelectionPoint.Y = LineCenterPointA.Y - ((LengthLineA * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.X = LineCenterPointB.X - ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.Y = LineCenterPointA.Y + ((LengthLineA * RectangleScaleSelectionFactor) / 2);
          }
          else if (InitialPoint.Y > CurrentPoint.Y && InitialPoint.X < CurrentPoint.X) {
             LineCenterPointA.X = InitialPoint.X;
             LineCenterPointB.Y = CurrentPoint.Y;
             LineCenterPointA.Y = InitialPoint.Y - (LengthLineA / 2);
             LineCenterPointB.X = CurrentPoint.X - (LengthLineB / 2);
             InitialSelectionPoint.X = LineCenterPointB.X - ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             InitialSelectionPoint.Y = LineCenterPointA.Y - ((LengthLineA * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.X = LineCenterPointB.X + ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.Y = LineCenterPointA.Y + ((LengthLineA * RectangleScaleSelectionFactor) / 2);
          }
          else if (InitialPoint.Y > CurrentPoint.Y && InitialPoint.X > CurrentPoint.X) {
             LineCenterPointA.X = InitialPoint.X;
             LineCenterPointB.Y = CurrentPoint.Y;
             LineCenterPointA.Y = InitialPoint.Y - (LengthLineA / 2);
             LineCenterPointB.X = CurrentPoint.X + (LengthLineB / 2);
             InitialSelectionPoint.X = LineCenterPointB.X + ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             InitialSelectionPoint.Y = LineCenterPointA.Y - ((LengthLineA * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.X = LineCenterPointB.X - ((LengthLineB * RectangleScaleSelectionFactor) / 2);
             CurrentSelectionPoint.Y = LineCenterPointA.Y + ((LengthLineA * RectangleScaleSelectionFactor) / 2);
          }
          // ... (End of complex calculation)

          DrawRect(FLinearColor(0, 1, 0, .15f),
             InitialSelectionPoint.X,
             InitialSelectionPoint.Y,
             CurrentSelectionPoint.X - InitialSelectionPoint.X,
             CurrentSelectionPoint.Y - InitialSelectionPoint.Y);

          TArray <AUnitBase*> NewUnitBases;
          GetActorsInSelectionRectangle<AUnitBase>(InitialSelectionPoint, CurrentSelectionPoint, NewUnitBases, false, false);
          
          ACameraControllerBase* Controller = Cast<ACameraControllerBase>(GetOwningPlayerController());
          
          // --- START OF MODIFICATION ---

          // To handle dragging the rectangle in any direction, we must find the min and max coordinates.
          const FVector2D SelectionRectMin(FMath::Min(InitialSelectionPoint.X, CurrentSelectionPoint.X), FMath::Min(InitialSelectionPoint.Y, CurrentSelectionPoint.Y));
          const FVector2D SelectionRectMax(FMath::Max(InitialSelectionPoint.X, CurrentSelectionPoint.X), FMath::Max(InitialSelectionPoint.Y, CurrentSelectionPoint.Y));

          for (int32 i = 0; i < NewUnitBases.Num(); i++) {

             AUnitBase* Unit = NewUnitBases[i];
             const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Unit);
             
             // Filter for units that are selectable and use skeletal movement
             if(Controller && Unit && Unit->CanBeSelected && Unit->bUseSkeletalMovement && (Unit->TeamId == Controller->SelectableTeamId || Controller->SelectableTeamId == 0) && !SUnit)
             {
                // Get the unit's center location in the world
                const FVector UnitWorldLocation = Unit->GetActorLocation();

                // Project the world location to screen space
                FVector2D ScreenLocation;
                if (Controller->ProjectWorldLocationToScreen(UnitWorldLocation, ScreenLocation))
                {
                    // Now, check if the projected center point is within our selection rectangle
                    if (ScreenLocation.X >= SelectionRectMin.X && ScreenLocation.X <= SelectionRectMax.X &&
                        ScreenLocation.Y >= SelectionRectMin.Y && ScreenLocation.Y <= SelectionRectMax.Y)
                    {
                        // The center is inside the rectangle, so we can select it.
                        if (Unit->GetOwner() == nullptr) Unit->SetOwner(Controller);
                        Unit->SetSelected();
                        SelectedUnits.Emplace(Unit);
                        SelectedUnitsSet.Add(Unit);
                        SelectUnitsFromSameSquad(Unit);
                    }
                }
             }
          }

          // --- END OF MODIFICATION ---
          
          NewUnitBases.Empty();
          
          if(Controller) Controller->AbilityArrayIndex = 0;
       }
       // This call handles the non-skeletal (ISM) units correctly already.
       SelectISMUnitsInRectangle(InitialPoint, CurrentPoint);
    }
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

void AHUDBase::DetectUnit(AUnitBase* DetectingUnit, TArray<AActor*>& DetectedUnits, float Sight, float LoseSight, bool DetectFriendlyUnits, int PlayerTeamId)
{

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	
	for (int32 i = 0; i < GameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
		

		if (Unit && !DetectFriendlyUnits && Unit->TeamId != DetectingUnit->TeamId)
		{
			const float DistSq = FVector::DistSquared(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());
			
			if (DistSq <= FMath::Square(Sight) &&
				Unit->GetUnitState() != UnitData::Dead &&
				DetectingUnit->GetUnitState() != UnitData::Dead)
			{

				DetectedUnits.Emplace(Unit);
			}
		}else if (Unit && DetectFriendlyUnits && Unit->TeamId == DetectingUnit->TeamId)
		{

			const float DistSq = FVector::DistSquared(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());

			if (DistSq <= FMath::Square(Sight))
				DetectedUnits.Emplace(Unit);

		}
	}
	
}

void AHUDBase::ControllDirectionToMouse(AActor* Units, FHitResult Hit)
{
	FVector CharacterDirectionVector = Units->GetActorForwardVector();
	FVector ActorLocation = Units->GetActorLocation();

	float AngleDiff = CalcAngle(CharacterDirectionVector, Hit.Location - ActorLocation);

	FQuat QuadRotation;

	if ((AngleDiff > 2.f && AngleDiff < 179) || (AngleDiff <= -179.f && AngleDiff > -359.f)) {
		AngleDiff > 50.f || AngleDiff < -50.f ? QuadRotation = FQuat(FRotator(0.f, -20.0f, 0.f)) : QuadRotation = FQuat(FRotator(0.f, -4.0f, 0.f));
		
		Units->AddActorLocalRotation(QuadRotation, false, 0, ETeleportType::None);
	}
	else if ((AngleDiff < -2.f && AngleDiff > -179.f) || (AngleDiff >= 180.f && AngleDiff < 359.f)) {
		AngleDiff > 50.f || AngleDiff < -50.f ? QuadRotation = FQuat(FRotator(0.f, 20.0f, 0.f)) : QuadRotation = FQuat(FRotator(0.f, 4.0f, 0.f));
		Units->AddActorLocalRotation(QuadRotation, false, 0, ETeleportType::None);
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
			if (!IsValid(Unit) || !Unit->Attributes) continue;

			// Schnelle Selektionsprüfung (O(1) via synchronisiertem Set)
			const bool bIsSelected = SelectedUnitsSet.Contains(Unit);
			if (!bShowAllHealthBarsPermanent && !Unit->OpenHealthWidget && !(bShowHealthOnSelected && bIsSelected)) continue;

			// Sichtbarkeits-Check
			if (!(Unit->IsMyTeam || Unit->IsVisibleEnemy)) continue;

			// 3. Einzige Projektion für diesen Frame
			FVector2D ScreenPos;
			if (!PC->ProjectWorldLocationToScreen(Unit->GetActorLocation(), ScreenPos)) continue;
			
			// Screen-Culling
			if (ScreenPos.X < 0.f || ScreenPos.X > (float)ViewX || ScreenPos.Y < 0.f || ScreenPos.Y > (float)ViewY) continue;

			CurrentVisibleCount++;

			float FinalRadiusX = 60.f;
			float FinalRadiusY = 60.f;
			float FinalRadiusZ = 60.f;
			bool bIsFlying = false;

			// Direkter Komponenten-Zugriff
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
							if (const FMassAgentCharacteristicsFragment* Frag = EM->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity))
							{
								bIsFlying = Frag->bIsFlying;
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

			// Einstellungs-Auswahl
			FHealthBarSettings EffectiveSettings;
			if (!Unit->CanMove) EffectiveSettings = BuildingHealthBarSettings;
			else if (Unit->IsFlying) EffectiveSettings = FlyingHealthBarSettings;
			else EffectiveSettings = GroundHealthBarSettings;

			// LOD anwenden
			int32 BaseSegments = EffectiveSettings.Segments;
			if (EffectiveSettings.Style == EHealthBarStyle::SemiCircle && BaseSegments <= 0) BaseSegments = 32;
			if (BaseSegments > 0)
			{
				EffectiveSettings.Segments = FMath::Max(4, FMath::FloorToInt(BaseSegments * SegmentScale));
			}

			float MaxRadius = FMath::Max(FinalRadiusX, FinalRadiusY);

			if (EffectiveSettings.Style == EHealthBarStyle::SemiCircle)
			{
				DrawSemiCircleHealthBar(Unit, ScreenPos, MaxRadius, MaxRadius, bIsFlying, EffectiveSettings, RightV, UpV);
			}
			else if (EffectiveSettings.Style == EHealthBarStyle::Stacked)
			{
				DrawStackedHealthBar(Unit, ScreenPos, MaxRadius, EffectiveSettings, RightV);
			}
			else if (EffectiveSettings.Style == EHealthBarStyle::SideBrackets)
			{
				DrawSideBracketsHealthBar(Unit, ScreenPos, FinalRadiusZ, MaxRadius, EffectiveSettings, RightV, UpV);
			}
		}
	};

	DrawBarsForArray(FriendlyUnits);
	DrawBarsForArray(EnemyUnitBases);

	LastFrameVisibleCount = CurrentVisibleCount;
}

void AHUDBase::DrawStackedHealthBar(AUnitBase* Unit, const FVector2D& ScreenPos, float WorldRadius, const FHealthBarSettings& Settings, const FVector& RightV)
{
	UAttributeSetBase* Attr = Unit->Attributes;
	float Health = Attr->GetHealth();
	float MaxHealth = Attr->GetMaxHealth();
	float Shield = Attr->GetShield();
	float MaxShield = Attr->GetMaxShield();

	if (MaxHealth <= 0.f) return;

	float HealthPct = FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Shield / MaxShield, 0.f, 1.f) : 0.f;

	APlayerController* PC = GetOwningPlayerController();
	float ProjectedSize = 60.f;
	if (PC)
	{
		FVector2D Edge;
		FVector Loc = Unit->GetActorLocation();
		// Einzige zusätzliche Projektion für die Breite
		if (PC->ProjectWorldLocationToScreen(Loc + RightV * WorldRadius, Edge))
		{
			ProjectedSize = (Edge - ScreenPos).Size() * 2.0f;
		}
	}

	float Thickness = Settings.Thickness * Settings.Scale;
	auto DrawSegmentedBar = [&](FVector2D InPos, float Pct, FColor Color)
	{
		if (Settings.Segments > 0)
		{
			float TotalSize = ProjectedSize;
			float SegSize = TotalSize / Settings.Segments;
			int32 FilledSegs = FMath::CeilToInt(Settings.Segments * Pct);

			for (int32 i = 0; i < Settings.Segments; i++)
			{
				bool bFilled = (i < FilledSegs);
				FColor SegColor = bFilled ? Color : Settings.BackgroundColor;
				FVector2D SegPos = InPos + FVector2D(i * SegSize, 0);
				FVector2D SegDim = FVector2D(SegSize - Settings.SegmentSpace, Thickness);

				FCanvasTileItem SegItem(SegPos, SegDim, FLinearColor(SegColor));
				SegItem.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(SegItem);
			}
		}
		else
		{
			FVector2D BarDim = FVector2D(ProjectedSize, Thickness);
			FVector2D FillDim = FVector2D(ProjectedSize * Pct, Thickness);

			// Hintergrund
			FCanvasTileItem BackgroundItem(InPos, BarDim, FLinearColor(Settings.BackgroundColor));
			BackgroundItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(BackgroundItem);
			// Fill
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
		Pos.Y += Thickness + Settings.SegmentSpace;
	}
	DrawSegmentedBar(Pos, HealthPct, Settings.HealthColor);
}

void AHUDBase::DrawSemiCircleHealthBar(AUnitBase* Unit, const FVector2D& ScreenPos, float RadiusX, float RadiusY, bool bIsFlying, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !Canvas) return;

	UAttributeSetBase* Attr = Unit->Attributes;
	float HealthPct = FMath::Clamp(Attr->GetHealth() / Attr->GetMaxHealth(), 0.f, 1.f);
	float MaxShield = Attr->GetMaxShield();
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Attr->GetShield() / MaxShield, 0.f, 1.f) : 0.f;

	float BaseRadius = RadiusX * Settings.RadiusMultiplier;
	float Thickness = Settings.Thickness * Settings.Scale;
	
	FVector Loc = Unit->GetActorLocation();
	FVector ZOff(0, 0, 5.f);
	if (bIsFlying) ZOff.Z = -5.f; 

	FVector2D Right2D, Up2D;
	if (PC->ProjectWorldLocationToScreen(Loc + ZOff + RightV * BaseRadius, Right2D) &&
		PC->ProjectWorldLocationToScreen(Loc + ZOff + UpV * BaseRadius, Up2D))
	{
		auto DrawArc2D = [&](float InRadius, float Pct, FColor Color)
		{
			const float Base = FMath::Max(1.f, BaseRadius);
			FVector2D EX = (Right2D - ScreenPos) * (InRadius / Base);
			FVector2D EY = (Up2D - ScreenPos) * (InRadius / Base);

			int32 Segments = Settings.Segments > 0 ? Settings.Segments : 32;
			const float TotalAngle = PI;
			const float SegAngle = TotalAngle / (float)Segments;
			float ScaledGapSpace = Settings.SegmentSpace * Settings.Scale * 2.0f;
			float GapAngle = (InRadius > 0.f) ? (ScaledGapSpace / InRadius) : 0.f;
			GapAngle = FMath::Clamp(GapAngle, 0.035f, SegAngle * 0.4f);
			const float DrawAngle = SegAngle - GapAngle;

			const float StartAngleRad = FMath::DegreesToRadians(Settings.RotationOffset);

			for (int32 s = 0; s < Segments; ++s)
			{
				const bool bFilled = (s < FMath::CeilToInt(Segments * Pct));
				const FColor SegColor = bFilled ? Color : Settings.BackgroundColor;
				const float A0 = StartAngleRad + s * SegAngle;
				const float A1 = A0 + DrawAngle;
				const FVector2D P0 = ScreenPos + EX * FMath::Cos(A0) + EY * FMath::Sin(A0);
				const FVector2D P1 = ScreenPos + EX * FMath::Cos(A1) + EY * FMath::Sin(A1);

				FCanvasLineItem LineItem(P0, P1);
				LineItem.LineThickness = Thickness;
				LineItem.SetColor(FLinearColor(SegColor));
				Canvas->DrawItem(LineItem);
			}
		};

		DrawArc2D(BaseRadius, HealthPct, Settings.HealthColor);
		if (MaxShield > 0.f)
		{
			DrawArc2D(BaseRadius + Thickness + 2.f, ShieldPct, Settings.ShieldColor);
		}
	}
}

void AHUDBase::DrawSideBracketsHealthBar(AUnitBase* Unit, const FVector2D& ScreenPos, float WorldRadius, float WorldWidthRadius, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV)
{
	UAttributeSetBase* Attr = Unit->Attributes;
	float HealthPct = FMath::Clamp(Attr->GetHealth() / Attr->GetMaxHealth(), 0.f, 1.f);
	float MaxShield = Attr->GetMaxShield();
	float ShieldPct = (MaxShield > 0.f) ? FMath::Clamp(Attr->GetShield() / MaxShield, 0.f, 1.f) : 0.f;

	APlayerController* PC = GetOwningPlayerController();
	float WorldWidth = 40.f;
	float ProjectedHeight = 60.f;

	if (PC)
	{
		FVector2D EdgeWidth, EdgeHeight;
		FVector Loc = Unit->GetActorLocation();
		
		if (PC->ProjectWorldLocationToScreen(Loc + RightV * WorldWidthRadius, EdgeWidth))
		{
			WorldWidth = (EdgeWidth - ScreenPos).Size();
		}

		if (PC->ProjectWorldLocationToScreen(Loc + UpV * WorldRadius, EdgeHeight))
		{
			ProjectedHeight = (EdgeHeight - ScreenPos).Size() * 2.0f;
		}
	}

	float BracketHeight = ProjectedHeight;
	float OffsetX = WorldWidth + 5.f;
	float Thickness = Settings.Thickness * Settings.Scale;

	auto DrawSegmentedBracket = [&](FVector2D Top, float Pct, FColor Color)
	{
		if (Settings.Segments > 0)
		{
			float SegHeight = BracketHeight / Settings.Segments;
			int32 FilledSegs = FMath::CeilToInt(Settings.Segments * Pct);
			for (int32 i = 0; i < Settings.Segments; i++)
			{
				bool bFilled = (i < FilledSegs);
				FColor SegColor = bFilled ? Color : Settings.BackgroundColor;
				
				FVector2D SegPos = Top + FVector2D(0, BracketHeight - (i + 1) * SegHeight);
				FVector2D SegDim = FVector2D(Thickness, SegHeight - Settings.SegmentSpace);
				
				FCanvasTileItem SegItem(SegPos, SegDim, FLinearColor(SegColor));
				SegItem.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(SegItem);
			}
		}
		else
		{
			// Hintergrund
			FCanvasTileItem BackgroundItem(Top, FVector2D(Thickness, BracketHeight), FLinearColor(Settings.BackgroundColor));
			BackgroundItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(BackgroundItem);

			// Fill von unten
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
	}
}


