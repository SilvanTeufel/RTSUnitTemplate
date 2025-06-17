// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MainStateProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UMainStateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMainStateProcessor();
	
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float UpdateCircleInterval = 0.02f;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = RTSUnitTemplate)
	bool CircleUpdated = false;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRunA = 0.0f;
	float TimeSinceLastRunB = 0.0f;
	void HandleUpdateSelectionCircle();
	
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};
