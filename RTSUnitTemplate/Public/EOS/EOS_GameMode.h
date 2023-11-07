// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EOS_GameMode.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AEOS_GameMode : public AGameModeBase
{
	GENERATED_BODY()

//private:
	//UFUNCTION()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void Register(APlayerController* NewPlayer);

	UPROPERTY(BlueprintReadWrite, Category=RTSUnitTemplate)
	FUniqueNetIdRepl UniqueNetIdRepl;
	
};
