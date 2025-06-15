#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "UnitPresenceSignalingProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UUnitPresenceSignalingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitPresenceSignalingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.0f;
	
private:
	float TimeSinceLastRun = 0.0f;
    
	FMassEntityQuery EntityQuery;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
};