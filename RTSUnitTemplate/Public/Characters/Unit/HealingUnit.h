// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/UnitBase.h"
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
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartHealingEvent();

	UFUNCTION(NetMulticast, Reliable)
	void MultiCastStartHealingEvent();
	
	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void StartHealingEvent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealingActorBaseClass", Keywords = "TopDownRTSTemplate HealingActorBaseClass"), Category = RTSUnitTemplate)
	TSubclassOf<class AHealingActor> HealingActorBaseClass;

	float HealTime = 0.5f;
	
};
