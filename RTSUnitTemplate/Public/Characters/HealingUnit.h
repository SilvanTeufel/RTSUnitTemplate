// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/UnitBase.h"
#include "HealingUnit.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AHealingUnit : public AUnitBase
{
	GENERATED_BODY()

public:
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitsToHeal", Keywords = "RTSUnitTemplate UnitsToHeal"), Category = RTSUnitTemplate)
	//AUnitBase* UnitToHeal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealActorSpawnOffset", Keywords = "RTSUnitTemplate HealActorSpawnOffset"), Category = RTSUnitTemplate)
	FVector HealActorSpawnOffset = FVector(0.f,0.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealActorScale", Keywords = "RTSUnitTemplate HealActorScale"), Category = RTSUnitTemplate)
	FVector HealActorScale = FVector(3.f,3.f,3.f);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpawnHealActor", Keywords = "RTSUnitTemplate SpawnHealActor"), Category = RTSUnitTemplate)
	void SpawnHealActor(AActor* Target);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextUnitToChaseHeal", Keywords = "RTSUnitTemplate SetNextUnitToChaseHeal"), Category = RTSUnitTemplate)
	bool SetNextUnitToChaseHeal();
	//UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextUnitToHeal", Keywords = "RTSUnitTemplate SetNextUnitToHeal"), Category = RTSUnitTemplate)
	//bool SetNextUnitToHeal();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealingActorBaseClass", Keywords = "TopDownRTSTemplate HealingActorBaseClass"), Category = RTSUnitTemplate)
	TSubclassOf<class AHealingActor> HealingActorBaseClass;

	float HealTime = 0.5f;
	
};
