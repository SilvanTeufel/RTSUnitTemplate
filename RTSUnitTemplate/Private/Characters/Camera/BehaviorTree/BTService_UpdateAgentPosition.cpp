#include "Characters/Camera/BehaviorTree/BTService_UpdateAgentPosition.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTService_UpdateAgentPosition::UBTService_UpdateAgentPosition()
{
	bNotifyBecomeRelevant = false;
	bNotifyCeaseRelevant = false;
	bNotifyTick = true;
	Interval = 0.2f; // update 5 times per second
	RandomDeviation = 0.0f;
	NodeName = TEXT("Update Agent Position");
}

void UBTService_UpdateAgentPosition::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return;
	}

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return;
	}

	BB->SetValueAsVector(AgentPositionKey, Pawn->GetActorLocation());
}
