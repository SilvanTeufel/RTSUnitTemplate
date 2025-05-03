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

private:
	
	FMassEntityQuery EntityQuery;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};