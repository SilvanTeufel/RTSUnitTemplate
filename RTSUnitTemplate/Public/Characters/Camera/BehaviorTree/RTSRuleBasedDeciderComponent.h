// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "Engine/DataTable.h"
#include "Core/UnitData.h" // for FBuildingCost
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData and UInferenceComponent
#include "RTSRuleBasedDeciderComponent.generated.h"

UENUM(BlueprintType)
enum class ERTSUnitCapLogic : uint8
{
	AndLogic UMETA(DisplayName = "AND (All must match)"),
	OrLogic  UMETA(DisplayName = "OR (At least one must match)")
};

USTRUCT(BlueprintType)
struct FRTSUnitCountCap
{
	GENERATED_BODY()

	// The tag to check (e.g., "Alt1", "CtrlQ")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	ERTSUnitTag Tag = ERTSUnitTag::Alt1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	int32 MaxCount = 999;
};

USTRUCT(BlueprintType)
struct FRTSGameTimeCap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	float Max = 9000.f;
};

USTRUCT(BlueprintType)
struct FRTSRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	// If false, this row is ignored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	bool bEnabled = true;

	// Optional label for readability in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	FName RuleName;

	// Resource thresholds. All must be met or exceeded (>=) to trigger the rule.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	FBuildingCost ResourceThresholds;

	/**
	 * Upper resource bounds - the counterpart to ResourceThresholds, for rules that should only fire while
	 * something is running SHORT (a power plant, an extra depot). A non-zero entry means "only match while
	 * the team has less than this much"; 0 disables the check for that resource.
	 * For supply-like resources the comparison uses the remaining headroom (Max - Current) instead of the
	 * raw amount, so "build a reactor once we are within 4 of the energy cap" is expressible directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	FBuildingCost ResourceMaxThresholds;

	// Caps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 MaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 MinFriendlyUnitCount = 0;

	// How to connect the UnitCaps in this row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	ERTSUnitCapLogic UnitCapLogic = ERTSUnitCapLogic::AndLogic;

	// Per-tag caps for friendly unit counts. If a tag is not in this array, it's not checked (Min=0, Max=999).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	TArray<FRTSUnitCountCap> UnitCaps;

	// Game time constraints (in seconds). Only execute if Min <= GameTime <= Max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FRTSGameTimeCap GameTimeCap;
	
	// The frequency of this rule (0-100). Higher values relative to other matching rules increase the chance of selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule", meta=(ClampMin="0.0", ClampMax="100.0"))
	float Frequency = 100.0f;

	/**
	 * Minimum seconds between two activations of this rule. 0 = no limit.
	 * Resource checks alone cannot prevent a burst: the cost is only deducted once the building
	 * actually starts, so a rich AI fires the same rule several times in a row and puts down three
	 * reactors before the first one exists. Set this to at least the build time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output", meta=(ClampMin="0.0"))
	float MinSecondsBetweenActivations = 0.f;

	/**
	 * Extra requirement on a STABLE classification tag such as "Buildings.Singularian.Bunker".
	 * KeyTags cannot be used for buildings: Server_AssignTagToSelectedUnits (the control-group logic)
	 * strips a KeyTag from every unit of the team that is not in the current selection, so a building's
	 * KeyTag never survives. Classification tags are never touched by it.
	 * Leave the tag unset to disable this check.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FGameplayTag ClassTagRequirement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 ClassTagMinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 ClassTagMaxCount = 999;

	/**
	 * Optional PREREQUISITE on a different classification tag - this is how a build ORDER is expressed.
	 * ClassTagRequirement above is the rule's own cap and is therefore already taken; a chain like
	 * "BioIntegrator, then MatterForge, then CybernaticFactory, then OrbitalUplink" needs a second,
	 * independent tag. The CtrlQ unit caps cannot do it: all four production buildings share that
	 * KeyTag, so a count of two says nothing about WHICH two are standing.
	 *
	 * Unlike the cap, this counts FINISHED buildings only. A prerequisite that already accepts a
	 * construction site would let the successor overtake its predecessor, which is exactly the
	 * ordering the chain is meant to prevent. Leave the tag unset to disable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FGameplayTag RequiredClassTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps", meta=(ClampMin="0"))
	int32 RequiredClassTagMinCount = 0;

	/**
	 * Which ability array the AbilityAction refers to: 0 = DefaultAbilities, 1 = SecondAbilities,
	 * 2 = ThirdAbilities, 3 = FourthAbilities.
	 * The only way to reach a higher array used to be IntermediateAction=ChangeAbilityIndex, which just
	 * does AddAbilityIndex(+1) and is never reset - so the second array was hit only by luck and the
	 * third (WallTower, Bunker, Tesla) could not be reached at all. This sets the index outright.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output", meta=(ClampMin="0", ClampMax="3"))
	int32 AbilityArrayIndex = 0;

	/**
	 * Marks this rule as "load the selection into a transporter". The agent's click traces straight
	 * down from its camera, so a right-click almost never lands on the building - and without knowing
	 * the intent it cannot re-aim, because the very same click is also an ordinary move order for the
	 * workers it constantly has selected. This flag travels in the action JSON and is read right
	 * before the click.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	bool bAimAtTransporter = false;

	// Output actions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction SelectionAction = ERTSAIAction::Ability1;

	// Optional action between selection and ability (e.g., ChangeAbilityIndex). Ignored if None.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction IntermediateAction = ERTSAIAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction AbilityAction = ERTSAIAction::Ability1;
};


USTRUCT(BlueprintType)
struct FRTSAttackRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	// If false, this row is ignored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	bool bEnabled = true;

	// Optional label for readability in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	FName RuleName;

	// How to connect the UnitCaps in this row. 
	// Default to OR to maintain current aggressive behavior, or AND for coordinated strikes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	ERTSUnitCapLogic UnitCapLogic = ERTSUnitCapLogic::OrLogic;

	// Per-tag caps for friendly unit counts. If a tag is not in this array, it's not checked (Min=0, Max=999).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	TArray<FRTSUnitCountCap> UnitCaps;

	// Game time constraints (in seconds). Only execute if Min <= GameTime <= Max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FRTSGameTimeCap GameTimeCap;

	// The frequency of this rule (0-100). Higher values relative to other matching rules increase the chance of selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule", meta=(ClampMin="0.0", ClampMax="100.0"))
	float Frequency = 100.0f;

	/**
	 * Minimum seconds between two activations of this rule. 0 = no limit.
	 * Without this the rule re-issues an attack order every decision tick, and two rules with
	 * different target positions tear the same units back and forth so they never arrive anywhere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule", meta=(ClampMin="0.0"))
	float MinSecondsBetweenActivations = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	FVector AttackPosition = FVector::ZeroVector;

	// If provided, we will search for actors of these classes and pick one randomly as the attack position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	TArray<TSubclassOf<AActor>> AttackPositionSourceClasses;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	bool UseClassAttackPositions = false;
};
/**
 * Easy-to-configure rule-based decider.
 * For each rule you can now output TWO actions in sequence: first a Selection, then an Ability.
 * The function returns a JSON string. When two actions are produced, the string is a JSON array
 * of two action objects: [ {..Selection..}, {..Ability..} ]. Consumers should iterate and execute in order.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API URTSRuleBasedDeciderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSRuleBasedDeciderComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;

protected:
	virtual void BeginPlay() override;

public:
	// Entry point: returns a JSON action string based on simple rules and fallbacks.
	UFUNCTION(BlueprintCallable, Category="AI|Rules")
	FString ChooseJsonActionRuleBased(const FGameStateData& GameState);

public:
	// ---------------- DataTable-based rules (optional) ----------------
	// If true and RulesDataTable is assigned, the component evaluates rows to choose actions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Table")
	bool bUseDataTableRules = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Table", meta=(EditCondition="bUseDataTableRules"))
	UDataTable* RulesDataTable = nullptr;

	// Attack rule DataTable: executes selection + attack at a target position, then returns camera after a delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	bool bUseAttackDataTableRules = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(EditCondition="bUseAttackDataTableRules"))
	UDataTable* AttackRulesDataTable = nullptr;

	// Per-team rule tables. One AI pawn class serves every team, so the tables have to be picked by team id.
	// If the owning pawn's team has an entry here it replaces RulesDataTable; teams without an entry keep the default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Table")
	TMap<int32, UDataTable*> TeamRulesDataTables;

	// Same principle as TeamRulesDataTables, for the attack table.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	TMap<int32, UDataTable*> TeamAttackRulesDataTables;

	/**
	 * AbilityKeys to force-enable for this AI's team at match start (e.g. "BuildFactory").
	 * Abilities can be locked behind progression via their bDisabled flag - a human unlocks them by playing,
	 * but the AI has no progression path, so anything listed here stays permanently unbuildable for it.
	 * Keys that do not exist are simply ignored, so one shared list can cover every faction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules")
	TArray<FString> ForceEnabledAbilityKeys;
	// Time to wait before returning the RLAgent to its original location after issuing attack orders
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0"))
	float AttackReturnDelaySeconds = 3.0f;
	// Minimum time between attack rule evaluations. If not elapsed, attack rules are skipped and normal rules/wander run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0"))
	float AttackRuleCheckIntervalSeconds = 30.0f;
	// Optional override positions for attack rules by table row index. Index 0 -> Row 0, 1 -> Row 1, etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	TArray<FVector> AttackPositions;

	// Delay in seconds after game start before initial attack position search.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	float AttackPositionUpdateDelay = 2.0f;

	// Interval in seconds between refreshing AttackPositions from classes during runtime. If <= 0.0, no repeating timer is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	float AttackPositionRefreshInterval = 10.0f;

	/** The extent used when projecting a point to the NavMesh to validate attack/move commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Rules|AttackTable")
	FVector NavMeshProjectionExtent = FVector(50.f, 50.f, 250.f);

	// ---------------- Supply priority ----------------
	// Being at the supply cap stops unit production dead, so no army is ever fielded and no attack rule
	// can ever match. Leaving the supply building in the weighted draw meant a team could sit at 10/10 for
	// six minutes without building the one thing that unblocks it. When headroom runs out it is not a
	// preference any more - it takes precedence.

	/** Rules that raise the supply cap; forced whenever headroom drops below ForceSupplyBelowHeadroom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Supply")
	TArray<FName> SupplyRuleNames = { FName(TEXT("SynapseCluster")), FName(TEXT("Reactor")) };

	/** Remaining supply below which the supply rule bypasses the draw. 0 disables the override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Supply", meta=(ClampMin="0"))
	int32 ForceSupplyBelowHeadroom = 10;

	/**
	 * Minimum gap between two forced supply builds.
	 *
	 * Without it the override took EVERY decision while headroom was low, so both factions built nothing
	 * but supply buildings for eight minutes - no unit producers, no army, no fight. The point is to
	 * guarantee supply keeps up, not to monopolise the AI.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Supply", meta=(ClampMin="0.0"))
	float SupplyForceIntervalSeconds = 20.f;

	mutable float LastSupplyForceTimeSeconds = -1000000.f;

	/** Remaining supply for this team, or -1 when it cannot be determined. */
	float GetSupplyHeadroom() const;

	/**
	 * War die zuletzt getroffene Entscheidung der Wander-Pfad?
	 *
	 * Der Wander-Pfad ist der Rueckfall, wenn keine Regel passt, und waehlt mit
	 * FMath::RandRange aus einer Liste - reiner Muenzwurf. Gemessen am 29.08.: er stellt die
	 * HAELFTE aller Entscheidungen des Lehrers (13388 von 26836). Wer diese Zeilen mit
	 * aufzeichnet, bringt dem Netz bei, einen Wuerfel nachzuahmen; die Wahrscheinlichkeitsmasse
	 * verteilt sich auf Aktionen, die keine Absicht tragen.
	 *
	 * mutable, weil RecordDecisionForTraining const ist und die Entscheidungspfade Lambdas in
	 * einer const-Methode sind.
	 */
	mutable bool bLastDecisionWasWander = false;

	// ---------------- Expansion cadence ----------------
	// Expanding was left to the weighted draw, where a frequency of 200 competes against a pool of
	// several thousand: the rule PASSED 65 times in five minutes and was picked exactly zero times.
	// A base is a scheduling decision, not a lottery ticket, so these rules get their own timer.

	/** Rule rows that run on a fixed cadence instead of competing in the weighted draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Expansion")
	TArray<FName> ExpansionRuleNames = { FName(TEXT("HiveExpansion")), FName(TEXT("BaseExpansion")) };

	/** Seconds between forced expansions. 0 disables the cadence and leaves them in the draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Expansion", meta=(ClampMin="0.0"))
	float ExpansionIntervalSeconds = 240.f;

	/** Mutable: the selection pass is const, but the cadence has to remember when it last fired. */
	mutable float LastExpansionFireTimeSeconds = 0.f;

	// ---------------- Defence ----------------
	// Attack rules only ever send units OUT. Nothing brought them home when the base itself was being
	// torn down, so a raid met no resistance at all.

	/** Off disables the defence reaction entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Defence")
	bool bEnableDefence = true;

	/** How far from one of our own buildings an enemy counts as "attacking the base". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Defence", meta=(ClampMin="0.0"))
	float DefenceTriggerRadius = 4000.f;

	/** Seconds between defence checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Defence", meta=(ClampMin="0.1"))
	float DefenceCheckIntervalSeconds = 3.0f;

	/** Send workers when there is no combat unit left, so a raid is not simply walked through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Defence")
	bool bDefendWithWorkersAsLastResort = true;

	/** Keeps this many workers mining even while defending with workers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Defence", meta=(ClampMin="0"))
	int32 MinWorkersKeptMining = 4;

	/** Checks for an attack on our base and sends units to meet it. Server only. */
	void EvaluateDefence();

	float LastDefenceCheckTimeSeconds = -1000000.f;

	// ---------------- Wander (small movement) fallback ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bEnableWander = true;
	
	// If true, we try to choose a direction biased toward AverageEnemyPosition using the four directional indices below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bBiasTowardEnemy = true;

	// Directional action indices to use when biasing (configure to match your action mapping)
	// Defaults correspond to ActionSpace indices 17..20 (move_camera 1..4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveUpAction = ERTSAIAction::MoveDirection3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveDownAction = ERTSAIAction::MoveDirection4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveLeftAction = ERTSAIAction::MoveDirection2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveRightAction = ERTSAIAction::MoveDirection1;

	// If no bias, pick randomly from this set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	TArray<ERTSAIAction> RandomWanderActions;

	// Minimal distance to consider we have a meaningful direction to the enemy (units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	float EnemyBiasMinDistance = 50.f;

	// Optional two-step wander: do a selection first (e.g., left_click 1) then perform the wander movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bWanderTwoStep = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	ERTSAIAction WanderSelectionAction = ERTSAIAction::LeftClick1; // left_click 1 by default
	// If you want a specific ability after wander selection, set this to 10..15. If left at None, the second action is the movement itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	ERTSAIAction WanderAbilityAction = ERTSAIAction::None;
	// Minimum number of consecutive wander moves to keep in the same direction before switching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(ClampMin="1"))
	int32 WanderMinSameDirectionRepeats = 3;

public:
	/**
	 * How far an attack waypoint is pulled towards the enemy's centre after the NavMesh projection.
	 * The projection returns the NEAREST navigable point, which is usually a cliff edge - units sent
	 * there pile up on the rim instead of engaging. 0 disables the correction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Attack", meta=(ClampMin="0.0"))
	float AttackWaypointInwardPull = 900.f;

	// Team id of the owning AI pawn, or -1 while it has no controller yet.
	// Public because the RL panel has to map "which AI plays for which team" without duplicating this.
	UFUNCTION(BlueprintCallable, Category = "AI|Rules")
	int32 ResolveOwningTeamId() const;

	/**
	 * Read each rule's resource thresholds from the ability it actually presses instead of the table.
	 * Hand-maintained thresholds drift from the abilities they gate - measured cases included a cost
	 * transcribed into the wrong resource column (500 Primary instead of 500 Secondary, so the rule
	 * never fired) and rules whose threshold was a third of what the ability really costs, so they
	 * fired and failed every time. With this on, changing an ability's ConstructionCost is enough.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules")
	bool bDeriveThresholdsFromAbility = true;

	// Headroom on the derived cost for Primary/Secondary/Tertiary (1.0 = exactly the ability cost).
	// Supply-like resources are never scaled - there the check is "does it still fit under the cap".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules", meta=(ClampMin="1.0"))
	float DerivedThresholdMultiplier = 1.0f;

	/** Resolves Row.SelectionAction + Row.AbilityAction to the ability's ConstructionCost. */
	bool TryGetAbilityCostForRule(const FRTSRuleRow& Row, FBuildingCost& OutCost) const;

private:

	/** True while Row.MinSecondsBetweenActivations has not elapsed since this rule last fired. */
	bool IsRuleOnCooldown(const FRTSRuleRow& Row, const FName& RowName) const;

	/** Same cooldown check for attack rows; kept on its own clock so row names cannot collide. */
	bool IsAttackRuleOnCooldown(const FRTSAttackRuleRow& Row, const FName& RowName) const;

	/** Set when an attack row actually issues its orders. */
	void MarkAttackRuleFired(const FName& RowName) const;

	/** Stamps the activation time used by IsRuleOnCooldown. Call wherever a rule's output is returned. */
	void MarkRuleFired(const FName& RowName) const;

	// Keyed by DataTable row name, so two rows sharing a RuleName still get separate clocks.
	mutable TMap<FName, float> LastRuleActivationTime;

	// Same, for the attack rules table.
	mutable TMap<FName, float> LastAttackRuleActivationTime;

	// Swaps in the team's entries from TeamRulesDataTables / TeamAttackRulesDataTables.
	// Cheap and idempotent: retries every evaluation until the pawn is possessed and a team id is known.
	void ApplyTeamTableOverrides();

	// Runs one tick after BeginPlay, once the AI pawn has been possessed and its team id is readable.
	void InitializeRuleTables();

	bool bTeamTablesApplied = false;

	int32 PickWanderActionIndex(const FGameStateData& GS) const;
	UInferenceComponent* GetInferenceComponent() const;
	// Tracking for wander direction repetition
	int32 LastWanderActionIndex = INDEX_NONE;
	int32 WanderActionRepeatCount = 0;

	// Returns the maximum among all friendly tag unit counts contained in GS
	int32 GetMaxFriendlyTagUnitCount(const FGameStateData& GS) const;

	// Evaluate a single DataTable rule row. Returns empty string if not matched.
	FString EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const;

	// If a RulesDataTable is set, iterate rows and return the first matching rule's JSON
	FString EvaluateRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference) const;

	// Evaluates attack rules and executes them if conditions are met.
	bool EvaluateAttackRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference);

public:
	/**
	 * Send the attack order straight to the units instead of simulating the player's hands.
	 *
	 * The old path teleports the RLAgent to the attack position, presses a control group, presses
	 * attack-move, and teleports the agent back on a timer. Two rules firing shortly after one
	 * another therefore aim at two different agent positions and the same units turn round
	 * mid-march - the "runs back and forth instead of attacking" picture. A batch move carries the
	 * target in the call, so a second order can only ever refine the first one.
	 */
		/**
	 * Angriffsziel: naechstgelegenes gegnerisches Ziel statt eines zufaelligen.
	 *
	 * Vorher waehlte die Zielsuche per FMath::RandRange irgendeinen Gegner aus der Liste - die Armee
	 * lief dadurch regelmaessig quer ueber die Karte an einem naeheren Ziel vorbei, und ein Gegner,
	 * der gerade die eigene Basis angriff, wurde genauso wahrscheinlich ignoriert wie irgendein
	 * Aussenposten. Naechstgelegen zu waehlen heisst deshalb zugleich: Angriffe auf die eigene Basis
	 * werden bevorzugt beantwortet, ohne dass es dafuer eine eigene Verteidigungsregel braucht.
	 *
	 * Bezugspunkt ist der Schwerpunkt der EIGENEN Gebaeude; gibt es keine, faellt es auf die Position
	 * des Agenten zurueck. Auf false gesetzt verhaelt sich die Auswahl wieder wie zuvor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate|AI")
	bool bAttackNearestTarget = true;

	/**
	 * Bei der Zielwahl GEBAEUDE bevorzugen und Einheiten nur nehmen, wenn kein gegnerisches Gebaeude
	 * bekannt ist.
	 *
	 * Warum: die Angriffszeilen fuehren als Quellklassen BuildingBase UND UnitBase, und beide landeten
	 * ununterschieden im selben Topf. Zusammen mit bAttackNearestTarget hiess das: es gewinnt der
	 * naechstgelegene GEGNER-AKTOR - und das ist fast immer eine herumlaufende Einheit, kein Gebaeude.
	 * Zwei Folgen, beide vom Nutzer beobachtet:
	 *   - Das Ziel LAEUFT. Jeder neue Befehl zeigt woanders hin, die Armee dreht unterwegs um.
	 *     Gemessen am 30.08. ueber drei Partien: Zielsspruenge von 1887 bis 5116 Einheiten zwischen
	 *     zwei Befehlen an dieselbe Gruppe, im Abstand von 5,5 bis 22,5 Sekunden.
	 *   - Die Armee erreicht die gegnerische Basis nie, weil sie nie dorthin geschickt wurde. Sie
	 *     jagt Streuner in der Landschaft.
	 * Gebaeude stehen still. Damit wird das Ziel stabil, die Bindung muss seltener eingreifen, und
	 * ein Angriff geht wieder dorthin, wo etwas zu zerstoeren ist.
	 *
	 * Der Vorzug gilt NUR fuer die Zielwahl der KI. Auf false verhaelt es sich wie zuvor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate|AI")
	bool bPreferBuildingTargets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	bool bUseDirectBatchAttackMove = true;

	/**
	 * Seconds an army stays committed to the target it was last sent to.
	 *
	 * The attack rules re-evaluate long before the march is over, and each evaluation picks whatever
	 * enemy position currently wins - measured jumps of 1900 and 12000 cm between two consecutive
	 * orders. The units obey every one of them and spend the match turning round. Inside the commit
	 * window a new order keeps the OLD target, so a march is allowed to finish. 0 disables it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0", ClampMax="300.0"))
	float AttackCommitSeconds = 25.f;

	/**
	 * How far a new target has to be from the current one before the commit window applies, in cm.
	 * A small correction towards the same fight is always allowed through - it is the long jump to a
	 * different fight that turns the army around.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0", ClampMax="20000.0"))
	float AttackRetargetMinDistance = 1500.f;

	/** Spacing of the arrival formation, in cm. 0 sends every unit to the identical point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0", ClampMax="2000.0"))
	float AttackFormationSpacing = 220.f;

private:
	bool ExecuteAttackRuleRow(const FRTSAttackRuleRow& Row, int32 TableRowIndex, const FGameStateData& GS, UInferenceComponent* Inference);

	/**
	 * Gathers this team's units carrying any of the given key tags and gives them one batched
	 * attack-move. Returns false when nothing qualified, so the caller can fall back.
	 */
	bool IssueDirectAttackMove(const TArray<ERTSUnitTag>& Tags, const FVector& Target,
		const FString& RowLabel, int32 LogTeamId);

	/** ERTSUnitTag -> the KeyTag.* gameplay tag the units actually carry. */
	static FGameplayTag KeyTagForUnitTag(ERTSUnitTag Tag);

	// Refreshes the cached AttackPositions by searching for actors of classes specified in the DataTable rows.
	void PopulateAttackPositions();

	// Compose multiple action indices into a single JSON string. If multiple indices are given, returns a JSON array string.
	// AbilityArrayIndexOverride >= 0 stamps "ability_array_index" onto every emitted action so the
	// executing agent can select the array right before pressing, instead of relying on a controller
	// field that the next decision overwrites first.
	FString BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference,
	                                 int32 AbilityArrayIndexOverride = -1,
	                                 bool bAimAtTransporter = false) const;

	/**
	 * Hands the decision to URLRecorderSubsystem so a network can be trained to imitate it. This is how the
	 * rule AI seeds the first RL model: it already plays a competent game, so its decisions are the cheapest
	 * source of "understands the rules" behaviour there is. No-op unless a recording is running.
	 */
	void RecordDecisionForTraining(const TArray<int32>& Indices) const;

	/** State that produced the current decision, cached so each recorded action pairs with its own input. */
	FGameStateData CachedRecordingState;
	bool bHasCachedRecordingState = false;

	/** Previous action written to the training set; becomes the next sample's LastActionIndex feature. */
	mutable int32 LastRecordedActionIndex = -1;

public:
	/**
	 * Welche Aktion die Regel-KI zuletzt aufgezeichnet hat. Fuer die DAgger-Mischung: uebernimmt der
	 * Lehrer einen Zug, muss das Netz denselben Wert als LastActionIndex weitergereicht bekommen.
	 */
	int32 GetLastRecordedActionIndex() const { return LastRecordedActionIndex; }

private:

	// Timestamp of the last time we attempted to evaluate attack rules (seconds). Initialized so first check is allowed immediately.
	float LastAttackRuleCheckTimeSeconds = -1000000.f;

	FTimerHandle AttackPositionRefreshTimerHandle;

	// While true, block ChooseJsonActionRuleBased from returning any action until the return timer completes
	bool bAttackReturnBlockActive = false;
	// Absolute time (GetWorld()->GetTimeSeconds) when the block should auto-expire (safety in case the timer is canceled)
	float AttackReturnBlockUntilTimeSeconds = 0.f;

	// Location to return the RLAgent to after the attack sequence finishes
	FVector AttackReturnLocation = FVector::ZeroVector;

	// NUR DIAGNOSE (Nutzerpunkt 6: Einheiten laufen hin und her statt anzugreifen).
	// Der Angriffsbefehl schickt die Einheiten an die Position, an der der RLAgent im
	// Befehlsmoment steht. Springt diese Position zwischen zwei Aktivierungen weit, bekommen
	// dieselben Einheiten laufend neue, weit auseinanderliegende Ziele - das waere das
	// Hin-und-her. Hier wird die vorige Befehlsposition und der Zeitpunkt gemerkt, um Takt
	// und Sprungweite messen zu koennen. Kein Eingriff.
	/**
	 * Bestandsmeldung: alle N Sekunden die Truppenstaerke je Kontrollgruppe ins Log.
	 *
	 * Grund: die Endzahl "lebende Einheiten" ist bei einer Fraktion, die jede Partie auf 0 endet,
	 * als Messgroesse unbrauchbar - sie kann eine Teilverbesserung nicht anzeigen. Diese Zeile
	 * zeigt den HOECHSTSTAND und den Zeitpunkt des Einbruchs, und erst daran ist zu erkennen, ob
	 * eine Aenderung ueberhaupt etwas bewirkt hat.
	 */
	UPROPERTY(EditAnywhere, Category="AI|Diagnose", meta=(ClampMin="0.0", ClampMax="300.0"))
	float BestandsmeldungIntervallSekunden = 30.f;

	float LetzteBestandsmeldung = -1.f;

	FVector LetzteAngriffsBefehlPos = FVector::ZeroVector;
	float LetzteAngriffsBefehlZeit = -1.f;

	// Helper to handle the return move and post-return actions
	void FinalizeAttackReturn();
};
