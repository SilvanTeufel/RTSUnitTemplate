#include "Characters/Camera/BehaviorTree/BTT_ChooseAction_RuleBased.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData

UBTT_ChooseAction_RuleBased::UBTT_ChooseAction_RuleBased()
{
	NodeName = TEXT("Choose Action (Rule-Based)");
	BlackboardKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ChooseAction_RuleBased, BlackboardKey));
}

EBTNodeResult::Type UBTT_ChooseAction_RuleBased::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Missing BlackboardComponent. Ensure your Behavior Tree has a valid Blackboard asset and UseBlackboard was called by an AAIController."));
        return EBTNodeResult::Failed;
    }

    // Prefer AAIController if available, but don't require it. Fallback to AvatarActor (Pawn).
    AAIController* Controller = OwnerComp.GetAIOwner();
    APawn* Pawn = nullptr;
    if (Controller)
    {
        Pawn = Controller->GetPawn();
        if (!Pawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: AAIController present but has no Pawn. Was the pawn possessed?"));
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from AAIController: %s"), *Pawn->GetName());
        }
    }

    if (!Pawn)
    {
        // Fallback: inspect the owner of the behavior tree component
        AActor* OwnerActor = OwnerComp.GetOwner();
        if (OwnerActor)
        {
            // If Owner is any Controller (AI or Player), ask it for its Pawn
            if (AController* AnyController = Cast<AController>(OwnerActor))
            {
                Pawn = AnyController->GetPawn();
                if (Pawn)
                {
                    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from Controller owner (%s): %s"), *OwnerActor->GetClass()->GetName(), *Pawn->GetName());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Controller owner (%s) has no Pawn (not possessed?)."), *OwnerActor->GetClass()->GetName());
                }
            }
            else
            {
                // In some manual setups the owner itself might be a Pawn
                Pawn = Cast<APawn>(OwnerActor);
                if (Pawn)
                {
                    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Owner actor cast to Pawn: %s"), *Pawn->GetName());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: BehaviorTreeComponent has no Owner actor. Cannot resolve Pawn."));
        }
    }

    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Failed to resolve Pawn. Aborting task."));
        return EBTNodeResult::Failed;
    }

    URTSRuleBasedDeciderComponent* Decider = Pawn->FindComponentByClass<URTSRuleBasedDeciderComponent>();
    if (!Decider)
    {
        UE_LOG(LogTemp, Error, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no URTSRuleBasedDeciderComponent."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    FGameStateData GS;
    GS.MyUnitCount = BB->GetValueAsInt(MyUnitCountKey);
    GS.EnemyUnitCount = BB->GetValueAsInt(EnemyUnitCountKey);
    GS.PrimaryResource = BB->GetValueAsFloat(PrimaryResourceKey);
    GS.SecondaryResource = BB->GetValueAsFloat(SecondaryResourceKey);
    GS.TertiaryResource = BB->GetValueAsFloat(TertiaryResourceKey);
    GS.AgentPosition = BB->GetValueAsVector(AgentPositionKey);
    GS.AverageEnemyPosition = BB->GetValueAsVector(AverageEnemyPositionKey);

    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: GS Snapshot -> MyUnits=%d, EnemyUnits=%d, Prim=%.2f, Sec=%.2f, Ter=%.2f, AgentPos=(%.1f,%.1f,%.1f), AvgEnemy=(%.1f,%.1f,%.1f)"),
        GS.MyUnitCount, GS.EnemyUnitCount, GS.PrimaryResource, GS.SecondaryResource, GS.TertiaryResource,
        GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
        GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);

    const FString Json = Decider->ChooseJsonActionRuleBased(GS);
    if (Json.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Decider returned empty JSON. Check rule thresholds and component configuration on %s."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
    return EBTNodeResult::Succeeded;
}
