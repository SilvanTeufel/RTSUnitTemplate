// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
class UObject;
#include "ClientReplicationProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UClientReplicationProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UClientReplicationProcessor();

	// Global toggle: when true, use full replication (directly set transform). When false, use reconciliation via steering/force.
	bool bUseFullReplication = false;
	bool bSkipReplication = false;

	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.25f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MinErrorForCorrectionSq = FMath::Square(1.f); // 5 cm
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MaxCorrectionAccel = 3000.f; // cm/s^2
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float Kp = 60.0f; // proportional gain to turn error into desired velocity/accel
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
