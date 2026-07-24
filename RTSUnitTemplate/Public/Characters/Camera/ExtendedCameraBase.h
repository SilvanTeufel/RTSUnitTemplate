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
#include "Widgets/AttributeTreeWidget.h"
#include "GameplayTagContainer.h"
#include "ExtendedCameraBase.generated.h"

class ALevelUnit;
class UEnhancedInputComponentBase;

/**
 * Designer-tunable depth-of-field / blur applied to the camera's post-process while a
 * full-screen Tab menu (or the map / Esc menu) is open. Defaults are a gentle
 * "frosted-glass" blur that keeps the scene readable. Consumed by AExtendedCameraBase::UpdateViewportBlur().
 *
 * To reproduce the legacy full-screen white-out set:
 *   Fstop = 0.1, MinFstop = 0.1, FocalDistance = 1.0, SensorWidth = 1000.0,
 *   DepthBlurAmount = 1.0, DepthBlurRadius = 100.0,
 *   FocalRegion = 0, NearTransitionRegion = 0, FarTransitionRegion = 0.
 */
USTRUCT(BlueprintType)
struct FRTSViewportBlurSettings
{
	GENERATED_BODY()

	/** Master switch. When false, UpdateViewportBlur() never applies blur and always clears the DoF overrides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur")
	bool bBlurEnabled = true;

	/** Cinematic-DoF aperture (f-number). LOWER = stronger blur. Real lenses ~f/1.4-f/22; legacy white-out used 0.1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "22.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldFstop = 2.0f;

	/** Smallest f-number the simulated diaphragm can reach. Keep <= Fstop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "22.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldMinFstop = 1.2f;

	/** World-space distance (cm) to the in-focus plane. Small = whole distant scene out of focus; legacy used 1.0 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "1.0", UIMin = "10.0", UIMax = "100000.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldFocalDistance = 10.0f;

	/** Sensor width (mm). Larger = shallower DoF (more blur). Full-frame ~36, Super-35 ~24.576; legacy used 1000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "1000.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldSensorWidth = 24.576f;

	/** Distance ("depth") blur amount: engine reads this as the depth in km at which 50% of DepthBlurRadius is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "0.000001", UIMin = "0.0", UIMax = "4.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldDepthBlurAmount = 1.0f;

	/** Max distance-blur radius (px @1920). Main "blur everything" knob. Legacy used 100 (full-frame smear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldDepthBlurRadius = 4.0f;

	/** Mobile DoF fully-in-focus region width (cm). Inert under desktop cinematic DoF; drives mobile/ES31 blur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur|Mobile DoF",
		meta = (ClampMin = "0.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldFocalRegion = 0.0f;

	/** Mobile DoF near transition region (cm). Inert under desktop cinematic DoF; drives mobile/ES31 blur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur|Mobile DoF",
		meta = (ClampMin = "0.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldNearTransitionRegion = 0.0f;

	/** Mobile DoF far transition region (cm). Inert under desktop cinematic DoF; drives mobile/ES31 blur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur|Mobile DoF",
		meta = (ClampMin = "0.0", EditCondition = "bBlurEnabled"))
	float DepthOfFieldFarTransitionRegion = 0.0f;
};

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
	bool SetupResourceWidget(AExtendedControllerBase* CameraControllerBase);
	// Override the Tick function
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	/** Binds ONLY camera-rig movement: WASD pan, Q/E rotate, X/Y + scroll zoom,
	 *  middle-mouse orbit, Space zoom-toggle, P orbit-move. A read-only pawn keeps this. */
	virtual void BindCameraInputActions(class UEnhancedInputComponentBase* EIC);

	/** Binds selection / commands / abilities / control-groups / UI toggles / modifier keys.
	 *  Override to an empty body in a spectator pawn to make the pawn read-only. */
	virtual void BindGameplayInputActions(class UEnhancedInputComponentBase* EIC);

public:

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

	// Radial attribute tree (alternative to the TalentChooser), shown on tab 4.
	// Wired from the MainHUD BP: place a UAttributeTreeWidget in your MainHUD and assign it here via
	// SetAttributeTreeWidget in EventPreConstruct (as BP_MainHUD/BP_MainHUD_2 do), like TalentChooserWidget.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UAttributeTreeWidget* AttributeTreeWidget;

	/** Server-authoritative: invest one talent point through an attribute-tree node. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_InvestAttributeTreeNode(ALevelUnit* Unit, FName NodeId);

	/** Server-authoritative: reset the unit's attribute tree (refunds talent points). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_ResetAttributeTree(ALevelUnit* Unit);

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
	bool InitializeWinConditionDisplay();

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

	/** Designer-tunable blur applied by UpdateViewportBlur() while a Tab / map / Esc menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Camera|Blur")
	FRTSViewportBlurSettings BlurSettings;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateViewportBlur(bool bEnable);

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