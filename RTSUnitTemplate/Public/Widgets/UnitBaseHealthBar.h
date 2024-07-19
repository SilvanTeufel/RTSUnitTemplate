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
		if(!HideWidget)
		{
			GetWorld()->GetTimerManager().SetTimer(TickTimerHandle, this, &UUnitBaseHealthBar::TimerTick, UpdateInterval, true);
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* GetOwnerActor() {
		return OwnerCharacter;
	}

	
//private:	
	//void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

	FTimerHandle CollapseTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HideWidget = false;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ResetCollapseTimer(float VisibleTime = 10.f);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CollapseWidget();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateInterval = 0.05;
	
	FTimerHandle TickTimerHandle;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TimerTick();
};

