// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SpawnEnergyBar.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API USpawnEnergyBar : public UUserWidget
{
	GENERATED_BODY()


public:
	void SetOwnerActor(AUnitSpawnPlatform* Platform) {
		OwnerPlatform = Platform;
		//if(!HideWidget)
		//{
			GetWorld()->GetTimerManager().SetTimer(TickTimerHandle, this, &USpawnEnergyBar::TimerTick, UpdateInterval, true);
			//SetVisibility(ESlateVisibility::Collapsed);
		//}
	}
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitSpawnPlatform* GetOwnerActor() {
		return OwnerPlatform;
	}

	FTimerHandle TickTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateInterval = 0.05;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TimerTick();
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate) // EditAnywhere, BlueprintReadWrite, 
	AUnitSpawnPlatform* OwnerPlatform;
	
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* EnergyBar;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* CurrentEnergyLabel;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* MaxEnergyLabel;

};
