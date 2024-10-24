// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraBase.h" // Include the header file for ACameraBase
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

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetupResourceWidget();
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

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Tab_Released_BP(int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Shift_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Input_Shift_Released(const FInputActionValue& InputActionValue, int32 CamState);
	/** Handles Enhanced Keyboard Inputs */

	bool IsOwnedByLocalPlayer();
	// Abilitys + Talents /////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UWidgetComponent* TalentChooser;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UWidgetComponent* AbilityChooser;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUserWidget(AUnitBase* SelectedActor);
	// Abilitys + Talents /////

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UWidgetComponent* WidgetSelector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UWidgetComponent* ResourceWidget;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSelectorWidget(int Id, AUnitBase* SelectedActor);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnAbilityInputDetected(EGASAbilityInputID InputID, AGASUnit* SelectedUnit, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ExecuteOnAbilityInputDetected(EGASAbilityInputID InputID, ACameraControllerBase* CamController);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AutoAdjustTalentChooserPosition = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool TabToggled = true;
};