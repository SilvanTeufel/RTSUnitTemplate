// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "DeathStateProcessor.generated.h"

struct FMassExecutionContext;
struct FMassStateDeadTag;
struct FMassAIStateFragment; // Für Timer
struct FMassVelocityFragment;
struct FMassActorFragment; // Für Effekte/Destroy

UCLASS()
class RTSUNITTEMPLATE_API UDeathStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UDeathStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void BeginDestroy() override;

	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	UFUNCTION()
	void HandleRemoveDeadUnit(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<class UMassEntitySubsystem> EntitySubsystem;

	FDelegateHandle RemoveDeadUnitSignalDelegateHandle;
};