// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "Core/RTSUnitUtils.h"
#include "MassEntityQuery.h"
#include "RunStateProcessor.generated.h"

struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API URunStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

	URunStateProcessor();
public:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	void SwitchToIdleState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor);
	void SwitchToChaseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);
	void SwitchToPauseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bShowLogs = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float VelocityDistanceCheck = 150.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float VelocityToIdle = 50.f;

	/**
	 * Sekunden ohne messbaren Fortschritt, nach denen ein Laufbefehl aufgegeben wird.
	 * Ohne diesen Waechter haengt eine Einheit, deren Ziel nicht erreichbar ist, dauerhaft im
	 * Laufzustand fest. 0 schaltet den Waechter ab.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float RunStallTimeout = 6.f;

	/** Strecke, ab der ein Tick als Fortschritt zaehlt und der Waechter zurueckgesetzt wird. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float RunStallProgressDistance = 25.f;
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;
	
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};
