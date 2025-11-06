#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"

#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Engine/DataTable.h"

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

int32 URTSRuleBasedDeciderComponent::GetMaxFriendlyTagUnitCount(const FGameStateData& GS) const
{
	int32 MaxVal = 0;
	// Alt1..Alt6
	MaxVal = FMath::Max(MaxVal, GS.Alt1TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Alt2TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Alt3TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Alt4TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Alt5TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Alt6TagFriendlyUnitCount);
	// Ctrl1..Ctrl6
	MaxVal = FMath::Max(MaxVal, GS.Ctrl1TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Ctrl2TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Ctrl3TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Ctrl4TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Ctrl5TagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.Ctrl6TagFriendlyUnitCount);
	// CtrlQ/W/E/R
	MaxVal = FMath::Max(MaxVal, GS.CtrlQTagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.CtrlWTagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.CtrlETagFriendlyUnitCount);
	MaxVal = FMath::Max(MaxVal, GS.CtrlRTagFriendlyUnitCount);
	return MaxVal;
}

FString URTSRuleBasedDeciderComponent::EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const
{
	if (!Row.bEnabled)
	{
		return TEXT("{}");
	}

	// Resources must exceed thresholds (strictly greater), using defaults in row
	if (!(GS.PrimaryResource > Row.PrimaryThreshold)) return TEXT("{}");
	if (!(GS.SecondaryResource > Row.SecondaryThreshold)) return TEXT("{}");
	if (!(GS.TertiaryResource > Row.TertiaryThreshold)) return TEXT("{}");
	if (!(GS.RareResource > Row.RareThreshold)) return TEXT("{}");
	if (!(GS.EpicResource > Row.EpicThreshold)) return TEXT("{}");
	if (!(GS.LegendaryResource > Row.LegendaryThreshold)) return TEXT("{}");

	// Caps
	if (!(GS.MyUnitCount < Row.MaxFriendlyUnitCount)) return TEXT("{}");
	// Per-tag friendly unit caps
	if (!(GS.Alt1TagFriendlyUnitCount <= Row.Alt1TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Alt2TagFriendlyUnitCount <= Row.Alt2TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Alt3TagFriendlyUnitCount <= Row.Alt3TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Alt4TagFriendlyUnitCount <= Row.Alt4TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Alt5TagFriendlyUnitCount <= Row.Alt5TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Alt6TagFriendlyUnitCount <= Row.Alt6TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl1TagFriendlyUnitCount <= Row.Ctrl1TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl2TagFriendlyUnitCount <= Row.Ctrl2TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl3TagFriendlyUnitCount <= Row.Ctrl3TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl4TagFriendlyUnitCount <= Row.Ctrl4TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl5TagFriendlyUnitCount <= Row.Ctrl5TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.Ctrl6TagFriendlyUnitCount <= Row.Ctrl6TagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.CtrlQTagFriendlyUnitCount <= Row.CtrlQTagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.CtrlWTagFriendlyUnitCount <= Row.CtrlWTagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.CtrlETagFriendlyUnitCount <= Row.CtrlETagMaxFriendlyUnitCount)) return TEXT("{}");
	if (!(GS.CtrlRTagFriendlyUnitCount <= Row.CtrlRTagMaxFriendlyUnitCount)) return TEXT("{}");

	// Build output
	return BuildCompositeActionJSON({ Row.SelectionActionIndex, Row.AbilityActionIndex }, Inference);
}

FString URTSRuleBasedDeciderComponent::EvaluateRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference) const
{
	if (!RulesDataTable)
	{
		return TEXT("{}");
	}
	TArray<FName> RowNames = RulesDataTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		return TEXT("{}");
	}
	// Randomize which row is evaluated first by picking a random start index and iterating circularly
	const int32 StartIndex = FMath::RandRange(0, RowNames.Num() - 1);
	for (int32 Offset = 0; Offset < RowNames.Num(); ++Offset)
	{
		const FName& Name = RowNames[(StartIndex + Offset) % RowNames.Num()];
		const FRTSRuleRow* Row = RulesDataTable->FindRow<FRTSRuleRow>(Name, TEXT("RTSRules"));
		if (!Row)
		{
			continue;
		}
		const FString Out = EvaluateRuleRow(*Row, GS, Inference);
		if (!Out.IsEmpty() && Out != TEXT("{}"))
		{
			UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: DataTable rule '%s' fired."), *Name.ToString());
			return Out;
		}
	}
	return TEXT("{}");
}

FString URTSRuleBasedDeciderComponent::ChooseJsonActionRuleBased(const FGameStateData& GameState)
{
	UInferenceComponent* Inference = GetInferenceComponent();
	if (!Inference)
	{
		UE_LOG(LogTemp, Error, TEXT("URTSRuleBasedDeciderComponent: No UInferenceComponent found on owning pawn. Returning {}."));
		return TEXT("{}");
	}

	// Helper lambdas for paths
	auto TryTable = [&]() -> FString
	{
		if (bUseDataTableRules && RulesDataTable)
		{
			const FString TableResult = EvaluateRulesFromDataTable(GameState, Inference);
			if (!TableResult.IsEmpty() && TableResult != TEXT("{}"))
			{
				return TableResult;
			}
		}
		return TEXT("{}");
	};

	auto TryWander = [&]() -> FString
	{
		if (bEnableWander)
		{
			const int32 WanderMoveIdx = PickWanderActionIndex(GameState);
			UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Wander path. MoveIdx=%d, TwoStep=%s (SelIdx=%d, AbIdx=%d)"), WanderMoveIdx, bWanderTwoStep?TEXT("true"):TEXT("false"), WanderSelectionActionIndex, WanderAbilityActionIndex);
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
		return TEXT("{}");
	};

	// Alternate which path is attempted first each call (static toggle survives between calls)
	static bool bTryTableFirstNext = true;
	const bool bTryTableFirst = bTryTableFirstNext;
	bTryTableFirstNext = !bTryTableFirstNext;

	FString Result;
	if (bTryTableFirst)
	{
		Result = TryTable();
		if (Result.IsEmpty() || Result == TEXT("{}"))
		{
			Result = TryWander();
		}
	}
	else
	{
		Result = TryWander();
		if (Result.IsEmpty() || Result == TEXT("{}"))
		{
			Result = TryTable();
		}
	}

	if (Result.IsEmpty() || Result == TEXT("{}"))
	{
		UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: Neither DataTable nor Wander produced an action (table=%s, wander=%s). Returning {}."),
			(bUseDataTableRules && RulesDataTable)?TEXT("enabled"):TEXT("disabled"), bEnableWander?TEXT("enabled"):TEXT("disabled"));
	}
	return Result;
}
