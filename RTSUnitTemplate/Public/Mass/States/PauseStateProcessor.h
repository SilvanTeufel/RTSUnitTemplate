#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "PauseStateProcessor.generated.h"

struct FMassExecutionContext;
struct FMassStatePauseTag; // Tag für diesen Zustand
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassVelocityFragment;
struct FMassStateAttackTag; // Zielzustand
struct FMassStateChaseTag; // Zielzustand

UCLASS()
class RTSUNITTEMPLATE_API UPauseStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UPauseStateProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};