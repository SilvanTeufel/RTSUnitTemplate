// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


// HUDBase.h (Corresponding Header)
#include "Hud/HUDBase.h"

// Engine Headers
#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Net/UnrealNetwork.h"

// Project-Specific Headers
#include "Characters/Unit/HealingUnit.h"
#include "Elements/Actor/ActorElementData.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/RTSGameModeBase.h"


void AHUDBase::DrawHUD()
{
	
	if (bSelectFriendly) {
	
		DeselectAllUnits();
		
		SelectedUnits.Empty();
		
		CurrentPoint = GetMousePos2D();

		if (abs(InitialPoint.X - CurrentPoint.X) >= 2) {

			DrawRect(FLinearColor(0, 0, 1, .15f),
				InitialPoint.X,
				InitialPoint.Y,
				CurrentPoint.X - InitialPoint.X,
				CurrentPoint.Y - InitialPoint.Y);

			const float LengthLineA = abs(InitialPoint.Y - CurrentPoint.Y);
			const float LengthLineB = abs(InitialPoint.X - CurrentPoint.X);
			FVector2D LineCenterPointA;
			FVector2D LineCenterPointB;
			FVector2D InitialSelectionPoint;
			FVector2D CurrentSelectionPoint;

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


			DrawRect(FLinearColor(0, 1, 0, .15f),
				InitialSelectionPoint.X,
				InitialSelectionPoint.Y,
				CurrentSelectionPoint.X - InitialSelectionPoint.X,
				CurrentSelectionPoint.Y - InitialSelectionPoint.Y);

			TArray <AUnitBase*> NewUnitBases;

			GetActorsInSelectionRectangle<AUnitBase>(InitialSelectionPoint, CurrentSelectionPoint, NewUnitBases, false, false);

			ACameraControllerBase* Controller = Cast<ACameraControllerBase>(GetOwningPlayerController());
			
			for (int32 i = 0; i < NewUnitBases.Num(); i++) {


				const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(NewUnitBases[i]);
			
				if(Controller && (NewUnitBases[i]->TeamId == Controller->SelectableTeamId || Controller->SelectableTeamId == 0) && !SUnit) // && IsActorInsideRec(IPoint, CPoint, ALocation) // && IsActorInsideRec(IPoint, CPoint, ALocation)
				{
					if (NewUnitBases[i]->GetOwner() == nullptr) NewUnitBases[i]->SetOwner(Controller);
					NewUnitBases[i]->SetSelected();
					SelectedUnits.Emplace(NewUnitBases[i]);
					SelectUnitsFromSameSquad(NewUnitBases[i]);
				}
			}
			
			NewUnitBases.Empty();
			
			if(Controller) Controller->AbilityArrayIndex = 0;
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
	if(!bSelectFullSquad) return;

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(GameMode)
	for (int32 i = 0; i < GameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
		if(Unit && Unit->SquadId == SelectedUnit->SquadId && Unit->SquadId != 0)
		{
			Unit->SetSelected();
			SelectedUnits.Emplace(Unit);
		}
	}
}

void AHUDBase::AddUnitsToArray()
{
	
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(GameMode)
	for(int i = 0; i < GameMode->AllUnits.Num(); i++)
	{

		AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
		
		if(Unit->TeamId)
			FriendlyUnits.Add(Unit);
		if(!Unit->TeamId)
			EnemyUnitBases.Add(Unit);
	}
	
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
			const float Distance = sqrt((ActorLocation.X-Units[i]->RunLocation.X)*(ActorLocation.X-Units[i]->RunLocation.X)+(ActorLocation.Y-Units[i]->RunLocation.Y)*(ActorLocation.Y-Units[i]->RunLocation.Y));//FVector::Distance(ActorLocation, Units[i]->RunLocation);

			if (Distance <= Units[i]->StopRunTolerance) { 
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
					const float MinSpeakDistance = SpeakUnits[i]->MinSpeakDistance;
					const float Distance = sqrt((UnitLocation.X-SpeakingUnitLocation.X)*(UnitLocation.X-SpeakingUnitLocation.X)+(UnitLocation.Y-SpeakingUnitLocation.Y)*(UnitLocation.Y-SpeakingUnitLocation.Y));
					
					if(Distance < MinSpeakDistance && SpeakUnits[i]->SpeechBubble && SpeakUnits[i]->SpeechBubble->WidgetIndex != 2)
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

			const float Distance = sqrt((ActorLocation.X-Units[i]->RunLocation.X)*(ActorLocation.X-Units[i]->RunLocation.X)+(ActorLocation.Y-Units[i]->RunLocation.Y)*(ActorLocation.Y-Units[i]->RunLocation.Y));//FVector::Distance(ActorLocation, Units[i]->RunLocation);
			//float DistanceZ =  abs(ActorLocation.Z - Units[i]->RunLocation.Z);
			//UE_LOG(LogTemp, Warning, TEXT("Distance HUD: %f"), Distance);
			if (Distance <= Units[i]->StopRunTolerance) { // || DistanceY <= Units[i]->StopRunToleranceY 
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

void AHUDBase::SetUnitSelected(AUnitBase* Unit)
{
	
		
	
	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		SelectedUnits[i]->SetDeselected();
	}

	if(SelectedUnits.Num())
	SelectedUnits.Empty();
	
	SelectedUnits.Add(Unit);

	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		SelectedUnits[i]->SetSelected();
	}

	SelectUnitsFromSameSquad(Unit);
}

void AHUDBase::DeselectAllUnits()
{
	if(CharacterIsUnSelectable)
		for (int32 i = 0; i < SelectedUnits.Num(); i++) {
			if(SelectedUnits[i])
				SelectedUnits[i]->SetDeselected();
		}

	if(CharacterIsUnSelectable)
		SelectedUnits.Empty();
}

void AHUDBase::DetectUnit(AUnitBase* DetectingUnit, TArray<AActor*>& DetectedUnits, float Sight, float LoseSight, bool DetectFriendlyUnits, int PlayerTeamId)
{

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	
	for (int32 i = 0; i < GameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
		

		if (Unit && !DetectFriendlyUnits && Unit->TeamId != DetectingUnit->TeamId)
		{
			float Distance = FVector::Dist(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());
			
			if (Distance <= Sight &&
				Unit->GetUnitState() != UnitData::Dead &&
				DetectingUnit->GetUnitState() != UnitData::Dead)
			{

				DetectedUnits.Emplace(Unit);
			}
		}else if (Unit && DetectFriendlyUnits && Unit->TeamId == DetectingUnit->TeamId)
		{

			float Distance = FVector::Dist(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());

			if (Distance <= Sight)
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


