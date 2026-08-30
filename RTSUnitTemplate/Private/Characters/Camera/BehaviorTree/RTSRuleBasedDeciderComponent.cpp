// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"

namespace
{
	/**
	 * 1 = auch Wander-Entscheidungen aufzeichnen (bisheriges Verhalten).
	 * 0 = nur Entscheidungen der Regeltabelle.
	 *
	 * Der Wander-Pfad wuerfelt (FMath::RandRange) und stellt die Haelfte aller Entscheidungen.
	 * Ob das Weglassen die Uebereinstimmung hebt, ist im Training messbar - ohne eine einzige
	 * Partie ausgeben zu muessen.
	 */
	/**
	 * 0 = der Wander-Pfad wird gar nicht erst versucht.
	 *
	 * Zur Eingrenzung: Arbeiter werden gemessen ~500 mal je Partie aus ihrer Arbeit in den
	 * Laufzustand gerissen. Die Angriffsbefehle sind es nachweislich nicht (75 Befehle,
	 * davonArbeiter=0 in allen). Der Wander-Pfad ist die Haelfte aller KI-Entscheidungen und
	 * schickt eine Kontrollgruppe irgendwohin - wenn die Arbeiter enthaelt, ist er die Quelle.
	 * Bricht die Zahl mit diesem Schalter ein, ist es belegt.
	 */
	static int32 GRTSWanderPath = 1;
	static FAutoConsoleVariableRef CVarRTSWanderPath(
		TEXT("rts.ai.wander"),
		GRTSWanderPath,
		TEXT("Wander-Pfad der Regel-KI. 0 = aus (nur zur Eingrenzung, die KI verliert damit ihren Rueckfall)."),
		ECVF_Default);

	static int32 GRLRecordWander = 1;
	static FAutoConsoleVariableRef CVarRLRecordWander(
		TEXT("rts.rl.record.wander"),
		GRLRecordWander,
		TEXT("Wander-Entscheidungen (reiner Zufall) mit aufzeichnen. 0 = nur Regeltabelle."),
		ECVF_Default);
}

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
#include "GameModes/ResourceGameMode.h"
#include "GAS/GameplayAbilityBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Actors/WorkArea.h"
#include "GameplayTagContainer.h"
#include "Characters/Camera/RL/RLRecorderSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

namespace
{
	/**
	 * 1 = always take the highest-frequency matching rule instead of sampling among the matches.
	 *
	 * This exists for recording RL training data. A network emits one action per state, so it can never
	 * reproduce a teacher that rolls dice - behaviour cloning against the sampling AI measured a hard
	 * ceiling of ~34% agreement, with half of all recorded states mapping to more than one action. With
	 * deterministic selection the same state always yields the same action and that ceiling disappears.
	 *
	 * Off during normal play on purpose: it makes the AI open identically every match.
	 */
	static int32 GRTSRulesDeterministic = 0;
	static FAutoConsoleVariableRef CVarRTSRulesDeterministic(
		TEXT("rts.ai.rules.deterministic"),
		GRTSRulesDeterministic,
		TEXT("1 = rule AI always picks its highest-frequency matching rule (use while recording RL data)."),
		ECVF_Default);

	bool IsDeterministicRuleSelection()
	{
		return GRTSRulesDeterministic != 0;
	}
}
#include "NavigationSystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Characters/Camera/BehaviorTree/RTSBTController.h"

URTSRuleBasedDeciderComponent::URTSRuleBasedDeciderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default random wander choices = move_camera 1..4 (indices 17..20)
	RandomWanderActions = { ERTSAIAction::MoveDirection1, ERTSAIAction::MoveDirection2, ERTSAIAction::MoveDirection3, ERTSAIAction::MoveDirection4 };
}

int32 URTSRuleBasedDeciderComponent::ResolveOwningTeamId() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const ARTSBTController* BTC = Cast<ARTSBTController>(OwnerPawn->GetController()))
		{
			return BTC->OrchestratorTeamId;
		}
		if (const AControllerBase* CB = Cast<AControllerBase>(OwnerPawn->GetController()))
		{
			return CB->SelectableTeamId;
		}
	}
	return -1;
}

void URTSRuleBasedDeciderComponent::ApplyTeamTableOverrides()
{
	if (bTeamTablesApplied) return;

	if (TeamRulesDataTables.Num() == 0 && TeamAttackRulesDataTables.Num() == 0)
	{
		bTeamTablesApplied = true;
		return;
	}

	const int32 MyTeamId = ResolveOwningTeamId();
	if (MyTeamId < 0) return; // Not possessed yet - retry on the next evaluation.

	if (UDataTable* const* Found = TeamRulesDataTables.Find(MyTeamId))
	{
		if (*Found) RulesDataTable = *Found;
	}
	if (UDataTable* const* Found = TeamAttackRulesDataTables.Find(MyTeamId))
	{
		if (*Found) AttackRulesDataTable = *Found;
	}
	bTeamTablesApplied = true;

	// Unlock whatever this AI is expected to be able to build. A player earns these through progression;
	// the agent has no such path, so without this its rules keep firing against permanently locked abilities.
	for (const FString& Key : ForceEnabledAbilityKeys)
	{
		if (!Key.IsEmpty())
		{
			UGameplayAbilityBase::SetAbilitiesForceEnabledForTeamByKey_Static(Key, MyTeamId, true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Team %d uses rules '%s' and attack rules '%s', %d ability key(s) unlocked."),
		MyTeamId, *GetNameSafe(RulesDataTable), *GetNameSafe(AttackRulesDataTable), ForceEnabledAbilityKeys.Num());
}

void URTSRuleBasedDeciderComponent::BeginPlay()
{
	Super::BeginPlay();

	// The pawn is possessed after this component's BeginPlay, so the team id is only readable one tick later.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &URTSRuleBasedDeciderComponent::InitializeRuleTables);
	}
}

void URTSRuleBasedDeciderComponent::InitializeRuleTables()
{
	ApplyTeamTableOverrides();

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

	// Recording mode: a die roll here would put the same state in the training set with four different
	// answers. Steer toward the enemy instead - deterministic, and unlike a hash of the position it is
	// behaviour worth teaching.
	if (IsDeterministicRuleSelection())
	{
		const FVector Delta3D = GS.AverageEnemyPosition - GS.AgentPosition;
		if (Delta3D.Size2D() > KINDA_SMALL_NUMBER)
		{
			switch (ArgMax2D(FVector2D(Delta3D.X, Delta3D.Y)))
			{
				case 0: return (int32)MoveRightAction;
				case 1: return (int32)MoveLeftAction;
				case 2: return (int32)MoveUpAction;
				default: return (int32)MoveDownAction;
			}
		}

		if (RandomWanderActions.Num() > 0)
		{
			return (int32)RandomWanderActions[0];
		}
	}

	// Fallback: random from provided indices
	if (RandomWanderActions.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, RandomWanderActions.Num()-1);
		return (int32)RandomWanderActions[Idx];
	}

	// Final fallback to MoveUpAction
	return (int32)MoveUpAction;
}

void URTSRuleBasedDeciderComponent::RecordDecisionForTraining(const TArray<int32>& Indices) const
{
	if (!bHasCachedRecordingState)
	{
		return;
	}

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	URLRecorderSubsystem* Recorder = GameInstance ? GameInstance->GetSubsystem<URLRecorderSubsystem>() : nullptr;
	if (!Recorder || !Recorder->IsRecording())
	{
		return;
	}

	// One line per action, because the network also emits one action per inference step. The two halves of
	// a composite decision ("select the workers, then press ability 3") share a world state, so each sample
	// carries the action that preceded it - otherwise the same state would appear with two different
	// answers and the pair could never be learned.
	// Wuerfelentscheidungen aus den Trainingsdaten halten, wenn so eingestellt. Siehe
	// bLastDecisionWasWander: der Wander-Pfad ist die Haelfte aller Entscheidungen und traegt
	// keine Absicht, die sich nachahmen liesse.
	if (GRLRecordWander == 0 && bLastDecisionWasWander)
	{
		return;
	}

	const int32 TeamId = ResolveOwningTeamId();
	FGameStateData StateForSample = CachedRecordingState;

	for (int32 Index : Indices)
	{
		if (Index < 0 || Index == (int32)ERTSAIAction::None)
		{
			continue;
		}

		StateForSample.LastActionIndex = LastRecordedActionIndex;
		Recorder->RecordSample(TeamId, UInferenceComponent::StateToArray(StateForSample), Index, ERLSampleSource::RuleBased);
		LastRecordedActionIndex = Index;
	}
}

FString URTSRuleBasedDeciderComponent::BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference,
                                                               int32 AbilityArrayIndexOverride,
                                                               bool bAimAtTransporter) const
{
	if (!Inference || Indices.Num() == 0)
	{
		return TEXT("{}");
	}

	RecordDecisionForTraining(Indices);

	if (Indices.Num() == 1 && AbilityArrayIndexOverride < 0 && !bAimAtTransporter)
	{
		return Inference->GetActionAsJSON(Indices[0]);
	}

	if (Indices.Num() == 1)
	{
		TSharedRef<TJsonReader<>> SingleReader = TJsonReaderFactory<>::Create(Inference->GetActionAsJSON(Indices[0]));
		TSharedPtr<FJsonObject> SingleObj;
		if (FJsonSerializer::Deserialize(SingleReader, SingleObj) && SingleObj.IsValid())
		{
			if (AbilityArrayIndexOverride >= 0) SingleObj->SetNumberField(TEXT("ability_array_index"), AbilityArrayIndexOverride);
			if (bAimAtTransporter) SingleObj->SetBoolField(TEXT("aim_at_transporter"), true);
			FString SingleOut;
			auto SingleWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SingleOut);
			FJsonSerializer::Serialize(SingleObj.ToSharedRef(), SingleWriter);
			return SingleOut;
		}
		return Inference->GetActionAsJSON(Indices[0]);
	}

	// Build an array of action objects
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	JsonValues.Reserve(Indices.Num());

	for (int32 Idx : Indices)
	{
		// Skip None/out-of-range sentinels (ERTSAIAction::None == 255) so a misconfigured action
		// no longer spams "Invalid ActionIndex 255 provided" on every decision tick.
		if (Idx < 0 || Idx == (int32)ERTSAIAction::None)
		{
			continue;
		}
		const FString ActionJson = Inference->GetActionAsJSON(Idx);
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ActionJson);
		TSharedPtr<FJsonObject> Obj;
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			if (AbilityArrayIndexOverride >= 0)
			{
				Obj->SetNumberField(TEXT("ability_array_index"), AbilityArrayIndexOverride);
			}
			if (bAimAtTransporter)
			{
				Obj->SetBoolField(TEXT("aim_at_transporter"), true);
			}
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

// "Ctrl5" for ERTSAIAction::Ctrl5 and so on - the suffix of the KeyTag a selection action maps to.
static FString SelectionActionToKeyTagSuffix(ERTSAIAction Action)
{
	switch (Action)
	{
	case ERTSAIAction::CtrlQ: return TEXT("CtrlQ");
	case ERTSAIAction::CtrlW: return TEXT("CtrlW");
	case ERTSAIAction::CtrlE: return TEXT("CtrlE");
	case ERTSAIAction::CtrlR: return TEXT("CtrlR");
	case ERTSAIAction::Ctrl1: return TEXT("Ctrl1");
	case ERTSAIAction::Ctrl2: return TEXT("Ctrl2");
	case ERTSAIAction::Ctrl3: return TEXT("Ctrl3");
	case ERTSAIAction::Ctrl4: return TEXT("Ctrl4");
	case ERTSAIAction::Ctrl5: return TEXT("Ctrl5");
	case ERTSAIAction::Ctrl6: return TEXT("Ctrl6");
	default: return FString();
	}
}

bool URTSRuleBasedDeciderComponent::TryGetAbilityCostForRule(const FRTSRuleRow& Row, FBuildingCost& OutCost) const
{
	const int32 AbilityIndex = static_cast<int32>(Row.AbilityAction) - static_cast<int32>(ERTSAIAction::Ability1);
	if (AbilityIndex < 0 || AbilityIndex > 5)
	{
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("DeriveCost: bad ability index %d"), AbilityIndex);
		return false;
	}

	const FString TagSuffix = SelectionActionToKeyTagSuffix(Row.SelectionAction);
	if (TagSuffix.IsEmpty())
	{
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("DeriveCost: selection %d has no key tag"), (int32)Row.SelectionAction);
		return false;
	}

	const FGameplayTag KeyTag = FGameplayTag::RequestGameplayTag(FName(*(TEXT("KeyTag.") + TagSuffix)), false);
	if (!KeyTag.IsValid())
	{
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("DeriveCost: tag KeyTag.%s not registered"), *TagSuffix);
		return false;
	}

	UWorld* World = GetWorld();
	ARTSGameModeBase* GameMode = World ? Cast<ARTSGameModeBase>(World->GetAuthGameMode()) : nullptr;
	if (!GameMode)
	{
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("DeriveCost: no RTSGameModeBase"));
		return false;
	}

	const int32 TeamId = ResolveOwningTeamId();

	// The rule presses one key for a whole group, and every member resolves its own ability, so the
	// binding cost is the cheapest member - gating on the priciest one blocks the affordable ones.
	//
	// "Cheapest" used to mean PrimaryCost alone, and that was wrong: several Singularian units cost
	// NOTHING in Primary and pay in Epic/Legendary instead (SalvoDroid 0/150/100, EdgeDancer 0/125/75,
	// ShardInterceptor 0/150/50). With PrimaryCost 0 those always won the comparison and gated the
	// whole rule on Epic - a resource the team barely accumulates. Measured in the 16.08. match:
	// 'Units_A' failed 168 times on "Epic: 0.00 < Thr 150" while the Vector (100 Primary, no Epic) sat
	// on the very same key and was affordable the whole time. So: prefer an ability the team can
	// actually pay for, and fall back to the lowest TOTAL cost rather than the lowest Primary.
	AResourceGameMode* ResourceMode = Cast<AResourceGameMode>(GameMode);
	auto TotalCost = [](const FBuildingCost& C) -> int32
	{
		return C.PrimaryCost + C.SecondaryCost + C.TertiaryCost + C.RareCost + C.EpicCost + C.LegendaryCost;
	};

	bool bFound = false;
	bool bFoundAffordable = false;
	for (AActor* Actor : GameMode->AllUnits)
	{
		AUnitBase* Unit = Cast<AUnitBase>(Actor);
		if (!Unit || Unit->TeamId != TeamId) continue;
		if (!Unit->UnitTags.HasTagExact(KeyTag)) continue;

		// Follow the rule's own array. Reading DefaultAbilities unconditionally gave array-1 and -2
		// rules the cost of a completely different building: an Antimatter rule was gated on the Base's
		// price, passed, pressed - and the ability then failed silently on the real 550/350/300.
		const TArray<TSubclassOf<UGameplayAbilityBase>>* Arr = nullptr;
		switch (Row.AbilityArrayIndex)
		{
		case 1:  Arr = &Unit->SecondAbilities; break;
		case 2:  Arr = &Unit->ThirdAbilities;  break;
		case 3:  Arr = &Unit->FourthAbilities; break;
		default: Arr = &Unit->DefaultAbilities; break;
		}
		if (!Arr || !Arr->IsValidIndex(AbilityIndex)) continue;

		const TSubclassOf<UGameplayAbilityBase> AbilityClass = (*Arr)[AbilityIndex];
		if (!AbilityClass) continue;

		const UGameplayAbilityBase* CDO = AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (!CDO) continue;

		const FBuildingCost& Cost = CDO->ConstructionCost;
		const bool bAffordable = ResourceMode && ResourceMode->CanAffordConstruction(Cost, TeamId);

		if (bAffordable)
		{
			// An affordable member always beats an unaffordable one, no matter how "cheap" the latter
			// looks on a single resource column.
			if (!bFoundAffordable || TotalCost(Cost) < TotalCost(OutCost))
			{
				OutCost = Cost;
				bFound = true;
				bFoundAffordable = true;
			}
		}
		else if (!bFoundAffordable && (!bFound || TotalCost(Cost) < TotalCost(OutCost)))
		{
			OutCost = Cost;
			bFound = true;
		}
	}

	if (bDebug && !bFound)
	{
		UE_LOG(LogTemp, Log, TEXT("DeriveCost: no unit with KeyTag.%s on team %d owning ability index %d (AllUnits=%d)"),
			*TagSuffix, TeamId, AbilityIndex, GameMode->AllUnits.Num());
	}

	return bFound;
}

bool URTSRuleBasedDeciderComponent::IsRuleOnCooldown(const FRTSRuleRow& Row, const FName& RowName) const
{
	if (Row.MinSecondsBetweenActivations <= 0.f) return false;

	const float* Last = LastRuleActivationTime.Find(RowName);
	if (!Last) return false;

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	return (Now - *Last) < Row.MinSecondsBetweenActivations;
}

bool URTSRuleBasedDeciderComponent::IsAttackRuleOnCooldown(const FRTSAttackRuleRow& Row, const FName& RowName) const
{
	if (Row.MinSecondsBetweenActivations <= 0.f) return false;

	const float* Last = LastAttackRuleActivationTime.Find(RowName);
	if (!Last) return false;

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	return (Now - *Last) < Row.MinSecondsBetweenActivations;
}

void URTSRuleBasedDeciderComponent::MarkAttackRuleFired(const FName& RowName) const
{
	const UWorld* World = GetWorld();
	LastAttackRuleActivationTime.Add(RowName, World ? World->GetTimeSeconds() : 0.f);
}

void URTSRuleBasedDeciderComponent::MarkRuleFired(const FName& RowName) const
{
	const UWorld* World = GetWorld();
	LastRuleActivationTime.Add(RowName, World ? World->GetTimeSeconds() : 0.f);

	// The ability array is NOT selected here on purpose: this is decision time, the press happens
	// later, and the next decision would overwrite the controller field before it is read. The index
	// travels inside the action JSON instead and is applied right before the press.
}

FString URTSRuleBasedDeciderComponent::EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const
{
	// Team MIT in das Label: beide Fraktionen schreiben in DIESELBE Logdatei, und die Zeilen
	// darunter nannten nur den Regelnamen. Beim Auswerten der Grundlinie am 18.08. hat mich das
	// dreimal in die Irre gefuehrt - zuletzt zaehlte ich 202 Ladeaktionen als Leerlaufschleife der
	// Xeno, obwohl es die der Singularianer waren. Eine Zaehlung ohne Zuordnung ist wertlos, und das
	// Label ist die einzige Stelle, an der man alle Meldungen dieser Funktion auf einmal erreicht.
	const FString RowLabel = FString::Printf(TEXT("T%d:%s"),
		ResolveOwningTeamId(),
		Row.RuleName.IsNone() ? TEXT("<Unnamed>") : *Row.RuleName.ToString());
	if (bDebug && !Row.bEnabled)
	{
		UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' is disabled."), *RowLabel);
		return TEXT("{}");
	}

	// Game time check
	if (UWorld* World = GetWorld())
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime < Row.GameTimeCap.Min || CurrentTime > Row.GameTimeCap.Max)
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed GameTime check: %.2f not in [%.2f, %.2f]"), *RowLabel, CurrentTime, Row.GameTimeCap.Min, Row.GameTimeCap.Max);
			return TEXT("{}");
		}
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

	// Resource thresholds. Prefer the real cost of the ability this rule presses over the table value -
	// see bDeriveThresholdsFromAbility for why the table cannot be trusted to stay in sync.
	FBuildingCost Thr = Row.ResourceThresholds;
	if (bDeriveThresholdsFromAbility)
	{
		FBuildingCost Derived;
		if (TryGetAbilityCostForRule(Row, Derived))
		{
			auto Scale = [this](int32 Value) -> int32
			{
				return Value > 0 ? FMath::CeilToInt(Value * DerivedThresholdMultiplier) : 0;
			};
			Thr.PrimaryCost   = Scale(Derived.PrimaryCost);
			Thr.SecondaryCost = Scale(Derived.SecondaryCost);
			Thr.TertiaryCost  = Scale(Derived.TertiaryCost);
			// Supply-like resources are compared against the remaining cap, so scaling would
			// reserve headroom that does not exist. Use the real cost.
			Thr.RareCost      = Derived.RareCost;
			Thr.EpicCost      = Derived.EpicCost;
			Thr.LegendaryCost = Derived.LegendaryCost;
		}
	}

	if (!CheckResource(GS.PrimaryResource, GS.MaxPrimaryResource, Thr.PrimaryCost, EResourceType::Primary, TEXT("Primary"))) return TEXT("{}");
	if (!CheckResource(GS.SecondaryResource, GS.MaxSecondaryResource, Thr.SecondaryCost, EResourceType::Secondary, TEXT("Secondary"))) return TEXT("{}");
	if (!CheckResource(GS.TertiaryResource, GS.MaxTertiaryResource, Thr.TertiaryCost, EResourceType::Tertiary, TEXT("Tertiary"))) return TEXT("{}");
	if (!CheckResource(GS.RareResource, GS.MaxRareResource, Thr.RareCost, EResourceType::Rare, TEXT("Rare"))) return TEXT("{}");
	if (!CheckResource(GS.EpicResource, GS.MaxEpicResource, Thr.EpicCost, EResourceType::Epic, TEXT("Epic"))) return TEXT("{}");
	if (!CheckResource(GS.LegendaryResource, GS.MaxLegendaryResource, Thr.LegendaryCost, EResourceType::Legendary, TEXT("Legendary"))) return TEXT("{}");

	// Upper bounds: the rule is only allowed while the resource is still SHORT. Supply-like resources are
	// judged by their remaining headroom, since "running out of energy" means the cap is close, not that
	// the stored amount is small.
	auto CheckResourceMax = [&](float Current, float Max, int32 Limit, EResourceType Type, const FString& Name) -> bool
	{
		if (Limit <= 0) return true;

		bool bIsSupply = false;
		if (RGState && RGState->IsSupplyLike.IsValidIndex(static_cast<int32>(Type)))
		{
			bIsSupply = RGState->IsSupplyLike[static_cast<int32>(Type)];
		}

		const float Value = bIsSupply ? (Max - Current) : Current;
		if (Value >= (float)Limit)
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed %s upper bound: %.2f >= %d%s"),
				*RowLabel, *Name, Value, Limit, bIsSupply ? TEXT(" (headroom)") : TEXT(""));
			return false;
		}
		return true;
	};

	if (!CheckResourceMax(GS.PrimaryResource, GS.MaxPrimaryResource, Row.ResourceMaxThresholds.PrimaryCost, EResourceType::Primary, TEXT("Primary"))) return TEXT("{}");
	if (!CheckResourceMax(GS.SecondaryResource, GS.MaxSecondaryResource, Row.ResourceMaxThresholds.SecondaryCost, EResourceType::Secondary, TEXT("Secondary"))) return TEXT("{}");
	if (!CheckResourceMax(GS.TertiaryResource, GS.MaxTertiaryResource, Row.ResourceMaxThresholds.TertiaryCost, EResourceType::Tertiary, TEXT("Tertiary"))) return TEXT("{}");
	if (!CheckResourceMax(GS.RareResource, GS.MaxRareResource, Row.ResourceMaxThresholds.RareCost, EResourceType::Rare, TEXT("Rare"))) return TEXT("{}");
	if (!CheckResourceMax(GS.EpicResource, GS.MaxEpicResource, Row.ResourceMaxThresholds.EpicCost, EResourceType::Epic, TEXT("Epic"))) return TEXT("{}");
	if (!CheckResourceMax(GS.LegendaryResource, GS.MaxLegendaryResource, Row.ResourceMaxThresholds.LegendaryCost, EResourceType::Legendary, TEXT("Legendary"))) return TEXT("{}");

	// Caps
	// Counting by classification tag serves TWO different jobs, and they need opposite answers:
	// the rule's own cap must SEE construction sites (otherwise a short cooldown queues one too many),
	// while the build-order prerequisite must NOT (otherwise the successor overtakes its predecessor).
	auto CountByClassTag = [this](const FGameplayTag& Tag, bool bIncludePendingAreas) -> int32
	{
		int32 Count = 0;
		UWorld* W = GetWorld();
		if (!W) return Count;

		const int32 MyTeam = ResolveOwningTeamId();

		// Counted live from AllUnits because FGameStateData only carries KeyTag counts - and KeyTags are
		// useless for buildings (the control-group logic strips them).
		if (ARTSGameModeBase* GM = Cast<ARTSGameModeBase>(W->GetAuthGameMode()))
		{
			for (AActor* A : GM->AllUnits)
			{
				AUnitBase* U = Cast<AUnitBase>(A);
				if (U && U->TeamId == MyTeam && U->UnitTags.HasTagExact(Tag))
				{
					++Count;
				}
			}
		}

		// Finished buildings alone UNDERCOUNT for a cap: while a site is still under construction the
		// building actor does not exist yet - measured, 3 MatterForges at ClassTagMaxCount 2 after the
		// cooldown went 90 -> 40 s.
		if (bIncludePendingAreas)
		{
			for (TActorIterator<AWorkArea> ItArea(W); ItArea; ++ItArea)
			{
				AWorkArea* Area = *ItArea;
				if (!IsValid(Area) || Area->TeamId != MyTeam || !Area->BuildingClass) continue;

				const ABuildingBase* BuildingCDO = Area->BuildingClass->GetDefaultObject<ABuildingBase>();
				if (BuildingCDO && BuildingCDO->UnitTags.HasTagExact(Tag))
				{
					++Count;
				}
			}
		}
		return Count;
	};

	if (Row.ClassTagRequirement.IsValid())
	{
		const int32 Count = CountByClassTag(Row.ClassTagRequirement, true);
		if (Count < Row.ClassTagMinCount || Count >= Row.ClassTagMaxCount)
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed class tag '%s': count=%d not in [%d, %d)"),
				*RowLabel, *Row.ClassTagRequirement.ToString(), Count, Row.ClassTagMinCount, Row.ClassTagMaxCount);
			return TEXT("{}");
		}
	}

	// Build order: stay blocked until the predecessor actually STANDS.
	if (Row.RequiredClassTag.IsValid() && Row.RequiredClassTagMinCount > 0)
	{
		const int32 Have = CountByClassTag(Row.RequiredClassTag, false);
		if (Have < Row.RequiredClassTagMinCount)
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' failed prerequisite '%s': %d < %d"),
				*RowLabel, *Row.RequiredClassTag.ToString(), Have, Row.RequiredClassTagMinCount);
			return TEXT("{}");
		}
	}

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

	return BuildCompositeActionJSON(ActionIndices, Inference, Row.AbilityArrayIndex, Row.bAimAtTransporter);
}

float URTSRuleBasedDeciderComponent::GetSupplyHeadroom() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return -1.f;
	}

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(World->GetAuthGameMode());
	const int32 TeamId = ResolveOwningTeamId();
	if (!ResourceGameMode || TeamId < 0)
	{
		return -1.f;
	}

	// The tightest of the supply-like resources decides: one of them being full is enough to block
	// production, so the smallest remaining headroom is the number that matters.
	static const EResourceType AllTypes[] = {
		EResourceType::Primary, EResourceType::Secondary, EResourceType::Tertiary,
		EResourceType::Rare, EResourceType::Epic, EResourceType::Legendary };

	float Tightest = -1.f;
	for (const EResourceType Type : AllTypes)
	{
		if (!ResourceGameMode->IsSupplyLikeResource(Type))
		{
			continue;
		}
		// Note the argument order differs between the two: GetMaxResource(Type, Team), GetResource(Team, Type).
		const float Headroom = ResourceGameMode->GetMaxResource(Type, TeamId) - ResourceGameMode->GetResource(TeamId, Type);
		if (Tightest < 0.f || Headroom < Tightest)
		{
			Tightest = Headroom;
		}
	}

	return Tightest;
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

		if (IsRuleOnCooldown(*Row, Name))
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Row '%s' on cooldown (%.0fs)."), *Name.ToString(), Row->MinSecondsBetweenActivations);
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
		// Supply before everything, including expansion: at the cap the team cannot train a single unit,
		// so it fields no army, so no attack rule can ever match and there is never a fight.
		if (ForceSupplyBelowHeadroom > 0 && SupplyRuleNames.Num() > 0)
		{
			const UWorld* SupplyWorld = GetWorld();
			const float Now = SupplyWorld ? SupplyWorld->GetTimeSeconds() : 0.f;
			const bool bIntervalElapsed = (Now - LastSupplyForceTimeSeconds) >= SupplyForceIntervalSeconds;

			if (bIntervalElapsed)
			{
				const float Headroom = GetSupplyHeadroom();
				if (Headroom >= 0.f && Headroom < (float)ForceSupplyBelowHeadroom)
				{
					for (const FMatchingRule& Match : MatchingRules)
					{
						if (SupplyRuleNames.Contains(Match.Name))
						{
							LastSupplyForceTimeSeconds = Now;
							MarkRuleFired(Match.Name);
							UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: supply rule '%s' forced (headroom %.0f)."),
							       *Match.Name.ToString(), Headroom);
							return Match.Output;
						}
					}
				}
			}
		}

		// Expansion next, on its own clock. Everything else still goes through the draw below; this only
		// guarantees that when a base is due AND affordable, it is not out-voted by twenty cheap rules.
		if (ExpansionIntervalSeconds > 0.f && ExpansionRuleNames.Num() > 0)
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			if (Now - LastExpansionFireTimeSeconds >= ExpansionIntervalSeconds)
			{
				for (const FMatchingRule& Match : MatchingRules)
				{
					if (ExpansionRuleNames.Contains(Match.Name))
					{
						LastExpansionFireTimeSeconds = Now;
						MarkRuleFired(Match.Name);
						UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: expansion rule '%s' forced by cadence at %.0fs."),
						       *Match.Name.ToString(), Now);
						return Match.Output;
					}
				}
			}
		}

		if (IsDeterministicRuleSelection())
		{
			const FMatchingRule* Best = &MatchingRules[0];
			for (const FMatchingRule& Match : MatchingRules)
			{
				// Strictly greater, so a tie keeps the earlier row. Without that tiebreak the "deterministic"
				// mode would still wobble between equally weighted rules and defeat its own purpose.
				if (Match.Frequency > Best->Frequency)
				{
					Best = &Match;
				}
			}

			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: T%d DataTable rule '%s' fired (deterministic, freq=%.1f)."), ResolveOwningTeamId(), *Best->Name.ToString(), Best->Frequency);
			MarkRuleFired(Best->Name);
			return Best->Output;
		}

		if (TotalFrequency > 0.0f)
		{
			float RandomValue = FMath::FRandRange(0.0f, TotalFrequency);
			float CumulativeFrequency = 0.0f;
			for (const FMatchingRule& Match : MatchingRules)
			{
				CumulativeFrequency += Match.Frequency;
				if (RandomValue <= CumulativeFrequency)
				{
					if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: T%d DataTable rule '%s' fired (weighted random, freq=%.1f/%.1f)."), ResolveOwningTeamId(), *Match.Name.ToString(), Match.Frequency, TotalFrequency);
					MarkRuleFired(Match.Name);
					return Match.Output;
				}
			}
		}
		
		// Fallback to first matching if total frequency is 0
		if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: T%d DataTable rule '%s' fired (fallback to first, total frequency was 0)."), ResolveOwningTeamId(), *MatchingRules[0].Name.ToString());
		MarkRuleFired(MatchingRules[0].Name);
		return MatchingRules[0].Output;
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: No DataTable rule matched."));
	return TEXT("{}");
}

FGameplayTag URTSRuleBasedDeciderComponent::KeyTagForUnitTag(ERTSUnitTag Tag)
{
	const TCHAR* Name = nullptr;
	switch (Tag)
	{
	case ERTSUnitTag::Alt1:  Name = TEXT("KeyTag.Alt1");  break;
	case ERTSUnitTag::Alt2:  Name = TEXT("KeyTag.Alt2");  break;
	case ERTSUnitTag::Alt3:  Name = TEXT("KeyTag.Alt3");  break;
	case ERTSUnitTag::Alt4:  Name = TEXT("KeyTag.Alt4");  break;
	case ERTSUnitTag::Alt5:  Name = TEXT("KeyTag.Alt5");  break;
	case ERTSUnitTag::Alt6:  Name = TEXT("KeyTag.Alt6");  break;
	case ERTSUnitTag::Ctrl1: Name = TEXT("KeyTag.Ctrl1"); break;
	case ERTSUnitTag::Ctrl2: Name = TEXT("KeyTag.Ctrl2"); break;
	case ERTSUnitTag::Ctrl3: Name = TEXT("KeyTag.Ctrl3"); break;
	case ERTSUnitTag::Ctrl4: Name = TEXT("KeyTag.Ctrl4"); break;
	case ERTSUnitTag::Ctrl5: Name = TEXT("KeyTag.Ctrl5"); break;
	case ERTSUnitTag::Ctrl6: Name = TEXT("KeyTag.Ctrl6"); break;
	case ERTSUnitTag::CtrlQ: Name = TEXT("KeyTag.CtrlQ"); break;
	case ERTSUnitTag::CtrlW: Name = TEXT("KeyTag.CtrlW"); break;
	case ERTSUnitTag::CtrlE: Name = TEXT("KeyTag.CtrlE"); break;
	case ERTSUnitTag::CtrlR: Name = TEXT("KeyTag.CtrlR"); break;
	default: return FGameplayTag();
	}

	return FGameplayTag::RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
}

bool URTSRuleBasedDeciderComponent::IssueDirectAttackMove(const TArray<ERTSUnitTag>& Tags,
	const FVector& Target, const FString& RowLabel, int32 LogTeamId)
{
	UWorld* World = GetWorld();
	if (!World || Tags.Num() == 0)
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ACustomControllerBase* Controller = OwnerPawn ? Cast<ACustomControllerBase>(OwnerPawn->GetController()) : nullptr;
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(World->GetAuthGameMode());
	if (!Controller || !GameMode)
	{
		return false;
	}

	FGameplayTagContainer Wanted;
	for (const ERTSUnitTag Tag : Tags)
	{
		const FGameplayTag Key = KeyTagForUnitTag(Tag);
		if (Key.IsValid())
		{
			Wanted.AddTag(Key);
		}
	}
	if (Wanted.IsEmpty())
	{
		return false;
	}

	TArray<AUnitBase*> Units;
	Units.Reserve(32);

	// Counted separately on purpose. A key tag shared between a producer and its products is the
	// known trap here: the rule qualifies on a tag count that is really a count of BUILDINGS, and
	// then nothing marches. Without these two numbers the empty result looks like a broken gather.
	int32 GebaeudeMitTag = 0;
	int32 ToteMitTag = 0;
	// Arbeiter werden hier NICHT ausgeschlossen - teilt sich ein Arbeiter den Schluesseltag mit
	// Kampfeinheiten, marschiert er mit. Gezaehlt, um genau das zu belegen statt zu vermuten.
	int32 ArbeiterMitTag = 0;

	for (AActor* Actor : GameMode->AllUnits)
	{
		AUnitBase* Unit = Cast<AUnitBase>(Actor);
		if (!Unit || Unit->TeamId != LogTeamId)
		{
			continue;
		}
		if (!Unit->UnitTags.HasAny(Wanted))
		{
			continue;
		}
		if (Unit->GetUnitState() == UnitData::Dead)
		{
			++ToteMitTag;
			continue;
		}
		// A building carries the same key tag as the units it produces, and marching a factory into
		// the enemy base is not the intent.
		if (Cast<ABuildingBase>(Unit))
		{
			++GebaeudeMitTag;
			continue;
		}
		if (!Unit->CanBeSelected)
		{
			continue;
		}
		if (Unit->IsWorker)
		{
			++ArbeiterMitTag;
		}
		Units.Add(Unit);
	}

	if (Units.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AttackOrder] Team=%d Regel='%s' KEINE beweglichen Traeger: Gebaeude=%d Tote=%d Tags=%s"),
			LogTeamId, *RowLabel, GebaeudeMitTag, ToteMitTag, *Wanted.ToStringSimple());
		return false;
	}

	// Arrival formation. Sending everyone to the identical point makes them shove each other onto
	// whatever geometry is nearby, and a unit standing on a ledge fails every path request after.
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt((float)Units.Num())));
	TArray<FVector> Targets;
	TArray<float> Speeds;
	TArray<float> Radii;
	Targets.Reserve(Units.Num());
	Speeds.Reserve(Units.Num());
	Radii.Reserve(Units.Num());

	for (int32 i = 0; i < Units.Num(); ++i)
	{
		const int32 Col = i % Columns;
		const int32 Row = i / Columns;
		const float OffsetX = (Col - (Columns - 1) * 0.5f) * AttackFormationSpacing;
		const float OffsetY = (Row - (Units.Num() / Columns) * 0.5f) * AttackFormationSpacing;
		Targets.Add(Target + FVector(OffsetX, OffsetY, 0.f));

		float Speed = 300.f;
		if (Units[i]->Attributes)
		{
			Speed = Units[i]->Attributes->GetBaseRunSpeed();
		}
		Speeds.Add(Speed);
		Radii.Add(Units[i]->MovementAcceptanceRadius);
	}

	// Slots off the navmesh are the other way a unit ends up running on the spot: it is given a
	// destination it can never reach and re-plans every tick.
	Targets = Controller->AdjustBatchTargetsForNav(Units, Targets);

	Controller->Server_Batch_CorrectSetUnitMoveTargets(World, Units, Targets, Speeds, Radii,
		/*AttackT*/ true, /*bResetHoldPosition*/ true, /*bResetFollowTarget*/ true,
		/*bOriginatorPredictsLocally*/ false);

	UE_LOG(LogTemp, Warning,
		TEXT("[AttackOrder] Team=%d Regel='%s' DIREKT Ziel=(%.0f, %.0f) Einheiten=%d davonArbeiter=%d Spalten=%d"),
		LogTeamId, *RowLabel, Target.X, Target.Y, Units.Num(), ArbeiterMitTag, Columns);

	return true;
}

bool URTSRuleBasedDeciderComponent::ExecuteAttackRuleRow(const FRTSAttackRuleRow& Row, int32 TableRowIndex, const FGameStateData& GS, UInferenceComponent* Inference)
{
	const FString RowLabel = Row.RuleName.IsNone() ? TEXT("<UnnamedAttack>") : Row.RuleName.ToString();
	// Both teams evaluate attack rows in the same frames, and the row names are identical in both
	// tables - without the team id the AttackSel/AttackRow lines cannot be attributed to a faction.
	// Cost me a wrong conclusion once: I read team 2's 'Harass' selection as the Xeno's, although
	// the Xeno 'Harass' row is disabled.
	const int32 LogTeamId = ResolveOwningTeamId();
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

	// Same set as Indices, but as unit tags: the direct batch path needs to know WHICH groups
	// qualified, not which key presses would have selected them.
	TArray<ERTSUnitTag> QualifyingTags;
	QualifyingTags.Reserve(8);

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
		const bool bCapQualifies = (Count > 0 && Count >= Cap.MinCount && Count < Cap.MaxCount);

		// Vormessung: dieselbe MinCount entscheidet hier nicht nur, OB angegriffen wird
		// (das tut die Cap-Logik weiter oben), sondern auch WER mitkommt. Eine Gruppe
		// unter ihrer eigenen Schwelle wird NIE zu einem Angriff gerufen und bleibt
		// dauerhaft zu Hause. Ohne diese Zahlen ist nach einer Aenderung nicht belegbar,
		// wie viele Einheiten davon betroffen waren. Team ueber die AttackPosition der
		// unmittelbar folgenden 'executing'-Zeile zuordnen.
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: [Team %d] AttackSel '%s' Tag=%d Count=%d (Min=%d Max=%d) -> %s"),
				LogTeamId, *RowLabel, (int32)Cap.Tag, Count, Cap.MinCount, Cap.MaxCount,
				bCapQualifies ? TEXT("MARSCHIERT") : TEXT("bleibt zuhause"));
		}

		if (bCapQualifies)
		{
			QualifyingTags.AddUnique(Cap.Tag);

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
		
		FVector AdjustedLoc = FVector(TargetXYOnly.X, TargetXYOnly.Y, OutZ);
		
		// NavMesh correction as requested: land on the NavMesh
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
		if (NavSys)
		{
			FNavLocation NavLoc;
			// 1. Try with the provided extent
			if (NavSys->ProjectPointToNavigation(AdjustedLoc, NavLoc, NavMeshProjectionExtent))
			{
				AdjustedLoc = NavLoc.Location;
			}
			// 2. Wide projection fallback if the provided extent fails (mirroring CustomControllerBase logic)
			else if (NavSys->ProjectPointToNavigation(AdjustedLoc, NavLoc, FVector(1500.f, 1500.f, 1500.f)))
			{
				AdjustedLoc = NavLoc.Location;
			}
		}
		
		// NavMesh projection snaps to the NEAREST navigable point, which is very often a cliff edge -
		// units sent there bunch up on the rim and stop moving. Pull the target inwards, towards the
		// enemy's centre of mass, and keep the pulled point only if it is navigable as well. A point
		// that survives that test is by construction away from the boundary it came from.
		if (AttackWaypointInwardPull > 0.f)
		{
			if (UNavigationSystemV1* NavSys2 = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
			{
				const FVector Inward = (GS.AverageEnemyPosition - AdjustedLoc).GetSafeNormal2D();
				if (!Inward.IsNearlyZero())
				{
					FNavLocation PulledNav;
					if (NavSys2->ProjectPointToNavigation(AdjustedLoc + Inward * AttackWaypointInwardPull,
					                                      PulledNav, NavMeshProjectionExtent))
					{
						AdjustedLoc = PulledNav.Location;
					}
				}
			}
		}

		return AdjustedLoc;
	};
	FVector AdjustedAttackLoc = ComputeGroundAdjusted(DesiredAttackPos);

	// Commit window: an army that is still walking keeps the destination it was given. Without this
	// the rules re-aim the same units every time they fire and the march never completes.
	if (AttackCommitSeconds > 0.f && LetzteAngriffsBefehlZeit >= 0.f)
	{
		if (const UWorld* CommitWorld = GetWorld())
		{
			const float SeitLetztem = CommitWorld->GetTimeSeconds() - LetzteAngriffsBefehlZeit;
			const float Sprung = FVector::Dist2D(LetzteAngriffsBefehlPos, AdjustedAttackLoc);

			if (SeitLetztem < AttackCommitSeconds && Sprung > AttackRetargetMinDistance)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[AttackOrder] Team=%d Regel='%s' GEBUNDEN: Sprung=%.0f nach %.1fs -> behalte Ziel=(%.0f, %.0f)"),
					LogTeamId, *RowLabel, Sprung, SeitLetztem,
					LetzteAngriffsBefehlPos.X, LetzteAngriffsBefehlPos.Y);

				AdjustedAttackLoc = LetzteAngriffsBefehlPos;
			}
		}
	}

	// Direct route: hand the target to the units in one batched order. Nothing is teleported, so
	// there is no agent position for a later rule to contradict, and no return timer to wait out.
	if (bUseDirectBatchAttackMove && IssueDirectAttackMove(QualifyingTags, AdjustedAttackLoc, RowLabel, LogTeamId))
	{
		// The [AttackOrder] diagnosis stays on: it is what made the jump-and-turn pattern visible
		// in the first place, and it is the only way to tell the two paths apart in a log.
		if (UWorld* DiagWorld = GetWorld())
		{
			LetzteAngriffsBefehlPos = AdjustedAttackLoc;
			LetzteAngriffsBefehlZeit = DiagWorld->GetTimeSeconds();
		}
		return true;
	}

	RLAgent->SetActorLocation(AdjustedAttackLoc);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Adjusted attack move to (%.1f, %.1f, %.1f) from desired (%.1f, %.1f, %.1f)"),
		AdjustedAttackLoc.X, AdjustedAttackLoc.Y, AdjustedAttackLoc.Z,
		DesiredAttackPos.X, DesiredAttackPos.Y, DesiredAttackPos.Z);

	const FString Json = BuildCompositeActionJSON(Indices, Inference);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: [Team %d] AttackRow '%s' executing %d actions at AttackPosition (%.1f, %.1f, %.1f)."), LogTeamId, *RowLabel, Indices.Num(), DesiredAttackPos.X, DesiredAttackPos.Y, DesiredAttackPos.Z);

	// REINE DIAGNOSE, kein Eingriff - Nutzerpunkt 6.
	//
	// Die Einheiten werden an den Punkt geschickt, an dem der RLAgent im Befehlsmoment steht
	// (AdjustedAttackLoc). Bekommen dieselben Einheiten kurz hintereinander Befehle zu weit
	// auseinanderliegenden Punkten, drehen sie unterwegs um - genau das Bild "laeuft hin und
	// her statt anzugreifen". Diese Zeile misst beides: den Abstand zum VORIGEN Befehl und die
	// Zeit dazwischen. Erst danach wird entschieden.
	//
	// Anmerkung zur urspruenglichen Vermutung: AttackReturnDelaySeconds /
	// bAttackReturnBlockActive / AttackReturnLocation betreffen NUR den RLAgent (die
	// KI-Kamera), die per SetActorLocation versetzt und danach zurueckgeholt wird. Sie
	// bewegen keine Kampfeinheit und scheiden als Ursache aus.
	if (UWorld* DiagWorld = GetWorld())
	{
		const float Jetzt = DiagWorld->GetTimeSeconds();
		const float Sprung = (LetzteAngriffsBefehlZeit >= 0.f)
			? FVector::Dist2D(LetzteAngriffsBefehlPos, AdjustedAttackLoc) : -1.f;
		const float Abstand = (LetzteAngriffsBefehlZeit >= 0.f)
			? (Jetzt - LetzteAngriffsBefehlZeit) : -1.f;

		UE_LOG(LogTemp, Warning,
			TEXT("[AttackOrder] Team=%d Regel='%s' Ziel=(%.0f, %.0f) Sprung=%.0f SeitLetztem=%.1fs Einheitengruppen=%d"),
			LogTeamId, *RowLabel, AdjustedAttackLoc.X, AdjustedAttackLoc.Y,
			Sprung, Abstand, Indices.Num());

		LetzteAngriffsBefehlPos = AdjustedAttackLoc;
		LetzteAngriffsBefehlZeit = Jetzt;
	}

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

		// Game time check
		if (UWorld* World = GetWorld())
		{
			float CurrentTime = World->GetTimeSeconds();
			if (CurrentTime < Row->GameTimeCap.Min || CurrentTime > Row->GameTimeCap.Max)
			{
				if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: AttackRow '%s' failed GameTime check: %.2f not in [%.2f, %.2f]"), *Name.ToString(), CurrentTime, Row->GameTimeCap.Min, Row->GameTimeCap.Max);
				continue;
			}
		}

		// An attack order needs time to be carried out. Re-issuing it every tick - worse, from two
		// rules pointing at different targets - keeps the army oscillating on the spot.
		if (IsAttackRuleOnCooldown(*Row, Name))
		{
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: AttackRow '%s' on cooldown (%.0fs)."), *Name.ToString(), Row->MinSecondsBetweenActivations);
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
		if (IsDeterministicRuleSelection())
		{
			// Highest frequency, ties to the earlier row - same reasoning as the build rules.
			SelectedMatch = &MatchingRules[0];
			for (const FMatchingAttackRule& Match : MatchingRules)
			{
				if (Match.Frequency > SelectedMatch->Frequency)
				{
					SelectedMatch = &Match;
				}
			}
		}
		else if (TotalFrequency > 0.0f)
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
			MarkAttackRuleFired(SelectedMatch->Name);
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: [Team %d] Attack rule '%s' fired (weighted random, freq=%.1f/%.1f)."), ResolveOwningTeamId(), *SelectedMatch->Name.ToString(), SelectedMatch->Frequency, TotalFrequency);
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

	int32 MyTeamId = -1;
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn)
	{
		if (ARTSBTController* BTC = Cast<ARTSBTController>(OwnerPawn->GetController()))
		{
			MyTeamId = BTC->OrchestratorTeamId;
		}
		else if (AControllerBase* CB = Cast<AControllerBase>(OwnerPawn->GetController()))
		{
			MyTeamId = CB->SelectableTeamId;
		}
	}

	// Bezugspunkt fuer bAttackNearestTarget. Deklaration bewusst AUSSERHALB des
	// UniqueClasses-Blocks: gelesen wird sie erst in der Zeilenschleife weiter unten.
	FVector EigenerSchwerpunkt = FVector::ZeroVector;
	int32 EigeneGebaeudeZahl = 0;

	if (UniqueClasses.Num() > 0)
	{

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			// Team filtering: skip units on our own team
			if (MyTeamId != -1)
			{
				if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
				{
					if (Unit->TeamId == MyTeamId)
					{
						continue;
					}
				}
			}

			// Schwerpunkt der EIGENEN Gebaeude als Bezugspunkt fuer die Zielwahl mitfuehren
			// (siehe bAttackNearestTarget). Kostet nichts extra - die Schleife laeuft ohnehin
			// ueber alle Aktoren.
			if (MyTeamId != -1)
			{
				if (const ABuildingBase* EigenesGebaeude = Cast<ABuildingBase>(Actor))
				{
					if (EigenesGebaeude->TeamId == MyTeamId)
					{
						EigenerSchwerpunkt += Actor->GetActorLocation();
						++EigeneGebaeudeZahl;
					}
				}
			}

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

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	for (int32 i = 0; i < RowNames.Num(); ++i)
	{
		const FRTSAttackRuleRow* Row = Rows[i];
		FVector ChosenPos = FVector::ZeroVector;
		int32 DiagFoundCount = 0;

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

			DiagFoundCount = PossibleLocations.Num();

			if (PossibleLocations.Num() > 0)
			{
				if (bAttackNearestTarget)
				{
					// Naechstgelegenes Ziel zum eigenen Schwerpunkt. Das ersetzt die Zufallswahl und
					// beantwortet zugleich Angriffe auf die eigene Basis: was uns angreift, ist per
					// Definition das naechste Ziel.
					// Bezugspunkt: Schwerpunkt der eigenen Gebaeude. Ohne eigene Gebaeude faellt es auf
					// den Traeger der Komponente zurueck (RLAgent gibt es in dieser Funktion nicht).
					const FVector Bezug = (EigeneGebaeudeZahl > 0)
						? (EigenerSchwerpunkt / (float)EigeneGebaeudeZahl)
						: (OwnerPawn ? OwnerPawn->GetActorLocation() : FVector::ZeroVector);

					double BesteDistSq = TNumericLimits<double>::Max();
					for (const FVector& Kandidat : PossibleLocations)
					{
						const double DistSq = FVector::DistSquared2D(Kandidat, Bezug);
						if (DistSq < BesteDistSq)
						{
							BesteDistSq = DistSq;
							ChosenPos = Kandidat;
						}
					}
				}
				else
				{
					const int32 RandIdx = FMath::RandRange(0, PossibleLocations.Num() - 1);
					ChosenPos = PossibleLocations[RandIdx];
				}
			}
			else
			{
				// Fallback to row position if no actors found
				ChosenPos = Row->AttackPosition;
			}
		}
		else if (Row)
		{
			// No classes specified for this row, use its default position
			ChosenPos = Row->AttackPosition;
		}

		// Diagnose: ohne diese Zeile ist nicht unterscheidbar, OB die Zielsuche leer ausgeht
		// (dann greift der Rueckfall auf die leere Row->AttackPosition und die Armee marschiert
		// zum Weltursprung) ODER ob ein gefundener Punkt erst durch die Boden-/NavMesh-Korrektur
		// weiter unten zerstoert wird. Das Log nennt ausserdem endlich das Team - beide
		// Fraktionen haben Zeilen namens Assault/AllIn, die Zuordnung war bisher geraten.
		if (bDebug)
		{
			UE_LOG(LogTemp, Log,
				TEXT("RuleBasedDecider: [Team %d] AttackPos row '%s': %d Ziele gefunden, gewaehlt (%.1f, %.1f, %.1f)%s"),
				MyTeamId, *RowNames[i].ToString(), DiagFoundCount,
				ChosenPos.X, ChosenPos.Y, ChosenPos.Z,
				DiagFoundCount == 0 ? TEXT(" <-- RUECKFALL AUF ROW-POSITION") : TEXT(""));
		}

		// Apply LineTrace to ground and NavMesh correction for the chosen position
		FVector FinalPos = ChosenPos;
		
		// 1. LineTrace to ground
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PopulateAttackPosGroundTrace), false);
		if (OwnerPawn) Params.AddIgnoredActor(OwnerPawn);

		// Trace from high up to find ground
		const FVector TraceStart(ChosenPos.X, ChosenPos.Y, ChosenPos.Z + 10000.f);
		const FVector TraceEnd(ChosenPos.X, ChosenPos.Y, ChosenPos.Z - 10000.f);
		
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			FinalPos.Z = Hit.ImpactPoint.Z;
		}

		// 2. NavMesh correction. An attack order to a point the navmesh does not cover leaves the units
		// walking into it and stopping, which is what "they get stuck" looks like. Widen the search
		// instead of giving up after two tries, and keep the previous valid position rather than
		// publishing an unreachable one.
		bool bProjected = false;
		if (NavSys)
		{
			static const FVector Extents[] = {
				FVector(50.f, 50.f, 250.f), FVector(500.f, 500.f, 500.f),
				FVector(1500.f, 1500.f, 1500.f), FVector(4000.f, 4000.f, 2000.f) };

			FNavLocation NavLoc;
			if (NavSys->ProjectPointToNavigation(FinalPos, NavLoc, NavMeshProjectionExtent))
			{
				FinalPos = NavLoc.Location;
				bProjected = true;
			}
			else
			{
				for (const FVector& Extent : Extents)
				{
					if (NavSys->ProjectPointToNavigation(FinalPos, NavLoc, Extent))
					{
						FinalPos = NavLoc.Location;
						bProjected = true;
						break;
					}
				}
			}
		}

		if (!bProjected)
		{
			// Nothing reachable anywhere near the target. Keep whatever we published last time - a stale
			// but reachable target beats a fresh unreachable one - and say so, because silently handing
			// out an off-navmesh position is exactly how the units ended up standing still.
			UE_LOG(LogTemp, Warning,
			       TEXT("RuleBasedDecider: attack position for row %d could not be projected onto the NavMesh (%s). Keeping the previous target."),
			       i, *FinalPos.ToCompactString());
			if (AttackPositions.IsValidIndex(i) && !AttackPositions[i].IsNearlyZero())
			{
				continue;
			}
		}

		AttackPositions[i] = FinalPos;
	}

	if (bDebug) UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: Refreshed AttackPositions for %d rows. Unique target classes searched: %d"), RowNames.Num(), UniqueClasses.Num());

	// 4. Setup next refresh if needed
	if (bAnyRowHasClasses && AttackPositionRefreshInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(AttackPositionRefreshTimerHandle, this, &URTSRuleBasedDeciderComponent::PopulateAttackPositions, AttackPositionRefreshInterval, false);
	}
}

void URTSRuleBasedDeciderComponent::EvaluateDefence()
{
	UWorld* World = GetWorld();
	if (!bEnableDefence || !World)
	{
		return;
	}

	// Workers pulled into a defence are only ever told to RUN at the threat - there is no way back.
	// Once the fight is over they stand around at the rally point and the team's income silently
	// stops. Runs every defence tick, before any new threat is evaluated: anything that is idle,
	// owns no resource place and is not busy building goes back to work.
	if (AResourceGameMode* RGM = Cast<AResourceGameMode>(World->GetAuthGameMode()))
	{
		const int32 SweepTeam = ResolveOwningTeamId();
		if (ARTSGameModeBase* GM = Cast<ARTSGameModeBase>(World->GetAuthGameMode()))
		{
			for (AActor* A : GM->AllUnits)
			{
				AUnitBase* W = Cast<AUnitBase>(A);
				if (!W || !W->IsWorker || W->TeamId != SweepTeam) continue;
				// A worker sitting in a transporter matches every condition below - it is Idle, owns no
				// resource place and is not building - so this sweep marched the reactor crew back out
				// while they were still hidden. Cargo is not idle.
				if (W->IsInsideTransport) continue;
				if (W->GetUnitState() != UnitData::Idle && W->GetUnitState() != UnitData::Run) continue;
				if (W->ResourcePlace || W->BuildArea || W->CurrentDraggedWorkArea) continue;

				W->SetUEPathfinding = true;
				W->SetUnitState(UnitData::GoToResourceExtraction);
				W->SwitchEntityTagByState(UnitData::GoToResourceExtraction, W->UnitStatePlaceholder);
			}
		}
	}

	const int32 MyTeamId = ResolveOwningTeamId();
	if (MyTeamId < 0)
	{
		return;
	}

	// 1. Where is our base being hit? Take the enemy that is closest to any of our buildings.
	TArray<FVector> OwnBuildings;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		ABuildingBase* Building = *It;
		if (IsValid(Building) && Building->TeamId == MyTeamId && Building->GetUnitState() != UnitData::Dead)
		{
			OwnBuildings.Add(Building->GetActorLocation());
		}
	}
	if (OwnBuildings.Num() == 0)
	{
		return;
	}

	const float TriggerSq = DefenceTriggerRadius * DefenceTriggerRadius;
	FVector ThreatLocation = FVector::ZeroVector;
	double BestDistSq = TNumericLimits<double>::Max();

	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId == MyTeamId || Unit->TeamId <= 0) continue;
		if (Unit->GetUnitState() == UnitData::Dead || Unit->bIsBuilding) continue;

		const FVector Loc = Unit->GetActorLocation();
		for (const FVector& Building : OwnBuildings)
		{
			const double DistSq = FVector::DistSquared2D(Loc, Building);
			if (DistSq <= TriggerSq && DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				ThreatLocation = Loc;
			}
		}
	}

	if (BestDistSq == TNumericLimits<double>::Max())
	{
		return; // nobody in our base
	}

	// 2. Everything that can fight goes there. Units already fighting are left alone - re-issuing a move
	// every few seconds would pull them out of combat over and over.
	TArray<AUnitBase*> Fighters;
	TArray<AUnitBase*> Workers;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != MyTeamId) continue;
		if (Unit->bIsBuilding || Unit->bIsConstructionUnit) continue;

		const TEnumAsByte<UnitData::EState> State = Unit->GetUnitState();
		if (State == UnitData::Dead || State == UnitData::Attack || State == UnitData::Chase ||
			State == UnitData::Casting || State == UnitData::Build)
		{
			continue;
		}

		if (Unit->IsWorker)
		{
			Workers.Add(Unit);
		}
		else
		{
			Fighters.Add(Unit);
		}
	}

	TArray<AUnitBase*>& Defenders = Fighters;
	if (Fighters.Num() == 0)
	{
		if (!bDefendWithWorkersAsLastResort)
		{
			return;
		}
		// Keep the economy alive: only the surplus above MinWorkersKeptMining picks up the fight.
		const int32 Spare = Workers.Num() - MinWorkersKeptMining;
		if (Spare <= 0)
		{
			return;
		}
		Workers.SetNum(Spare);
		Defenders = Workers;
	}

	if (Defenders.Num() == 0)
	{
		return;
	}

	// 3. The rally point has to be reachable, or they walk at it and stop - the same trap the attack
	// positions had.
	FVector Target = ThreatLocation;
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
	{
		FNavLocation NavLoc;
		if (NavSys->ProjectPointToNavigation(Target, NavLoc, FVector(500.f, 500.f, 500.f)) ||
			NavSys->ProjectPointToNavigation(Target, NavLoc, FVector(2000.f, 2000.f, 1000.f)))
		{
			Target = NavLoc.Location;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RuleBasedDecider: defence target %s is off the NavMesh, skipping."), *Target.ToCompactString());
			return;
		}
	}

	AControllerBase* Controller = nullptr;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		Controller = Cast<AControllerBase>(OwnerPawn->GetController());
	}

	for (AUnitBase* Defender : Defenders)
	{
		Defender->SetUEPathfinding = true;
		Defender->RunLocation = Target;
		if (Controller)
		{
			Controller->MoveToLocationUEPathFinding(Defender, Target);
		}
		Defender->SetUnitState(UnitData::Run);
		Defender->SwitchEntityTagByState(UnitData::Run, Defender->UnitStatePlaceholder);
		// Let them acquire targets on the way instead of running past the enemy.
		Defender->SetToggleUnitDetection(true);
	}

	UE_LOG(LogTemp, Log, TEXT("RuleBasedDecider: team %d defending - sent %d %s to %s."),
	       MyTeamId, Defenders.Num(), Fighters.Num() > 0 ? TEXT("fighters") : TEXT("workers"),
	       *Target.ToCompactString());
}

FString URTSRuleBasedDeciderComponent::ChooseJsonActionRuleBased(const FGameStateData& GameState)
{
	// No-op once resolved; covers the case where possession was still pending on the first tick.
	ApplyTeamTableOverrides();

	// Defence runs before anything else and outside the rule system: a raid has to be answered even
	// while the build rules are on cooldown or an attack-return block is active.
	if (UWorld* DefenceWorld = GetWorld())
	{
		const float Now = DefenceWorld->GetTimeSeconds();
		if (Now - LastDefenceCheckTimeSeconds >= DefenceCheckIntervalSeconds)
		{
			LastDefenceCheckTimeSeconds = Now;
			EvaluateDefence();
		}
	}

	// Every path below funnels through BuildCompositeActionJSON, which records the decision. It needs the
	// state that caused it, and only this entry point has it.
	CachedRecordingState = GameState;
	bHasCachedRecordingState = true;

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

	// Bestandsmeldung - reine Diagnose, greift in nichts ein.
	if (BestandsmeldungIntervallSekunden > 0.f)
	{
		if (const UWorld* BestandWorld = GetWorld())
		{
			const float Jetzt = BestandWorld->GetTimeSeconds();
			if (LetzteBestandsmeldung < 0.f || Jetzt - LetzteBestandsmeldung >= BestandsmeldungIntervallSekunden)
			{
				LetzteBestandsmeldung = Jetzt;

				const int32 Kampf = GameState.Ctrl1TagFriendlyUnitCount
					+ GameState.Ctrl2TagFriendlyUnitCount + GameState.Ctrl3TagFriendlyUnitCount;

				// Die TATSAECHLICHE Zahl der Gebaeude und Einheiten, unabhaengig von jedem Tag.
				// Grund: Ctrl5 faellt bei den Xeno von 9 auf 0, und zwar Minuten BEVOR der Gegner
				// den ersten Angriffsbefehl gibt. Entweder verschwinden die Gebaeude wirklich - oder
				// nur ihr KeyTag, und dann ist die ganze Kette ein Tagging-Fehler. Diese beiden
				// Zahlen nebeneinander entscheiden das in einer einzigen Partie.
				int32 EchteGebaeude = 0;
				int32 EchteEinheiten = 0;
				if (const ARTSGameModeBase* BestandGM = Cast<ARTSGameModeBase>(BestandWorld->GetAuthGameMode()))
				{
					const int32 MeinTeam = ResolveOwningTeamId();
					for (AActor* Actor : BestandGM->AllUnits)
					{
						const AUnitBase* U = Cast<AUnitBase>(Actor);
						if (!U || U->TeamId != MeinTeam || U->GetUnitState() == UnitData::Dead)
						{
							continue;
						}
						if (Cast<ABuildingBase>(U)) { ++EchteGebaeude; } else { ++EchteEinheiten; }
					}
				}

				UE_LOG(LogTemp, Warning,
					TEXT("[Bestand] t=%.0f Team=%d Ctrl1=%d Ctrl2=%d Ctrl3=%d (Kampf=%d) Ctrl5=%d CtrlQ=%d CtrlW=%d | ECHT Gebaeude=%d Einheiten=%d"),
					Jetzt, ResolveOwningTeamId(),
					GameState.Ctrl1TagFriendlyUnitCount, GameState.Ctrl2TagFriendlyUnitCount,
					GameState.Ctrl3TagFriendlyUnitCount, Kampf,
					GameState.Ctrl5TagFriendlyUnitCount, GameState.CtrlQTagFriendlyUnitCount,
					GameState.CtrlWTagFriendlyUnitCount, EchteGebaeude, EchteEinheiten);
			}
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
		if (GRTSWanderPath == 0)
		{
			return TEXT("{}");
		}

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
				// Guard the selection step like the ability step below: a None-configured
				// WanderSelectionAction (==255) must not be serialized (that was the per-tick 255 source).
				if (WanderSelectionAction != ERTSAIAction::None)
				{
					Steps.Add((int32)WanderSelectionAction);
				}
				// If a specific ability is provided, use it; otherwise use the movement as the second step
				Steps.Add((WanderAbilityAction != ERTSAIAction::None) ? (int32)WanderAbilityAction : ChosenIdx);
				bLastDecisionWasWander = true;
				return BuildCompositeActionJSON(Steps, Inference);
			}
			bLastDecisionWasWander = true;
			return Inference->GetActionAsJSON(ChosenIdx);
		}
		return TEXT("{}");
	};

	// Alternate which path is attempted first each call (static toggle survives between calls).
	// In recording mode the order is fixed to the rule table: the alternation depends on call count rather
	// than on game state, so the same state would otherwise land in the training set as a build decision
	// one moment and a camera wander the next. (Note the toggle is a function-local static, i.e. shared by
	// every decider in the level - both AI teams flip it for each other.)
	static bool bTryTableFirstNext = true;
	const bool bTryTableFirst = IsDeterministicRuleSelection() ? true : bTryTableFirstNext;
	bTryTableFirstNext = !bTryTableFirstNext;

	bLastDecisionWasWander = false;

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