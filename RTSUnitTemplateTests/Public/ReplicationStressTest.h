#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "ReplicationStressTest.generated.h"

/**
 * Functional Test for RTS Replication Pipeline Stress Testing.
 * Tests initial massive replication and burst spawning.
 */
UCLASS()
class RTSUNITTEMPLATETESTS_API AReplicationStressTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AReplicationStressTest();

	UPROPERTY(EditAnywhere, Replicated, Category = "RTS Test")
	int32 StaticLoadUnitCount = 500;

	UPROPERTY(EditAnywhere, Replicated, Category = "RTS Test")
	int32 BurstLoadUnitCount = 200;

	UPROPERTY(EditAnywhere, Replicated, Category = "RTS Test")
	float BurstDelay = 5.0f;

	UPROPERTY(EditAnywhere, Replicated, Category = "RTS Test")
	float TestTimeout = 60.0f;

	/** WÄHLE HIER DEIN EINHEITEN-BLUEPRINT AUS (z.B. BP_Unit) */
	UPROPERTY(EditAnywhere, Category = "RTS Test")
	TSubclassOf<class AUnitBase> UnitClassToSpawn;

	/** Startet den Test automatisch beim Drücken von Play im Editor */
	UPROPERTY(EditAnywhere, Category = "RTS Test")
	bool bAutoStartInPIE = false;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void PrepareTest() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartTest() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void SpawnUnits(int32 Count);
	void RunDetailedClientCheck();

	UPROPERTY(Replicated)
	bool bBurstSpawned = false;
	float TimeSinceStart = 0.0f;
	float LastMonitorTime = 0.0f;
	
	enum class ETestState : uint8
	{
		WaitingForStaticLoad,
		WaitingForBurstLoad,
		ValidatingSelection,
		Finished
	};

	ETestState CurrentState = ETestState::WaitingForStaticLoad;
};