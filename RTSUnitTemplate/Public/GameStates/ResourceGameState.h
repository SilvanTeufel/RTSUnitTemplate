// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ResourceGameState.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AResourceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// Use replicated properties to share data with clients
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	TArray<FResourceArray> TeamResources;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Function to handle changes to the replicated TeamResources
	UFUNCTION()
	void OnRep_TeamResources();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SetTeamResources(TArray<FResourceArray> Resources);
};