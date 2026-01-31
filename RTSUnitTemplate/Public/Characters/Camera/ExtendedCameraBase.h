// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraBase.h" // Include the header file for ACameraBase
#include "Widgets/AbilityChooser.h"
#include "Widgets/MinimapWidget.h" 
#include "Widgets/ResourceWidget.h"
#include "Widgets/TaggedUnitSelector.h"
#include "Widgets/TalentChooser.h"
#include "Widgets/UnitWidgetSelector.h"
#include "Widgets/WinConditionWidget.h"
#include "Widgets/MapMenuWidget.h"
#include "Widgets/StoryWidgetBase.h"
#include "GameplayTagContainer.h"
#include "ExtendedCameraBase.generated.h"

/**
 * ExtendedCameraBase is a child class of ACameraBase
 */
UCLASS()
class RTSUNITTEMPLATE_API AExtendedCameraBase : public ACameraBase
{
	GENERATED_BODY()

public:

	AExtendedCameraBase(const FObjectInitializer& ObjectInitializer);
	
	// Override the BeginPlay function
	virtual void BeginPlay() override;

	UFUNCTION(Client, Reliable)
	void Client_UpdateWidgets(UUnitWidgetSelector* NewWidgetSelector, UTaggedUnitSelector* NewTaggedSelector, UResourceWidget* NewResourceWidget);
	
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetupResourceWidget(AExtendedControllerBase* CameraControllerBase);
	// Override the Tick function
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState);

	/** Handles Enhanced Keyboard Inputs */
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_LeftClick_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_LeftClick_Released(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_RightClick_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_G_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_A_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Alt_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Alt_Released(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Ctrl_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Ctrl_Released(const FInputActionValue& InputActionValue, int32 CamState);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Tab_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState);

	void Input_V_Pressed(const FInputActionValue& InputActionValue, int32 CamState);

	void Input_Tab_Released_BP(int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Shift_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Shift_Released(const FInputActionValue& InputActionValue, int32 CamState);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Esc_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	/** Handles Enhanced Keyboard Inputs */

	bool IsOwnedByLocalPlayer();


	/** The class of Minimap Widget to create. Assign this in the Blueprint editor for this camera pawn. */
	//UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//class UWidgetComponent* MinimapWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMinimapWidget* Minimap; 
	// Abilitys + Talents /////

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UTalentChooser* TalentChooserWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UAbilityChooser* AbilityChooserWidget;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUserWidget(AUnitBase* SelectedActor);
	// Abilitys + Talents /////

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UUnitWidgetSelector* UnitSelectorWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UTaggedUnitSelector* TaggedSelectorWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UResourceWidget* ResourceWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UWinConditionWidget* WinConditionWidget;

	FTimerHandle WinConditionDisplayTimerHandle;
	FTimerHandle InitialWinConditionDelayTimerHandle;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ShowWinConditionWidget(float Duration);

	void HideWinConditionWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitializeWinConditionDisplay();

	UFUNCTION()
	void OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition);

	UFUNCTION()
	void OnTagProgressUpdated(AWinLoseConfigActor* Config);

	UFUNCTION()
	void OnTeamIdChanged_Internal(int32 NewTeamId);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMapMenuWidget* MapMenuWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStoryWidgetBase* StoryWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TagTime = 0.5f;
	
	int32 TabMode = 1;

	float FKeyHoldTimes[6];
	bool bFKeyTagAssigned[6];
	bool bFKeyPressed[6];
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateTabModeUI();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSelectorWidget(int Id, AUnitBase* SelectedActor);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateSelectorWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool InitUnitSelectorWidgetController(ACustomControllerBase* WithPC);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool InitTaggedSelectorWidgetController(ACustomControllerBase* WithPC);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool InitAbiltiyChooserWidgetController(ACustomControllerBase* WithPC);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnAbilityInputDetected(EGASAbilityInputID InputID, AGASUnit* SelectedUnit, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ExecuteOnAbilityInputDetected(EGASAbilityInputID InputID, ACameraControllerBase* CamController);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AutoAdjustTalentChooserPosition = true;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveW(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveW(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveS(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveS(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveA(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveA(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveD(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveD(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_ZoomIn(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopZoomIn(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_ZoomOut(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopZoomOut(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_LockOnCharacter();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_OrbitAndMove();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_SpawnEffects(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_RotateLeft(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopRotateLeft(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_RotateRight(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopRotateRight(ACameraControllerBase* CameraControllerBase);
	// Add other state handling functions as needed...
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveW_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveW_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveS_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveS_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveA_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveA_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MoveD_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopMoveD_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_ZoomIn_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopZoomIn_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_ZoomOut_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopZoomOut_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_RotateLeft_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopRotateLeft_NoStrg(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_RotateRight_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_StopRotateRight_NoStrg(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_TPressed(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_PPressed(ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_OPressed(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityArrayIndex(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityUnitIndex(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_ScrollZoom(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MiddleMousePressed(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_MiddleMouseReleased(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityOne(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityTwo(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityThree(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityFour(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilityFive(ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleState_AbilitySix(ACameraControllerBase* CameraControllerBase);
};