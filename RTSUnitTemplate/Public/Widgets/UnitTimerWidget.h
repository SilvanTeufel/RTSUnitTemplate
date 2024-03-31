// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "UnitTimerWidget.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitTimerWidget : public UUserWidget
{
	GENERATED_BODY()
	virtual void NativeConstruct() override;
	
public:
	void SetOwnerActor(AUnitBase* Unit) {
		OwnerCharacter = Unit;
	}


protected:
	void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	TWeakObjectPtr<AUnitBase> OwnerCharacter;

	UPROPERTY(meta = (BindWidget))
		class UProgressBar* TimerBar;

	bool IsVisible = false;
	bool SetVisible = false;
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSUnitTemplate")
	FLinearColor CastingColor = FLinearColor::Red; // Replace with your desired color

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSUnitTemplate")
	FLinearColor PauseColor = FLinearColor::Yellow;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSUnitTemplate")
	FLinearColor BuildColor = FLinearColor::Black;
};
