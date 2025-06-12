#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "UnitSignalingProcessor.generated.h"

// Forward declarations
class UMassUnitSpawnerSubsystem;
class AUnitBase;

UCLASS()
class RTSUNITTEMPLATE_API UUnitSignalingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitSignalingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void BeginDestroy() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.5f;
	
private:
	float TimeSinceLastRun = 0.0f;
    
	
	// This function will be bound to the "end of phase" delegate.
	void CreatePendingEntities(const float DeltaTime);

	// This array will hold the actors gathered during the Execute phase.
	TArray<TObjectPtr<AUnitBase>> ActorsToCreateThisFrame;

	// This handle manages our subscription to the OnProcessingPhaseFinished delegate.
	FDelegateHandle PhaseFinishedDelegateHandle;

	UPROPERTY(Transient)
	TObjectPtr<UMassUnitSpawnerSubsystem> SpawnerSubsystem;
};