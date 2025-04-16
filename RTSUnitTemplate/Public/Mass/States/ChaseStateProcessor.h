#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
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
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EntityQuery;

	// Hilfsfunktion zum Setzen des Bewegungsziels
	void UpdateMoveTarget(FMassMoveTargetFragment& MoveTarget, const FVector& TargetLocation, float Speed, UWorld* World);
	void StopMovement(FMassMoveTargetFragment& MoveTarget, UWorld* World);
};