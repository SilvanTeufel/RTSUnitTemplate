// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnitControllerBase.h"
#include "WorkerUnitControllerBase.h"
#include "BuildingControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ABuildingControllerBase : public AWorkerUnitControllerBase
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void BuildingControlStateMachine(float DeltaSeconds);
	
};




