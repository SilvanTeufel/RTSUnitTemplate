// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Runtime/MoviePlayer/Public/MoviePlayer.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "HUDBase.generated.h"



/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AHUDBase : public AHUD
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DrawHUD", Keywords = "RTSUnitTemplate DrawHUD"), Category = RTSUnitTemplate)
		virtual void DrawHUD(); // used in Tick();

	void Tick(float DeltaSeconds);

	void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool bSelectFullSquad = false;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SelectUnitsFromSameSquad(AUnitBase* SelectedUnit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void AddUnitsToArray();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		int AssignNewHighestIndex(AUnitBase* Unit);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		int32 HighestUnitIndex = 0;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "bSelectFriendly", Keywords = "TopDownRTSTemplate bSelectFriendly"), Category = TopDownRTSTemplate)
		bool bSelectFriendly = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "InitialPoint", Keywords = "RTSUnitTemplate InitialPoint"), Category = RTSUnitTemplate)
		FVector2D InitialPoint; // Position of mouse on screen when pressed;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CurrentPoint", Keywords = "RTSUnitTemplate CurrentPoint"), Category = RTSUnitTemplate)
		FVector2D CurrentPoint; // Position of mouse on screen while holding;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IPoint", Keywords = "RTSUnitTemplate IPoint"), Category = RTSUnitTemplate)
		FVector IPoint = FVector(0.f,0.f, 0.f); // Position of mouse on screen when pressed;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CPoint", Keywords = "RTSUnitTemplate CPoint"), Category = RTSUnitTemplate)
		FVector CPoint = FVector(0.f,0.f, 0.f);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RectangleScaleSelectionFactor", Keywords = "RTSUnitTemplate RectangleScaleSelectionFactor"), Category = RTSUnitTemplate)
		float RectangleScaleSelectionFactor = 0.9;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMousePos2D", Keywords = "RTSUnitTemplate GetMousePos2D"), Category = RTSUnitTemplate)
		FVector2D GetMousePos2D();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SelectedEnemyUnits", Keywords = "RTSUnitTemplate SelectedEnemyUnits"), Category = RTSUnitTemplate)
	    TArray <AUnitBase*> SelectedUnits;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "FriendlyUnits", Keywords = "RTSUnitTemplate FriendlyUnits"), Category = RTSUnitTemplate)
		TArray <AUnitBase*> FriendlyUnits;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AllUnits", Keywords = "RTSUnitTemplate AllUnits"), Category = RTSUnitTemplate)
		TArray <AActor*> AllUnits;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		TArray <AUnitBase*> EnemyUnitBases;

	//UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	//	TArray <AUnitBase*> AllUnitBases;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		TArray <AWorkingUnitBase*> WorkingUnits;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SpeakingUnits", Keywords = "RTSUnitTemplate SpeakingUnits"), Category = RTSUnitTemplate)
		TArray <ASpeakingUnit*> SpeakingUnits;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsSpeakingUnitClose", Keywords = "RTSUnitTemplate IsSpeakingUnitClose"), Category = TopDownRTSTemplate)
		void IsSpeakingUnitClose(TArray <AUnitBase*> Units, TArray <ASpeakingUnit*> SpeakUnits);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CharacterIsUnSelectable", Keywords = "RTSUnitTemplate CharacterIsUnSelectable"), Category = RTSUnitTemplate)
		bool CharacterIsUnSelectable = true;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveUnitsThroughWayPoints", Keywords = "RTSUnitTemplate MoveUnitsThroughWayPoints"), Category = TopDownRTSTemplate)
		void MoveUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "PatrolUnitsThroughWayPoints", Keywords = "RTSUnitTemplate PatrolUnitsThroughWayPoints"), Category = TopDownRTSTemplate)
		void PatrolUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetUnitSelected", Keywords = "RTSUnitTemplate SetUnitSelected"), Category = TopDownRTSTemplate)
		void SetUnitSelected(AUnitBase* Unit);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetUnitSelected", Keywords = "RTSUnitTemplate SetUnitSelected"), Category = TopDownRTSTemplate)
		void DeselectAllUnits();
	
	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void DetectAllUnits();
	
	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void DetectUnit(AUnitBase* DetectingUnit, TArray<AActor*>& DetectedUnits, float Sight, float LoseSight, bool DetectFriendlyUnits, int PlayerTeamId);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ControllDirectionToMouse", Keywords = "RTSUnitTemplate ControllDirectionToMouse"), Category = RTSUnitTemplate)
		void ControllDirectionToMouse(AActor* Units, FHitResult Hit);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsActorInsideRectangle", Keywords = "RTSUnitTemplate IsActorInsideRectangle"), Category = TopDownRTSTemplate)
		bool IsActorInsideRec(FVector InPoint, FVector CuPoint, FVector ALocation);

};
