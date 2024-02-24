// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hud/PathProviderHUD.h"
#include "Characters/Camera/CameraBase.h"

#include "Core/UnitData.h"
#include "GameFramework/PlayerController.h"
#include "Actors/EffectArea.h"
#include "EOS/EOS_PlayerController.h"
#include "ControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AControllerBase : public AEOS_PlayerController
{
	GENERATED_BODY()

public:
	AControllerBase();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	APathProviderHUD* HUDBase;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	ACameraBase* CameraBase;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* ClickedActor;

	//UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	//FVector ClickLocation = FVector(0.0f, 0.0f, 0.0f);
	
	void Tick(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftPressed", Keywords = "RTSUnitTemplate ShiftPressed"), Category = RTSUnitTemplate)
		void ShiftPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftReleased", Keywords = "RTSUnitTemplate ShiftReleased"), Category = RTSUnitTemplate)
		void ShiftReleased();

	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void SelectUnit(int Index);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LeftClickAMoveUEPF(AUnitBase* Unit, FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LeftClickAMove(AUnitBase* Unit, FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LeftClickAttack(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LeftClickSelect();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LeftClickPressed", Keywords = "RTSUnitTemplate LeftClickPressed"), Category = RTSUnitTemplate)
		void LeftClickPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LeftClickReleased", Keywords = "RTSUnitTemplate LeftClickReleased"), Category = RTSUnitTemplate)
		void LeftClickReleased();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetWidgets(int Index);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetRunLocation(AUnitBase* Unit, const FVector& DestinationLocation);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetUnitState_Replication(AUnitBase* Unit, int State);
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
		bool UseUnrealEnginePathFinding = true;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float UEPathfindingCornerOffset = 100.f;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetToggleUnitDetection(AUnitBase* Unit, bool State);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void RightClickRunShift(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void RightClickRunUEPF(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void RightClickRunDijkstraPF(AUnitBase* Unit, FVector Location, int Counter);

	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RightClickPressed", Keywords = "RTSUnitTemplate RightClickPressed"), Category = RTSUnitTemplate)
		void RightClickPressed();

	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
		void SetRunLocationUseDijkstra(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);

	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
		void SetRunLocationUseDijkstraForAI(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpacePressed", Keywords = "RTSUnitTemplate SpacePressed"), Category = RTSUnitTemplate)
		void SpacePressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpaceReleased", Keywords = "RTSUnitTemplate SpaceReleased"), Category = RTSUnitTemplate)
		void SpaceReleased();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ToggleUnitDetection(AUnitBase* Unit);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "APressed", Keywords = "RTSUnitTemplate APressed"), Category = RTSUnitTemplate)
		void TPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AReleased", Keywords = "RTSUnitTemplate AReleased"), Category = RTSUnitTemplate)
		void AReleased();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "JumpCamera", Keywords = "RTSUnitTemplate JumpCamera"), Category = RTSUnitTemplate)
		void JumpCamera();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "StrgPressed", Keywords = "RTSUnitTemplate StrgPressed"), Category = RTSUnitTemplate)
		void StrgPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "StrgReleased", Keywords = "RTSUnitTemplate StrgReleased"), Category = RTSUnitTemplate)
		void StrgReleased();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomIn", Keywords = "RTSUnitTemplate ZoomIn"), Category = RTSUnitTemplate)
		void ZoomIn();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomOut", Keywords = "RTSUnitTemplate ZoomOut"), Category = RTSUnitTemplate)
		void ZoomOut();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TSubclassOf<class AMissileRain> MissileRainClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnMissileRain(int TeamId, FVector Location);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TSubclassOf<class AEffectArea> EffectAreaClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnEffectArea(int TeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn = nullptr);
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
		bool IsShiftPressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AttackToggled", Keywords = "RTSUnitTemplate AttackToggled"), Category = RTSUnitTemplate)
		bool AttackToggled = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsStrgPressed", Keywords = "RTSUnitTemplate IsStrgPressed"), Category = RTSUnitTemplate)
		bool IsStrgPressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsSpacePressed", Keywords = "RTSUnitTemplate IsSpacePressed"), Category = RTSUnitTemplate)
		bool IsSpacePressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AltIsPressed", Keywords = "RTSUnitTemplate AltIsPressed"), Category = RTSUnitTemplate)
		bool AltIsPressed = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "LeftClickisPressed", Keywords = "RTSUnitTemplate LeftClickisPressed"), Category = RTSUnitTemplate)
		bool LeftClickIsPressed = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "LockCameraToUnit", Keywords = "RTSUnitTemplate LockCameraToUnit"), Category = RTSUnitTemplate)
		bool LockCameraToUnit = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AIsPressed", Keywords = "TopDownRTSCamLib AIsPressed"), Category = RTSUnitTemplate)
		int AIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "DIsPressed", Keywords = "TopDownRTSCamLib DIsPressed"), Category = RTSUnitTemplate)
		int DIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "WIsPressed", Keywords = "TopDownRTSCamLib WIsPressed"), Category = RTSUnitTemplate)
		int WIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "SIsPressed", Keywords = "TopDownRTSCamLib SIsPressed"), Category = RTSUnitTemplate)
		int SIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool MiddleMouseIsPressed = false;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		TArray <AUnitBase*> SelectedUnits;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		int SelectableTeamId = 0;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetControlerTeamId(int Id);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SaveLevel(const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LoadLevel(const FString& SlotName);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LevelUp();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ResetTalents();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestment(int32 InvestmentState);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SaveLevelUnit(const int32 UnitIndex, const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LoadLevelUnit(const int32 UnitIndex, const FString& SlotName);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LevelUpUnit(const int32 UnitIndex);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ResetTalentsUnit(const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestmentUnit(const int32 UnitIndex, int32 InvestmentState);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpendAbilityPoints(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ResetAbility(const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SaveAbility(const int32 UnitIndex, const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LoadAbility(const int32 UnitIndex, const FString& SlotName);

	UPROPERTY(BlueprintReadWrite, Category = TopDownRTSTemplate)
	int SelectedUnitCount = 0;


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetResource(int TeamId, EResourceType RType);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ModifyResource(EResourceType ResourceType, int32 TeamId, float Amount);
};

