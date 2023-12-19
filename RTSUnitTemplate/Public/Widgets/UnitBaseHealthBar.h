// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "UnitBaseHealthBar.generated.h"

/**
 * 
 */
 
UCLASS()
class RTSUNITTEMPLATE_API UUnitBaseHealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerActor(AUnitBase* Enemy) {
		OwnerCharacter = Enemy;
	}
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* GetOwnerActor() {
		return OwnerCharacter;
	}

	
//private:	
	void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void UpdateExperience();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate) // EditAnywhere, BlueprintReadWrite, 
	AUnitBase* OwnerCharacter; //TWeakObjectPtr<AUnitBase>

	UPROPERTY(meta = (BindWidget))
		class UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
		class UTextBlock* CurrentHealthLabel;

	UPROPERTY(meta = (BindWidget))
		class UTextBlock* MaxHealthLabel;

	UPROPERTY(meta = (BindWidget))
	    class UProgressBar* ShieldBar;

	UPROPERTY(meta = (BindWidget))
	    class UTextBlock* CurrentShieldLabel;

	UPROPERTY(meta = (BindWidget))
	    class UTextBlock* MaxShieldLabel;

	UPROPERTY(meta = (BindWidget))
		class UTextBlock* CharacterLevel;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ExperienceProgressBar;

//private:
	float PreviousShieldValue = -1.0f;

};

