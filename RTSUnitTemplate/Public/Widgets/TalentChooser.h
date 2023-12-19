// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
//#include "Core/Talents.h"
#include "Characters/Unit/LevelUnit.h"
#include "TalentChooser.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UTalentChooser : public UUserWidget
{
	GENERATED_BODY()
    
public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateProgressBars();
	
	TArray<FGameplayAttributeData*> Attributes;

	// Function to initialize the Attributes array
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitializeAttributes();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FString> AttributeNames = {"Stamina", "Attack Power", "Willpower", "Haste", "Armor", "MagicResistance"};

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	ALevelUnit* GetOwnerActor() {
		return OwnerUnitBase;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ALevelUnit* OwnerUnitBase;

	// Dynamic arrays for UI elements
	UPROPERTY(meta = (BindWidget))
	TArray<class UProgressBar*> ClassProgressBars;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> ClassButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UTextBlock*> ClassNames;

	// Functions to dynamically create UI elements
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateExperience();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateLevelAndTalents();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CreateClassUIElements();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitializeLevelAndTalentUI();

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ExperienceProgressBar;
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock*  CurrentLevel;

	UPROPERTY(meta = (BindWidget))
	class UButton* LevelUpButton;
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock*  AvailableTalents;

	UPROPERTY(meta = (BindWidget))
	class UButton*  ResetTalentsButton;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleTalentButtonClicked(int32 ButtonIndex);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnLevelUpClicked();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnResetTalentsClicked();
	
public:
	virtual void NativeConstruct() override;

	void SetOwnerActor(ALevelUnit* Unit) {
		OwnerUnitBase = Unit;
	}

	// Additional helper functions as needed
	// ...
};
