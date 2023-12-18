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

	UFUNCTION()
	void UpdateProgressBars();
	
	TArray<FGameplayAttributeData*> Attributes;

	// Function to initialize the Attributes array
	void InitializeAttributes();

	UPROPERTY()
	TArray<FString> AttributeNames = {"Stamina", "Attack Power", "Willpower", "Haste", "Armor", "MagicResistance"};
	
	UPROPERTY()
	TWeakObjectPtr<ALevelUnit> OwnerUnitBase;

	// Dynamic arrays for UI elements
	UPROPERTY(meta = (BindWidget))
	TArray<class UProgressBar*> ClassProgressBars;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> ClassButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UTextBlock*> ClassNames;

	// Functions to dynamically create UI elements
	UFUNCTION()
	void UpdateExperience();

	UFUNCTION()
	void UpdateLevelAndTalents();
	
	UFUNCTION()
	void CreateClassUIElements();

	UFUNCTION()
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

	UFUNCTION()
	void HandleTalentButtonClicked(int32 ButtonIndex);

	UFUNCTION()
	void OnLevelUpClicked();

	UFUNCTION()
	void OnResetTalentsClicked();
	
public:
	virtual void NativeConstruct() override;

	void SetOwnerActor(ALevelUnit* Unit) {
		OwnerUnitBase = Unit;
	}

	// Additional helper functions as needed
	// ...
};
