// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnitControllerBase.h"
#include "WorkerUnitControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AWorkerUnitControllerBase : public AUnitControllerBase
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void WorkingUnitControlStateMachine(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void EvasionWorker(AUnitBase* UnitBase, FVector CollisionLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GoToResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GoToBase(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GoToBuild(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetGroundLocation(FVector ALocation, AUnitBase* UnitBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Build(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnWorkResource(EResourceType ResourceType, FVector Location, TSubclassOf<class AWorkResource> WRClass, AUnitBase* ActorToLockOn);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* WorkResource);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DetachWorkResource(AWorkResource* WorkResource);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location,
								AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ResetPathfindingTime = 2.f;
};




