#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"

#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Characters/Camera/RLAgent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameStates/ResourceGameState.h"
#include "EngineUtils.h"

URTSRuleBasedDeciderComponent::URTSRuleBasedDeciderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default random wander choices = move_camera 1..4 (indices 17..20)
	RandomWanderActions = { ERTSAIAction::MoveDirection1, ERTSAIAction::MoveDirection2, ERTSAIAction::MoveDirection3, ERTSAIAction::MoveDirection4 };
}

void URTSRuleBasedDeciderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bUseAttackDataTableRules && AttackRulesDataTable)
	{
		bool bAnyRowHasClasses = false;
		TArray<FName> RowNames = AttackRulesDataTable->GetRowNames();
		for (const FName& Name : RowNames)
		{
			const FRTSAttackRuleRow* Row = AttackRulesDataTable->FindRow<FRTSAttackRuleRow>(Name, TEXT("RTSAttackRules"));
			if (Row && Row->AttackPositionSourceClasses.Num() > 0)
			{
				bAnyRowHasClasses = true;
				break;
			}
		}

		if (bAnyRowHasClasses)
		{
			GetWorld()->GetTimerManager().SetTimer(AttackPositionRefreshTimerHandle, this, &URTSRuleBasedDeciderComponent::PopulateAttackPositions, AttackPositionUpdateDelay, false);
		}
	}
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
				case 0: return (int32)MoveRightAction; // X+
				case 1: return (int32)MoveLeftAction;  // X-
				case 2: return (int32)MoveUpAction;    // Y+
				case 3: return (int32)MoveDownAction;  // Y-
				default: break;
			}
		}
	}
	*/

	// Fallback: random from provided indices
	if (RandomWanderActions.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, RandomWanderActions.Num()-1);
		return (int32)RandomWanderActions[Idx];
	}

	// Final fallback to MoveUpAction
	return (int32)MoveUpAction;
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

static int32 GetTagCount(const FGameStateData& GS, ERTSUnitTag Tag)
{
	switch (Tag)
	{
	case ERTSUnitTag::Alt1: return GS.Alt1TagFriendlyUnitCount;
	case ERTSUnitTag::Alt2: return GS.Alt2TagFriendlyUnitCount;
	case ERTSUnitTag::Alt3: return GS.Alt3TagFriendlyUnitCount;
	case ERTSUnitTag::Alt4: return GS.Alt4TagFriendlyUnitCount;
	case ERTSUnitTag::Alt5: return GS.Alt5TagFriendlyUnitCount;
	case ERTSUnitTag::Alt6: return GS.Alt6TagFriendlyUnitCount;

	case ERTSUnitTag::Ctrl1: return GS.Ctrl1TagFriendlyUnitCount;
	case ERTSUnitTag::Ctrl2: return GS.Ctrl2TagFriendlyUnitCount;
	case ERTSUnitTag::Ctrl3: return GS.Ctrl3TagFriendlyUnitCount;
	case ERTSUnitTag::Ctrl4: return GS.Ctrl4TagFriendlyUnitCount;
	case ERTSUnitTag::Ctrl5: return GS.Ctrl5TagFriendlyUnitCount;
	case ERTSUnitTag::Ctrl6: return GS.Ctrl6TagFriendlyUnitCount;

	case ERTSUnitTag::CtrlQ: return GS.CtrlQTagFriendlyUnitCount;
	case ERTSUnitTag::CtrlW: return GS.CtrlWTagFriendlyUnitCount;
	case ERTSUnitTag::CtrlE: return GS.CtrlETagFriendlyUnitCount;
	case ERTSUnitTag::CtrlR: return GS.CtrlRTagFriendlyUnitCount;
	default: break;
	}

	return 0;
}

FString URTSRuleBasedDeciderComponent::EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const
{
	const FString RowLabel = Row.RuleName.IsNone() ? TEXT("<Unnamed>") : Row.RuleName.ToString();
	if (bDebug && !Row.bEnabled)
	{
		UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' is disabled."), *RowLabel);
		return TEXT("{}");
	}

	AResourceGameState* RGState = GetWorld() ? GetWorld()->GetGameState<AResourceGameState>() : nullptr;

	auto CheckResource = [&](float Current, float Max, int32 Cost, EResourceType Type, const FString& Name) -> bool
	{
		if (Cost <= 0) return true;

		bool bIsSupply = false;
		if (RGState && RGState->IsSupplyLike.IsValidIndex(static_cast<int32>(Type)))
		{
			bIsSupply = RGState->IsSupplyLike[static_cast<int32>(Type)];
		}

		if (bIsSupply)
		{
			if (Current + (float)Cost > Max)
			{
				if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed %s (Supply): %.2f + %d > %.2f"), *RowLabel, *Name, Current, Cost, Max);
				return false;
			}
		}
		else
		{
			if (Current < (float)Cost)
			{
				if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed %s: %.2f < Thr %d"), *RowLabel, *Name, Current, Cost);
				return false;
			}
		}
		return true;
	};

	// Resource thresholds
	if (!CheckResource(GS.PrimaryResource, GS.MaxPrimaryResource, Row.ResourceThresholds.PrimaryCost, EResourceType::Primary, TEXT("Primary"))) return TEXT("{}");
	if (!CheckResource(GS.SecondaryResource, GS.MaxSecondaryResource, Row.ResourceThresholds.SecondaryCost, EResourceType::Secondary, TEXT("Secondary"))) return TEXT("{}");
	if (!CheckResource(GS.TertiaryResource, GS.MaxTertiaryResource, Row.ResourceThresholds.TertiaryCost, EResourceType::Tertiary, TEXT("Tertiary"))) return TEXT("{}");
	if (!CheckResource(GS.RareResource, GS.MaxRareResource, Row.ResourceThresholds.RareCost, EResourceType::Rare, TEXT("Rare"))) return TEXT("{}");
	if (!CheckResource(GS.EpicResource, GS.MaxEpicResource, Row.ResourceThresholds.EpicCost, EResourceType::Epic, TEXT("Epic"))) return TEXT("{}");
	if (!CheckResource(GS.LegendaryResource, GS.MaxLegendaryResource, Row.ResourceThresholds.LegendaryCost, EResourceType::Legendary, TEXT("Legendary"))) return TEXT("{}");

	// Caps
	if (!(GS.MyUnitCount < Row.MaxFriendlyUnitCount)) { if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed MyUnitCount cap: %d !< %d"), *RowLabel, GS.MyUnitCount, Row.MaxFriendlyUnitCount); return TEXT("{}"); }
	if (!(GS.MyUnitCount >= Row.MinFriendlyUnitCount)) { if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed MyUnitCount min: %d < %d"), *RowLabel, GS.MyUnitCount, Row.MinFriendlyUnitCount); return TEXT("{}"); }

	// Per-tag friendly unit caps
	if (Row.UnitCaps.Num() > 0)
	{
		bool bCapsPassed = (Row.UnitCapLogic == ERTSUnitCapLogic::AndLogic);

		for (const FRTSUnitCountCap& Cap : Row.UnitCaps)
		{
			const int32 Count = GetTagCount(GS, Cap.Tag);
			const bool bMatch = (Count >= Cap.MinCount && Count < Cap.MaxCount);

			if (Row.UnitCapLogic == ERTSUnitCapLogic::AndLogic)
			{
				if (!bMatch)
				{
					if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed tag cap (AND): tag=%d, count=%d, min=%d, max=%d"), *RowLabel, (int32)Cap.Tag, Count, Cap.MinCount, Cap.MaxCount);
					bCapsPassed = false;
					break;
				}
			}
			else // OrLogic
			{
				if (bMatch)
				{
					bCapsPassed = true;
					break;
				}
			}
		}

		if (!bCapsPassed)
		{
			if (Row.UnitCapLogic == ERTSUnitCapLogic::OrLogic)
			{
				if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed all tag caps (OR)."), *RowLabel);
			}
			return TEXT("{}");
		}
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' PASSED. Selection=%d Intermediate=%d Ability=%d"), 
		*RowLabel, (int32)Row.SelectionAction, (int32)Row.IntermediateAction, (int32)Row.AbilityAction);

	// Build output
	TArray<int32> ActionIndices;
	if (Row.SelectionAction != ERTSAIAction::None)
	{
		ActionIndices.Add((int32)Row.SelectionAction);
	}
	if (Row.IntermediateAction != ERTSAIAction::None)
	{
		ActionIndices.Add((int32)Row.IntermediateAction);
	}
	if (Row.AbilityAction != ERTSAIAction::None)
	{
		ActionIndices.Add((int32)Row.AbilityAction);
	}

	return BuildCompositeActionJSON(ActionIndices, Inference);
}

FString URTSRuleBasedDeciderComponent::EvaluateRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference) const
{
	if (!RulesDataTable)
	{
		if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("RuleBasedDecider: RulesDataTable is null; skipping table evaluation."));
		return TEXT("{}");
	}
	TArray<FName> RowNames = RulesDataTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: RulesDataTable has 0 rows."));
		return TEXT("{}");
	}

	struct FMatchingRule
	{
		FName Name;
		float Frequency;
		FString Output;
	};
	TArray<FMatchingRule> MatchingRules;
	float TotalFrequency = 0.0f;

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Evaluating %d DataTable rules."), RowNames.Num());
	for (const FName& Name : RowNames)
	{
		const FRTSRuleRow* Row = RulesDataTable->FindRow<FRTSRuleRow>(Name, TEXT("RTSRules"));
		if (!Row)
		{
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: DataTable row '%s' not found or mismatched type."), *Name.ToString());
			continue;
		}

		const FString Out = EvaluateRuleRow(*Row, GS, Inference);
		if (!Out.IsEmpty() && Out != TEXT("{}"))
		{
			float Freq = FMath::Max(0.0f, Row->Frequency);
			MatchingRules.Add({ Name, Freq, Out });
			TotalFrequency += Freq;
		}
	}

	if (MatchingRules.Num() > 0)
	{
		if (TotalFrequency > 0.0f)
		{
			float RandomValue = FMath::FRandRange(0.0f, TotalFrequency);
			float CumulativeFrequency = 0.0f;
			for (const FMatchingRule& Match : MatchingRules)
			{
				CumulativeFrequency += Match.Frequency;
				if (RandomValue <= CumulativeFrequency)
				{
					if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: DataTable rule '%s' fired (weighted random, freq=%.1f/%.1f)."), *Match.Name.ToString(), Match.Frequency, TotalFrequency);
					return Match.Output;
				}
			}
		}
		
		// Fallback to first matching if total frequency is 0
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: DataTable rule '%s' fired (fallback to first, total frequency was 0)."), *MatchingRules[0].Name.ToString());
		return MatchingRules[0].Output;
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: No DataTable rule matched."));
	return TEXT("{}");
}

bool URTSRuleBasedDeciderComponent::ExecuteAttackRuleRow(const FRTSAttackRuleRow& Row, int32 TableRowIndex, const FGameStateData& GS, UInferenceComponent* Inference)
{
	const FString RowLabel = Row.RuleName.IsNone() ? TEXT("<UnnamedAttack>") : Row.RuleName.ToString();
	if (!Row.bEnabled)
	{
		if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("RuleBasedDecider: AttackRow '%s' is disabled."), *RowLabel);
		return false;
	}
	if (!Inference)
	{
		return false;
	}

	TArray<int32> Indices;
	Indices.Reserve(32);
	auto AddPair = [&Indices](ERTSAIAction Action)
	{
		if (Action != ERTSAIAction::None)
		{
			Indices.Add((int32)Action);
			Indices.Add((int32)ERTSAIAction::LeftClick2); // Left Click 2 (Attack)
		}
	};

	// Process UnitCaps for attack selection
	for (const FRTSUnitCountCap& Cap : Row.UnitCaps)
	{
		const int32 Count = GetTagCount(GS, Cap.Tag);
		if (Count > 0 && Count >= Cap.MinCount && Count < Cap.MaxCount)
		{
			// Map tag to ERTSAIAction
			switch (Cap.Tag)
			{
			case ERTSUnitTag::Alt1: AddPair(ERTSAIAction::Alt1); break;
			case ERTSUnitTag::Alt2: AddPair(ERTSAIAction::Alt2); break;
			case ERTSUnitTag::Alt3: AddPair(ERTSAIAction::Alt3); break;
			case ERTSUnitTag::Alt4: AddPair(ERTSAIAction::Alt4); break;
			case ERTSUnitTag::Alt5: AddPair(ERTSAIAction::Alt5); break;
			case ERTSUnitTag::Alt6: AddPair(ERTSAIAction::Alt6); break;
			case ERTSUnitTag::Ctrl1: AddPair(ERTSAIAction::Ctrl1); break;
			case ERTSUnitTag::Ctrl2: AddPair(ERTSAIAction::Ctrl2); break;
			case ERTSUnitTag::Ctrl3: AddPair(ERTSAIAction::Ctrl3); break;
			case ERTSUnitTag::Ctrl4: AddPair(ERTSAIAction::Ctrl4); break;
			case ERTSUnitTag::Ctrl5: AddPair(ERTSAIAction::Ctrl5); break;
			case ERTSUnitTag::Ctrl6: AddPair(ERTSAIAction::Ctrl6); break;
			case ERTSUnitTag::CtrlQ: AddPair(ERTSAIAction::CtrlQ); break;
			case ERTSUnitTag::CtrlW: AddPair(ERTSAIAction::CtrlW); break;
			case ERTSUnitTag::CtrlE: AddPair(ERTSAIAction::CtrlE); break;
			case ERTSUnitTag::CtrlR: AddPair(ERTSAIAction::CtrlR); break;
			default: break;
			}
		}
	}

	if (Indices.Num() == 0)
	{
		if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("RuleBasedDecider: AttackRow '%s' had no qualifying selections (no actions)."), *RowLabel);
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ARLAgent* RLAgent = OwnerPawn ? Cast<ARLAgent>(OwnerPawn) : nullptr;
	if (!RLAgent)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: AttackRow '%s' could not execute: owner is not ARLAgent."), *RowLabel);
		return false;
	}

	// Choose attack position: class override if enabled and valid, otherwise row's default position
	FVector DesiredAttackPos = Row.AttackPosition;
	if (Row.UseClassAttackPositions && Row.AttackPositionSourceClasses.Num() > 0)
	{
		if (AttackPositions.IsValidIndex(TableRowIndex))
		{
			DesiredAttackPos = AttackPositions[TableRowIndex];
		}
	}

	const FVector OriginalLocation = RLAgent->GetActorLocation();
	// Adjust target location to ground with capsule clearance while not sinking below current Z
	auto ComputeGroundAdjusted = [&](const FVector& TargetXYOnly) -> FVector
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return FVector(TargetXYOnly.X, TargetXYOnly.Y, OriginalLocation.Z);
		}
		// Determine capsule half height (fallback to 88 if no capsule)
		float CapsuleHalfHeight = 88.f;
		if (UCapsuleComponent* Cap = RLAgent->FindComponentByClass<UCapsuleComponent>())
		{
			CapsuleHalfHeight = Cap->GetScaledCapsuleHalfHeight();
		}
		const float CurrentZ = RLAgent->GetActorLocation().Z;
		const FVector TraceStart(TargetXYOnly.X, TargetXYOnly.Y, CurrentZ + 10000.f);
		const FVector TraceEnd(TargetXYOnly.X, TargetXYOnly.Y, CurrentZ - 10000.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackRuleGroundTrace), /*bTraceComplex*/ false);
		Params.AddIgnoredActor(RLAgent);
		bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
		float OutZ = CurrentZ;
		if (bHit)
		{
			const float GroundZ = Hit.ImpactPoint.Z;
			if (GroundZ > CurrentZ)
			{
				OutZ = GroundZ + CapsuleHalfHeight;
			}
		}
		return FVector(TargetXYOnly.X, TargetXYOnly.Y, OutZ);
	};
	const FVector AdjustedAttackLoc = ComputeGroundAdjusted(DesiredAttackPos);
	RLAgent->SetActorLocation(AdjustedAttackLoc);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Adjusted attack move to (%.1f, %.1f, %.1f) from desired (%.1f, %.1f, %.1f)"),
		AdjustedAttackLoc.X, AdjustedAttackLoc.Y, AdjustedAttackLoc.Z,
		DesiredAttackPos.X, DesiredAttackPos.Y, DesiredAttackPos.Z);

	const FString Json = BuildCompositeActionJSON(Indices, Inference);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: AttackRow '%s' executing %d actions at AttackPosition (%.1f, %.1f, %.1f)."), *RowLabel, Indices.Num(), DesiredAttackPos.X, DesiredAttackPos.Y, DesiredAttackPos.Z);
	Inference->ExecuteActionFromJSON(Json);

	// Schedule return to original location after delay
	AttackReturnLocation = OriginalLocation;
	if (UWorld* World = GetWorld())
	{
		// Activate block until the timer completes
		bAttackReturnBlockActive = true;
		AttackReturnBlockUntilTimeSeconds = World->GetTimeSeconds() + AttackReturnDelaySeconds;

		FTimerHandle Handle;
		TWeakObjectPtr<URTSRuleBasedDeciderComponent> WeakDecider(this);
		World->GetTimerManager().SetTimer(Handle, [WeakDecider]()
		{
			if (WeakDecider.IsValid())
			{
				WeakDecider->FinalizeAttackReturn();
			}
		}, AttackReturnDelaySeconds, false);
	}

	return true;
}

void URTSRuleBasedDeciderComponent::FinalizeAttackReturn()
{
	if (!bAttackReturnBlockActive)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ARLAgent* Agent = OwnerPawn ? Cast<ARLAgent>(OwnerPawn) : nullptr;
	if (!Agent)
	{
		return;
	}

	UWorld* W = Agent->GetWorld();
	if (!W)
	{
		return;
	}

	float CapsuleHalfHeight = 88.f;
	if (UCapsuleComponent* Cap = Agent->FindComponentByClass<UCapsuleComponent>())
	{
		CapsuleHalfHeight = Cap->GetScaledCapsuleHalfHeight();
	}

	const float CurrentZ = Agent->GetActorLocation().Z;
	const FVector TraceStart(AttackReturnLocation.X, AttackReturnLocation.Y, CurrentZ + 10000.f);
	const FVector TraceEnd(AttackReturnLocation.X, AttackReturnLocation.Y, CurrentZ - 10000.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackRuleGroundTraceReturn), false);
	Params.AddIgnoredActor(Agent);

	FVector FinalLoc = AttackReturnLocation;
	if (W->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		const float GroundZ = Hit.ImpactPoint.Z;
		if (GroundZ > CurrentZ)
		{
			FinalLoc.Z = GroundZ + CapsuleHalfHeight;
		}
		else
		{
			FinalLoc.Z = CurrentZ;
		}
	}
	else
	{
		FinalLoc.Z = CurrentZ;
	}

	Agent->SetActorLocation(FinalLoc);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: RLAgent returned to adjusted location (%.1f, %.1f, %.1f) after attack."), FinalLoc.X, FinalLoc.Y, FinalLoc.Z);

	// Post-return action: left_click 1
	if (UInferenceComponent* PostInf = Agent->FindComponentByClass<UInferenceComponent>())
	{
		const ERTSAIAction PostReturnAction = ERTSAIAction::LeftClick1;
		const FString ClickJson = PostInf->GetActionAsJSON((int32)PostReturnAction);
		PostInf->ExecuteActionFromJSON(ClickJson);
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Post-return action executed: left_click 1 (index %d)."), (int32)PostReturnAction);
	}

	bAttackReturnBlockActive = false;
	AttackReturnBlockUntilTimeSeconds = 0.f;
}

bool URTSRuleBasedDeciderComponent::EvaluateAttackRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference)
{
	if (!AttackRulesDataTable)
	{
		return false;
	}
	TArray<FName> RowNames = AttackRulesDataTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		return false;
	}

	struct FMatchingAttackRule
	{
		FName Name;
		const FRTSAttackRuleRow* Row;
		int32 OriginalIndex;
		float Frequency;
	};
	TArray<FMatchingAttackRule> MatchingRules;
	float TotalFrequency = 0.0f;

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Evaluating %d Attack DataTable rows."), RowNames.Num());
	for (int32 i = 0; i < RowNames.Num(); ++i)
	{
		const FName& Name = RowNames[i];
		const FRTSAttackRuleRow* Row = AttackRulesDataTable->FindRow<FRTSAttackRuleRow>(Name, TEXT("RTSAttackRules"));
		if (!Row)
		{
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: Attack DataTable row '%s' not found or mismatched type."), *Name.ToString());
			continue;
		}

		if (!Row->bEnabled)
		{
			continue;
		}

		// Check if the rule is executable (has valid selections)
		// Note: We need a lightweight way to check this without executing.
		// For now, let's see if we can refactor ExecuteAttackRuleRow to separate check and execute, 
		// but since it depends on many factors, we'll do a quick check here similar to what it does.
		
		bool bHasSelections = false;
		if (Row->UnitCaps.Num() > 0)
		{
			if (Row->UnitCapLogic == ERTSUnitCapLogic::AndLogic)
			{
				bHasSelections = true;
				bool bHasAnyUnitsToSend = false;
				for (const FRTSUnitCountCap& Cap : Row->UnitCaps)
				{
					const int32 Count = GetTagCount(GS, Cap.Tag);
					if (Count < Cap.MinCount || Count >= Cap.MaxCount)
					{
						bHasSelections = false;
						break;
					}
					if (Count > 0) bHasAnyUnitsToSend = true;
				}
				// Fail AND rule if literally 0 units would be sent (even if all 0 counts are within [Min, Max] ranges)
				if (!bHasAnyUnitsToSend) bHasSelections = false;
			}
			else // OrLogic
			{
				for (const FRTSUnitCountCap& Cap : Row->UnitCaps)
				{
					const int32 Count = GetTagCount(GS, Cap.Tag);
					if (Count > 0 && Count >= Cap.MinCount && Count < Cap.MaxCount)
					{
						bHasSelections = true;
						break;
					}
				}
			}
		}

		if (bHasSelections)
		{
			float Freq = FMath::Max(0.0f, Row->Frequency);
			MatchingRules.Add({ Name, Row, i, Freq });
			TotalFrequency += Freq;
		}
	}

	if (MatchingRules.Num() > 0)
	{
		const FMatchingAttackRule* SelectedMatch = nullptr;
		if (TotalFrequency > 0.0f)
		{
			float RandomValue = FMath::FRandRange(0.0f, TotalFrequency);
			float CumulativeFrequency = 0.0f;
			for (const FMatchingAttackRule& Match : MatchingRules)
			{
				CumulativeFrequency += Match.Frequency;
				if (RandomValue <= CumulativeFrequency)
				{
					SelectedMatch = &Match;
					break;
				}
			}
		}

		if (!SelectedMatch)
		{
			SelectedMatch = &MatchingRules[0];
		}

		if (SelectedMatch && ExecuteAttackRuleRow(*(SelectedMatch->Row), SelectedMatch->OriginalIndex, GS, Inference))
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Attack rule '%s' fired (weighted random, freq=%.1f/%.1f)."), *SelectedMatch->Name.ToString(), SelectedMatch->Frequency, TotalFrequency);
			return true;
		}
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: No Attack DataTable rule matched."));
	return false;
}

void URTSRuleBasedDeciderComponent::PopulateAttackPositions()
{
	if (!bUseAttackDataTableRules || !AttackRulesDataTable)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FName> RowNames = AttackRulesDataTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		return;
	}

	// 1. Collect unique classes and prepare per-row class lists
	TSet<TSubclassOf<AActor>> UniqueClasses;
	TArray<const FRTSAttackRuleRow*> Rows;
	Rows.Reserve(RowNames.Num());

	for (const FName& Name : RowNames)
	{
		const FRTSAttackRuleRow* Row = AttackRulesDataTable->FindRow<FRTSAttackRuleRow>(Name, TEXT("RTSAttackRules"));
		Rows.Add(Row);
		if (Row)
		{
			for (const TSubclassOf<AActor>& Cls : Row->AttackPositionSourceClasses)
			{
				if (Cls)
				{
					UniqueClasses.Add(Cls);
				}
			}
		}
	}

	// 2. Efficiently find all actors of these classes (Single pass over actors as requested)
	TMap<TSubclassOf<AActor>, TArray<FVector>> FoundLocationsMap;
	for (const TSubclassOf<AActor>& Cls : UniqueClasses)
	{
		FoundLocationsMap.Add(Cls, TArray<FVector>());
	}

	if (UniqueClasses.Num() > 0)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			UClass* ActorClass = Actor->GetClass();
			// We check if the actor matches any of our target classes or their subclasses
			for (auto& Pair : FoundLocationsMap)
			{
				if (ActorClass->IsChildOf(Pair.Key))
				{
					Pair.Value.Add(Actor->GetActorLocation());
				}
			}
		}
	}

	// 3. Update AttackPositions (one per row)
	AttackPositions.SetNum(RowNames.Num());
	bool bAnyRowHasClasses = false;

	for (int32 i = 0; i < RowNames.Num(); ++i)
	{
		const FRTSAttackRuleRow* Row = Rows[i];
		if (Row && Row->AttackPositionSourceClasses.Num() > 0)
		{
			bAnyRowHasClasses = true;

			// Pick all possible locations from all requested classes for this specific row
			TArray<FVector> PossibleLocations;
			for (const TSubclassOf<AActor>& Cls : Row->AttackPositionSourceClasses)
			{
				if (const TArray<FVector>* Locs = FoundLocationsMap.Find(Cls))
				{
					PossibleLocations.Append(*Locs);
				}
			}

			if (PossibleLocations.Num() > 0)
			{
				const int32 RandIdx = FMath::RandRange(0, PossibleLocations.Num() - 1);
				AttackPositions[i] = PossibleLocations[RandIdx];
			}
			else
			{
				// Fallback to row position if no actors found
				AttackPositions[i] = Row->AttackPosition;
			}
		}
		else if (Row)
		{
			// No classes specified for this row, use its default position
			AttackPositions[i] = Row->AttackPosition;
		}
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Refreshed AttackPositions for %d rows. Unique target classes searched: %d"), RowNames.Num(), UniqueClasses.Num());

	// 4. Setup next refresh if needed
	if (bAnyRowHasClasses && AttackPositionRefreshInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(AttackPositionRefreshTimerHandle, this, &URTSRuleBasedDeciderComponent::PopulateAttackPositions, AttackPositionRefreshInterval, false);
	}
}

FString URTSRuleBasedDeciderComponent::ChooseJsonActionRuleBased(const FGameStateData& GameState)
{
	UInferenceComponent* Inference = GetInferenceComponent();
	if (!Inference)
	{
		if (bDebug) UE_LOG(LogTemp, Error, TEXT("URTSRuleBasedDeciderComponent: No UInferenceComponent found on owning pawn. Returning {}."));
		return TEXT("{}");
	}

	// If we are in the middle of an attack-return block, do not produce any actions until timer finishes
	{
		UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		if (bAttackReturnBlockActive)
		{
			if (Now < AttackReturnBlockUntilTimeSeconds)
			{
				const float Remaining = AttackReturnBlockUntilTimeSeconds - Now;
				if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("RuleBasedDecider: Blocking actions during attack return (%.2fs remaining)."), Remaining);
				return TEXT("{}");
			}

			// Safety: block expired but timer didn't clear it yet.
			// Force return move now and clear the block.
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: Attack return block expired via safety; forcing return now."));
			FinalizeAttackReturn();

			// Fall through to evaluate rules normally now that we are supposedly back
		}
	}

	// Attack rules have priority but are throttled by AttackRuleCheckIntervalSeconds.
	if (bUseAttackDataTableRules && AttackRulesDataTable)
	{
		UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		const float SinceLast = Now - LastAttackRuleCheckTimeSeconds;
		if (SinceLast >= AttackRuleCheckIntervalSeconds)
		{
			LastAttackRuleCheckTimeSeconds = Now; // mark the attempt time regardless of outcome
			if (EvaluateAttackRulesFromDataTable(GameState, Inference))
			{
				// Actions executed internally; no external JSON needed
				return TEXT("{}");
			}
		}
		else
		{
			if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("RuleBasedDecider: Skipping AttackRules (cooldown %.1fs left)."), AttackRuleCheckIntervalSeconds - SinceLast);
		}
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
			// First pick a base candidate according to existing rules
			const int32 BaseIdx = PickWanderActionIndex(GameState);
			int32 ChosenIdx = BaseIdx;
			// Initialize tracking if first time
			if (LastWanderActionIndex == INDEX_NONE)
			{
				LastWanderActionIndex = BaseIdx;
				WanderActionRepeatCount = 0; // will be incremented below when we emit
			}
			// Enforce repeating the same direction at least WanderMinSameDirectionRepeats times before switching
			const bool bForceKeepLast = (LastWanderActionIndex != INDEX_NONE) && (LastWanderActionIndex != BaseIdx) && (WanderActionRepeatCount < WanderMinSameDirectionRepeats);
			ChosenIdx = bForceKeepLast ? LastWanderActionIndex : BaseIdx;
			// Update tracking based on the actually chosen direction
			if (ChosenIdx == LastWanderActionIndex)
			{
				WanderActionRepeatCount = FMath::Max(1, WanderActionRepeatCount + 1);
			}
			else
			{
				LastWanderActionIndex = ChosenIdx;
				WanderActionRepeatCount = 1;
			}
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Wander path. Base=%d, Chosen=%d, Repeats=%d/%d, TwoStep=%s (SelIdx=%d, AbIdx=%d)"), BaseIdx, ChosenIdx, WanderActionRepeatCount, WanderMinSameDirectionRepeats, bWanderTwoStep?TEXT("true"):TEXT("false"), (int32)WanderSelectionAction, (int32)WanderAbilityAction);
			if (bWanderTwoStep)
			{
				TArray<int32> Steps;
				Steps.Add((int32)WanderSelectionAction);
				// If a specific ability is provided, use it; otherwise use the movement as the second step
				Steps.Add((WanderAbilityAction != ERTSAIAction::None) ? (int32)WanderAbilityAction : ChosenIdx);
				return BuildCompositeActionJSON(Steps, Inference);
			}
			return Inference->GetActionAsJSON(ChosenIdx);
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
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: Neither DataTable nor Wander produced an action (table=%s, wander=%s). Returning {}."),
			(bUseDataTableRules && RulesDataTable)?TEXT("enabled"):TEXT("disabled"), bEnableWander?TEXT("enabled"):TEXT("disabled"));
	}
	return Result;
}