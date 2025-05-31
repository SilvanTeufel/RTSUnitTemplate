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
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};