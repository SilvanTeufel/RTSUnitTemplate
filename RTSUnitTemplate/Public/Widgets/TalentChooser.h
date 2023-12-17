// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Talents.h"
#include "Characters/Unit/TalentedUnit.h"
#include "TalentChooser.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UTalentChooser : public UUserWidget
{
	GENERATED_BODY()
    
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
	UPROPERTY()
	TWeakObjectPtr<ATalentedUnit> OwnerUnitBase;

	// Dynamic arrays for UI elements
	UPROPERTY(meta = (BindWidget))
	TArray<class UProgressBar*> ClassProgressBars;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> ClassButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UTextBlock*> ClassNames;

	// Functions to dynamically create UI elements
	void CreateClassUIElements();

	// Handlers for button clicks
	// Handlers for button clicks
	//UFUNCTION()
	//void OnClassButtonClicked();

	//void ButtonClicked(int32 ButtonIndex);

	UPROPERTY(meta = (BindWidget))
	class UTextBlock*  AvailableTalents;

	UPROPERTY(meta = (BindWidget))
	class UButton*  ResetButton;

	UFUNCTION()
	void ResetClass();

	UFUNCTION()
	void HandleTalentButtonClicked(int32 ButtonIndex);
	
	//UFUNCTION()
	//void SetupButtons();

public:
	virtual void NativeConstruct() override;

	void SetOwnerActor(ATalentedUnit* Unit) {
		OwnerUnitBase = Unit;
	}

	// Additional helper functions as needed
	// ...
};
