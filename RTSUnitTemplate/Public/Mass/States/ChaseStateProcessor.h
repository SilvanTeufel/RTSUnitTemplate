// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "ChaseStateProcessor.generated.h"

struct FMassExecutionContext;
struct FMassStateChaseTag; // Tag für diesen Zustand
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassTransformFragment;
struct FMassCombatStatsFragment;
struct FMassMoveTargetFragment;
struct FMassStatePauseTag; // Zielzustand
struct FMassStateIdleTag; // Zielzustand

UCLASS()
class RTSUNITTEMPLATE_API UChaseStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UChaseStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};