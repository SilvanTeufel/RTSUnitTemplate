// ExtendedCameraBase.h

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

	// Override the Tick function
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState);

	/** Handles Enhanced Keyboard Inputs */
	void Input_LeftClick_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_LeftClick_Released(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_RightClick_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_G_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_A_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Ctrl_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Ctrl_Released(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Tab_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Shift_Pressed(const FInputActionValue& InputActionValue, int32 CamState);
	void Input_Shift_Released(const FInputActionValue& InputActionValue, int32 CamState);
	/** Handles Enhanced Keyboard Inputs */
	
	// Talents /////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ActionWidgetRotation", Keywords = "TopDownRTSTemplate ActionWidgetRotation"), Category = TopDownRTSTemplate)
	FRotator TalentChooserRotation = FRotator(0.f, 0.f, 0.f);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite,  Category = TopDownRTSTemplate)
	class UWidgetComponent* TalentChooser;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	FVector2D TalentChooserLocation = FVector2D(0.15f, 0.5f );

	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void SpawnTalentChooser();
	
	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void SetTalentChooserLocation();

	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
		void SetUserWidget(AUnitBase* SelectedActor);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float WidgetDistance = 50.f;
	// Talents /////

	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
	void OnAbilityInputDetected(EGASAbilityInputID InputID, AGASUnit* SelectedUnit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	bool AutoAdjustTalentChooserPosition = true;
};