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
		GetWorld()->GetTimerManager().SetTimer(TickTimerHandle, this, &UUnitTimerWidget::TimerTick, UpdateInterval, true);
	}


protected:
	//void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	TWeakObjectPtr<AUnitBase> OwnerCharacter;

	UPROPERTY(meta = (BindWidget))
		class UProgressBar* TimerBar;

	bool IsVisible = false;
	
public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool SetVisible = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor CastingColor = FLinearColor::Red; // Replace with your desired color

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor PauseColor = FLinearColor::Yellow;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor TransportColor = FLinearColor::Blue;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor BuildColor = FLinearColor::Black;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor ExtractionColor = FLinearColor::Black;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool DisableAutoAttack = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool DisableBuild = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateInterval = 0.15;
	
	FTimerHandle TickTimerHandle;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TimerTick();
};
