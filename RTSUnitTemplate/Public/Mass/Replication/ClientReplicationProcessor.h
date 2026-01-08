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
	bool bStopMovementReplication = false;
	
	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.1f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MinErrorForCorrectionSq = FMath::Square(25.f); // 5 cm
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MaxCorrectionAccel = 5000.f; // cm/s^2 // 1000.f
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float Kp = 15.0f; // proportional gain to turn error into desired velocity/accel // 6.0f
	// If reconciliation error exceeds this distance (cm), perform a one-time full replication (snap) on that tick
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float FullReplicationDistance = 2000.f; // cm

	// Rotation reconciliation (Yaw-only by default)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bEnableRotationReconciliation = true; // enable gentle rotation correction
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MinYawErrorForCorrectionDeg = 3.0f; // deg, threshold to start correcting
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float MaxRotationCorrectionDegPerSec = 30.0f; // deg/sec, weak by default
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float KpRot = 0.5f; // proportional gain for yaw correction
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bRotationYawOnly = true; // correct yaw only by default
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
