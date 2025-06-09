// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "PatrolRandomStateProcessor.generated.h"

// Forward Decls...
struct FMassExecutionContext;
struct FMassStatePatrolRandomTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassPatrolFragment;
struct FMassMoveTargetFragment;
struct FMassStatePatrolIdleTag;
struct FMassStateChaseTag;
class UNavigationSystemV1; // Für Random Point

UCLASS()
class RTSUNITTEMPLATE_API UPatrolRandomStateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UPatrolRandomStateProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};