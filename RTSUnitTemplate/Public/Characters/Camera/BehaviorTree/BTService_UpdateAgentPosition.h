#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateAgentPosition.generated.h"

/**
 * Example BT Service that updates a Vector blackboard key with the controlled pawn's world location.
 */
UCLASS()
class RTSUNITTEMPLATE_API UBTService_UpdateAgentPosition : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateAgentPosition();

	// Blackboard Vector key to write the agent/pawn location into
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName AgentPositionKey = TEXT("AgentPosition");

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
