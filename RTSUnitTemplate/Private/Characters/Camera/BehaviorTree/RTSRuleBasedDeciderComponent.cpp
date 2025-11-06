#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"

#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"

URTSRuleBasedDeciderComponent::URTSRuleBasedDeciderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default random wander choices = move_camera 1..4 (indices 17..20)
	RandomWanderIndices = {17,18,19,20};
}

static FORCEINLINE int32 ArgMax2D(const FVector2D& V)
{
	return (FMath::Abs(V.X) >= FMath::Abs(V.Y)) ? (V.X >= 0 ? 0 : 1) : (V.Y >= 0 ? 2 : 3);
}

bool URTSRuleBasedDeciderComponent::ShouldTriggerPrimary(const FGameStateData& GS) const
{
    return bEnablePrimaryRule && GS.PrimaryResource > PrimaryThreshold && GS.MyUnitCount < PrimaryMaxMyUnitCount;
}

bool URTSRuleBasedDeciderComponent::ShouldTriggerSecondary(const FGameStateData& GS) const
{
	return bEnableSecondaryRule && GS.SecondaryResource > SecondaryThreshold && GS.MyUnitCount < SecondaryMaxMyUnitCount;
}

bool URTSRuleBasedDeciderComponent::ShouldTriggerTertiary(const FGameStateData& GS) const
{
	return bEnableTertiaryRule && GS.TertiaryResource > TertiaryThreshold && GS.MyUnitCount < TertiaryMaxMyUnitCount;
}

UInferenceComponent* URTSRuleBasedDeciderComponent::GetInferenceComponent() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return nullptr;
	}
	return OwnerPawn->FindComponentByClass<UInferenceComponent>();
}

int32 URTSRuleBasedDeciderComponent::PickWanderActionIndex(const FGameStateData& GS) const
{
	/*
	// If bias is enabled and we have a meaningful direction to the enemy average position, choose among 4 directions
	if (bBiasTowardEnemy)
	{
		const FVector Delta3D = GS.AverageEnemyPosition - GS.AgentPosition;
		if (Delta3D.Size2D() > EnemyBiasMinDistance)
		{
			const FVector2D Delta(Delta3D.X, Delta3D.Y);
			// Map axis preference to indices: 0:X+, 1:X-, 2:Y+, 3:Y-
			const int32 AxisCase = ArgMax2D(Delta);
			switch (AxisCase)
			{
				case 0: return MoveRightIndex; // X+
				case 1: return MoveLeftIndex;  // X-
				case 2: return MoveUpIndex;    // Y+
				case 3: return MoveDownIndex;  // Y-
				default: break;
			}
		}
	}
	*/

	// Fallback: random from provided indices
	if (RandomWanderIndices.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, RandomWanderIndices.Num()-1);
		return RandomWanderIndices[Idx];
	}

	// Final fallback to MoveUpIndex
	return MoveUpIndex;
}

FString URTSRuleBasedDeciderComponent::BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference) const
{
	if (!Inference || Indices.Num() == 0)
	{
		return TEXT("{}");
	}

	if (Indices.Num() == 1)
	{
		return Inference->GetActionAsJSON(Indices[0]);
	}

	// Build an array of action objects
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	JsonValues.Reserve(Indices.Num());

	for (int32 Idx : Indices)
	{
		const FString ActionJson = Inference->GetActionAsJSON(Idx);
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ActionJson);
		TSharedPtr<FJsonObject> Obj;
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			JsonValues.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	FString Out;
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(JsonValues, Writer);
	return Out;
}

FString URTSRuleBasedDeciderComponent::ChooseJsonActionRuleBased(const FGameStateData& GameState)
{
	UInferenceComponent* Inference = GetInferenceComponent();
	if (!Inference)
	{
		UE_LOG(LogTemp, Error, TEXT("URTSRuleBasedDeciderComponent: No UInferenceComponent found on owning pawn. Returning {}."));
		return TEXT("{}");
	}

	// Randomize the order in which rules and wandering are evaluated each call
	enum class ERBChoice { Primary, Secondary, Tertiary, Wander };
	TArray<ERBChoice> Order;
	Order.Add(ERBChoice::Primary);
	Order.Add(ERBChoice::Secondary);
	Order.Add(ERBChoice::Tertiary);
	Order.Add(ERBChoice::Wander);

	// Fisher–Yates shuffle using FMath::RandRange
	for (int32 i = Order.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		if (i != j)
		{
			const ERBChoice Temp = Order[i];
			Order[i] = Order[j];
			Order[j] = Temp;
		}
	}

	for (ERBChoice C : Order)
	{
		switch (C)
		{
			case ERBChoice::Primary:
				if (ShouldTriggerPrimary(GameState))
				{
					UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Primary rule fired (Threshold=%.2f, MaxUnits=%d). Indices: Sel=%d, Ab=%d"), PrimaryThreshold, PrimaryMaxMyUnitCount, PrimarySelectionActionIndex, PrimaryAbilityActionIndex);
					return BuildCompositeActionJSON({ PrimarySelectionActionIndex, PrimaryAbilityActionIndex }, Inference);
				}
				break;
			case ERBChoice::Secondary:
				if (ShouldTriggerSecondary(GameState))
				{
					UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Secondary rule fired (Threshold=%.2f, MaxUnits=%d). Indices: Sel=%d, Ab=%d"), SecondaryThreshold, SecondaryMaxMyUnitCount, SecondarySelectionActionIndex, SecondaryAbilityActionIndex);
					return BuildCompositeActionJSON({ SecondarySelectionActionIndex, SecondaryAbilityActionIndex }, Inference);
				}
				break;
			case ERBChoice::Tertiary:
				if (ShouldTriggerTertiary(GameState))
				{
					UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Tertiary rule fired (Threshold=%.2f, MaxUnits=%d). Indices: Sel=%d, Ab=%d"), TertiaryThreshold, TertiaryMaxMyUnitCount, TertiarySelectionActionIndex, TertiaryAbilityActionIndex);
					return BuildCompositeActionJSON({ TertiarySelectionActionIndex, TertiaryAbilityActionIndex }, Inference);
				}
				break;
			case ERBChoice::Wander:
				if (bEnableWander)
				{
					const int32 WanderMoveIdx = PickWanderActionIndex(GameState);
					UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Wander chosen. MoveIdx=%d, TwoStep=%s (SelIdx=%d, AbIdx=%d)"), WanderMoveIdx, bWanderTwoStep?TEXT("true"):TEXT("false"), WanderSelectionActionIndex, WanderAbilityActionIndex);
					if (bWanderTwoStep)
					{
						TArray<int32> Steps;
						Steps.Add(WanderSelectionActionIndex);
						// If a specific ability is provided, use it; otherwise use the movement as the second step
						Steps.Add((WanderAbilityActionIndex != INDEX_NONE) ? WanderAbilityActionIndex : WanderMoveIdx);
						return BuildCompositeActionJSON(Steps, Inference);
					}
					return Inference->GetActionAsJSON(WanderMoveIdx);
				}
				break;
			default:
				break;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: All rules disabled and wander disabled. Returning {}."));
	// If everything disabled, no action
	return TEXT("{}");
}
