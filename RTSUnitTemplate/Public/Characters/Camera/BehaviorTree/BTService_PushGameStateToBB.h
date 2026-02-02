#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_PushGameStateToBB.generated.h"

class UBlackboardComponent;

/**
 * UBTService_PushGameStateToBB
 *
 * Clean BT-native way to feed Blackboard from current game state every tick/interval.
 * - Gathers FGameStateData via ARLAgent::GatherGameState using team id from AExtendedControllerBase.
 * - Writes keys used by rule-based tasks/services.
 * - Add this service near the root of your Behavior Tree.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API UBTService_PushGameStateToBB : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_PushGameStateToBB();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Single push used by both BecomeRelevant and Tick
	void PushOnce(UBehaviorTreeComponent& OwnerComp);

public:
	// Blackboard keys (must exist with matching types in the BT's Blackboard asset)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MyUnitCountKey = TEXT("MyUnitCount");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName EnemyUnitCountKey = TEXT("EnemyUnitCount");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MyTotalHealthKey = TEXT("MyTotalHealth");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName EnemyTotalHealthKey = TEXT("EnemyTotalHealth");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName PrimaryResourceKey = TEXT("PrimaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName SecondaryResourceKey = TEXT("SecondaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TertiaryResourceKey = TEXT("TertiaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName RareResourceKey = TEXT("RareResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName EpicResourceKey = TEXT("EpicResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName LegendaryResourceKey = TEXT("LegendaryResource");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxPrimaryResourceKey = TEXT("MaxPrimaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxSecondaryResourceKey = TEXT("MaxSecondaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxTertiaryResourceKey = TEXT("MaxTertiaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxRareResourceKey = TEXT("MaxRareResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxEpicResourceKey = TEXT("MaxEpicResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MaxLegendaryResourceKey = TEXT("MaxLegendaryResource");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName AgentPositionKey = TEXT("AgentPosition");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName AverageFriendlyPositionKey = TEXT("AverageFriendlyPosition");
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName AverageEnemyPositionKey = TEXT("AverageEnemyPosition");

	// Also push the agent pawn into the BB so tasks can resolve it even when the AIController has no pawn
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName AgentPawnKey = TEXT("AgentPawn");

	// Optional: get TeamId from Blackboard instead of controller (set to name of an Int key). If None, other methods are used.
	UPROPERTY(VisibleAnywhere, Category = "Blackboard")
	FName TeamIdBBKey = TEXT("TeamId");

	// Force a specific TeamId (>=0) and bypass controller lookup. Leave at -1 to auto-resolve.
	UPROPERTY(VisibleAnywhere, Category = "Config")
	int32 ForcedTeamId = -1;

	// Allow console override via cvar r.RTSBT.ForcedTeamId when true
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAllowConsoleForcedTeamId = true;

	// Allow falling back to any AExtendedControllerBase found in the world if RLAgent has no controller
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAllowWorldFallbackController = true;

	// Rate limit for debug prints (seconds)
	UPROPERTY(EditAnywhere, Category = "Debug")
	float DebugPrintInterval = 1.0f;

private:
	double LastDebugPrintTime = -1000.0;
	int32 TickCounter = 0;
};
