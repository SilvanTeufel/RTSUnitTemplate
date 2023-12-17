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

	TWeakObjectPtr<AUnitBase> GetOwnerActor(AUnitBase* Enemy) {
		return OwnerCharacter;
	}

	
//private:	
	void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate) // EditAnywhere, BlueprintReadWrite, 
	TWeakObjectPtr<AUnitBase> OwnerCharacter;

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

//private:
	float PreviousShieldValue = -1.0f;

};

