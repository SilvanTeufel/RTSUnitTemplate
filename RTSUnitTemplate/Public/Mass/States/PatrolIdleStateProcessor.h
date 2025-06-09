// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "PatrolIdleStateProcessor.generated.h"

// Forward Decls...
struct FMassExecutionContext;
struct FMassStatePatrolIdleTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassPatrolFragment;
struct FMassVelocityFragment;
struct FMassStatePatrolRandomTag;
struct FMassStateChaseTag;


UCLASS()
class RTSUNITTEMPLATE_API UPatrolIdleStateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UPatrolIdleStateProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.5f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};