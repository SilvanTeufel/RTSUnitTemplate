// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "Core/StartLoadoutTypes.h"
#include "StartLoadoutComponent.generated.h"

class ACameraControllerBase;
class AUnitBase;
class AWaypoint;
class UDataTable;
class UStartLoadoutWidget;

/** What the card selection is for. */
UENUM(BlueprintType)
enum class EStartLoadoutMode : uint8
{
	/** One selection at match start (with buildings), plus the ExtraRounds later on. */
	StartLoadout	UMETA(DisplayName = "Start Loadout"),
	/**
	 * Autobattler: a new card every round; every card chosen so far spawns its army again each round (the armies
	 * grow), not selectable, marching on the enemy. Buildings and workers of the cards are left out.
	 */
	Autobattler		UMETA(DisplayName = "Autobattler"),
};

/** A later card selection in a normal match (e.g. at minute 8): mobile units and a bonus, no buildings. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutRound
{
	GENERATED_BODY()

	/** Seconds after the match start (the loading screen gone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	float AtSeconds = 480.f;

	/** The sets (row struct FStartLoadoutSetRow). Their Tech and Production cards are ignored. Empty = CardSets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (RequiredAssetDataTags = "RowStructure=/Script/RTSUnitTemplate.StartLoadoutSetRow"))
	TObjectPtr<UDataTable> CardSets = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	int32 OfferedSets = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	float SelectionSeconds = 30.f;

	/** The bonus goes to every unit of the team on the field (buildings excepted), not only to the new ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	bool bBonusForAllUnits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	FText Header = NSLOCTEXT("StartLoadout", "ReinforcementsHeader", "Reinforcements");
};

/**
 * Card selection, switched on in the game mode (bEnabled). StartLoadout mode: when the loading screen is gone, every
 * human player picks one of the card sets of his faction (CardSets, rows FStartLoadoutSetRow) - workers, army, a tech
 * and a production building; the main base is always there. ExtraRounds open more selections later (mobile units and
 * a bonus for the whole army). Autobattler mode: see EStartLoadoutMode. AI players (bIsAi) pick at random. The game is
 * paused while the players choose (bPauseWhileChoosing). Spawned units cost supply like trained ones and refund it.
 *
 * Network: the offer goes to each player through a UStartLoadoutPlayerComponent the server adds to his controller
 * (Client RPC), the choice comes back through it (Server RPC). The spawned units replicate like produced ones.
 */
UCLASS(ClassGroup = (RTSUnitTemplate), meta = (BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UStartLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStartLoadoutComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	EStartLoadoutMode Mode = EStartLoadoutMode::StartLoadout;

	/** The card sets (row struct FStartLoadoutSetRow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (RequiredAssetDataTags = "RowStructure=/Script/RTSUnitTemplate.StartLoadoutSetRow"))
	TObjectPtr<UDataTable> CardSets = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	TArray<FStartLoadoutFaction> Factions;

	/** Start bonuses (row struct FStartLoadoutBonusRow): every offered set without a fixed Bonus draws one at random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (RequiredAssetDataTags = "RowStructure=/Script/RTSUnitTemplate.StartLoadoutBonusRow"))
	TObjectPtr<UDataTable> Bonuses = nullptr;

	/** How many sets each player is offered, drawn at random from his faction's sets (every player his own draw). 0 = all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	int32 OfferedSets = 3;

	/**
	 * Most cards one set may have (workers, army, extra army, buildings, cooldown reset). The selection screen lays
	 * them out two per row - a seventh card makes a fourth row and the set no longer fits on a 1080p screen. A set
	 * with more loses its last extra army cards, then its support building (logged). 0 = no limit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	int32 MaxCardsPerSet = 6;

	/** The selection screen. Empty = UStartLoadoutWidget (builds its own layout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	TSubclassOf<UStartLoadoutWidget> WidgetClass;

	/** Time to choose (real seconds). Who has not chosen then gets a random set. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout", meta = (ClampMin = "0"))
	float SelectionSeconds = 60.f;

	/** The time limits (SelectionSeconds, a round's SelectionSeconds) only count with more than one human player.
	 *  Off = the countdown runs alone too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	bool bTimeLimitOnlyInMultiplayer = false;

	/** Pause the game while the players choose (needs a game mode that allows pausing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	bool bPauseWhileChoosing = true;

	/** Difficulty levels, chosen before the start selection (StartLoadout mode). Empty = no difficulty selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Difficulty")
	TArray<FStartLoadoutDifficulty> Difficulties;

	/** Preselected level (and the one taken when nobody can choose). INDEX_NONE = the last one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Difficulty")
	int32 DefaultDifficulty = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Difficulty")
	FText DifficultyHeader = NSLOCTEXT("StartLoadout", "DifficultyHeader", "Choose your difficulty");

	/** The chosen level (index into Difficulties), INDEX_NONE before the choice. */
	UPROPERTY(BlueprintReadOnly, Category = "Start Loadout|Difficulty")
	int32 ChosenDifficulty = INDEX_NONE;

	/** Server: the host's difficulty choice (from UStartLoadoutPlayerComponent::Server_ChooseDifficulty). */
	void ReceiveDifficulty(APlayerController* PC, int32 Index);

	/** Server: weakens a unit of a team without a human player by the chosen difficulty (once per unit). The game
	 *  mode calls it for every unit that gets its index; units already there are done when the level is chosen. */
	void ApplyDifficultyTo(AUnitBase* Unit);

	/** StartLoadout mode: more selections later in the match (mobile units and a bonus, no buildings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	TArray<FStartLoadoutRound> ExtraRounds;

	/**
	 * Auto ability cards (row struct FStartLoadoutSetRow with AutoAbility set): offered right after the start
	 * selection and after every ExtraRound. A card switches an automatic ability on for one unit type of the team.
	 * The rows are also the pool the units draw from at their level milestones (UAutoAbilitySubsystem). Empty = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Auto Abilities", meta = (RequiredAssetDataTags = "RowStructure=/Script/RTSUnitTemplate.StartLoadoutSetRow"))
	TObjectPtr<UDataTable> AutoAbilityCardSets = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Auto Abilities", meta = (ClampMin = "1"))
	int32 AutoAbilityOfferedSets = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Auto Abilities", meta = (ClampMin = "0"))
	float AutoAbilitySelectionSeconds = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Auto Abilities")
	FText AutoAbilityHeader = NSLOCTEXT("StartLoadout", "AutoAbilityHeader", "Choose an automatic ability");

	/** Shown above the auto ability cards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Auto Abilities", meta = (MultiLine = true))
	FText AutoAbilityHint = NSLOCTEXT("StartLoadout", "AutoAbilityHint", "Automatic abilities fire by themselves - when a unit attacks, gets hit or enters a fight - and then cool down. The card switches its ability on for EVERY unit of the shown type: the ones you have and every one you build later. Take the same ability again in the attribute tree and its cooldown is halved. Units also learn a random one at level 5, 10, 15 and 20.");

	/** Spawned units pay their supply (StartupSupplyCost, else UnitSpaceNeeded) and get it back when they die. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout")
	bool bChargeSupply = true;

	/** Autobattler: seconds from one wave to the next selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Autobattler", meta = (ClampMin = "5"))
	float AutobattlerRoundSeconds = 40.f;

	/** Autobattler: time to choose a card each round (real seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Autobattler", meta = (ClampMin = "0"))
	float AutobattlerSelectionSeconds = 20.f;

	/** Autobattler: the spawned units can be selected and ordered. Off = they fight on their own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Autobattler")
	bool bAutobattlerUnitsSelectable = false;

	/** Autobattler: the state the units march on the enemy in (they attack what they meet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Autobattler")
	TEnumAsByte<UnitData::EState> AutobattlerUnitState = UnitData::Patrol;

	/** Autobattler: a team of the Factions (TeamIds) without any player picks its cards at random by itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Autobattler")
	bool bFillEmptyTeams = true;

	/** Free space a start building needs around it (radius, uu) - at least, its own size counts if bigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Placement", meta = (ClampMin = "100"))
	float BuildingRadius = 500.f;

	/** Gap between a start building and other buildings (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Placement", meta = (ClampMin = "0"))
	float BuildingGap = 450.f;

	/** Most height difference of the ground under a start building (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Placement", meta = (ClampMin = "1"))
	float MaxHeightDifference = 120.f;

	/** Start units stand at most this much higher or lower than the main base (uu) - not on another plateau. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Placement", meta = (ClampMin = "1"))
	float SameLevelMaxHeightDifference = 100.f;

	/** Least distance between two start units (uu) - bigger units keep their own size apart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Loadout|Placement", meta = (ClampMin = "50"))
	float UnitSpacing = 220.f;

	/** Server: the game mode has given every controller its team (SetTeamIdsAndWaypoints). */
	void OnControllersGathered();

	/** Server: opens the first selection (the game mode calls it when the loading screen is gone). Once per match. */
	void BeginSelection();

	/** Server: a player's choice (from UStartLoadoutPlayerComponent::Server_ChooseLoadout). */
	void ReceiveChoice(APlayerController* PC, FName RowName);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|StartLoadout")
	bool IsSelecting() const { return bSelecting; }

	/** Autobattler: the round that is being played (1 = the first). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|StartLoadout")
	int32 GetAutobattlerRound() const { return AutoRound; }

	/** The sets of a faction in a table, in table order. */
	void GetSetsForFaction(const UDataTable* Table, FName Faction, TArray<FName>& OutRowNames, TArray<FStartLoadoutSetRow>& OutRows) const;

	/** The faction a team plays (TeamIds, else its main base class). Null = none. */
	const FStartLoadoutFaction* FindFactionForTeam(int32 TeamId) const;

	/** Server: removes every running ability cooldown of the team's units and buildings. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void ResetTeamCooldowns(int32 TeamId);

	/** Server: applies a bonus to a unit. bFresh = just spawned (full health); else the health share is kept. */
	void ApplyBonus(AUnitBase* Unit, const FStartLoadoutBonus& Bonus, bool bFresh) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class ESelectionKind : uint8 { Start, Extra, Autobattler, AutoAbility };

	struct FSelection
	{
		ESelectionKind Kind = ESelectionKind::Start;
		const UDataTable* Table = nullptr;
		int32 Offered = 3;
		float Seconds = 60.f;
		bool bBonusForAllUnits = false;
		FText Header;
		FText Hint;
	};

	struct FSpawnOptions
	{
		AWaypoint* Waypoint = nullptr;
		bool bSelectable = true;
		UnitData::EState State = UnitData::None;
	};

	void OpenSelection(const FSelection& Selection);
	void OpenDifficultySelection();
	void SetDifficulty(int32 Index);
	void ScaleForDifficulty(AUnitBase* Unit) const;
	void OpenExtraRound(int32 Index);
	void OpenAutoAbilityRound();
	void ApplyAutoAbilityCard(int32 TeamId, const FStartLoadoutSetRow& Set);
	void OpenAutobattlerRound();
	bool TickSelection(float DeltaTime);
	void FinishSelection(const TCHAR* Reason);
	bool CanUnpause() const;
	void DrawOffer(FName Faction, TArray<FName>& Names, TArray<FStartLoadoutSetRow>& Rows) const;

	void ApplyLoadout(ACameraControllerBase* PC, const FStartLoadoutFaction& Faction, const FStartLoadoutSetRow& Set);
	void ApplyReinforcements(int32 TeamId, const FStartLoadoutSetRow& Set);
	void SpawnAutobattlerWave();
	void ApplyBonusToSpawned(const TArray<AUnitBase*>& Units, const FStartLoadoutBonus& Bonus, int32 TeamId) const;
	void ChargeSupply(AUnitBase* Unit) const;
	bool FindTeamAnchor(int32 TeamId, FVector& OutCentre, float& OutRadius, FVector& OutForward) const;
	AWaypoint* GetEnemyWaypoint(int32 TeamId);

	AUnitBase* FindMainBase(int32 TeamId, const FVector& Near) const;
	AUnitBase* SpawnAt(const FStartLoadoutCard& Card, const FVector& Ground, int32 TeamId, const FSpawnOptions& Options = FSpawnOptions()) const;
	bool FindBuildingSpot(const FVector& Centre, float CentreRadius, const FVector& Back, float Radius, FVector& Out) const;
	/** The set's ExtraArmy cards, each its own block beside the army at ArmyAnchor. */
	void PlaceExtraArmy(const FStartLoadoutSetRow& Set, const FVector& ArmyAnchor, const FVector& Forward, int32 TeamId,
		TArray<AUnitBase*>& OutUnits, const FSpawnOptions& Options = FSpawnOptions()) const;

	void PlaceUnits(const FStartLoadoutCard& Card, const FVector& Anchor, const FVector& Forward, int32 TeamId, TArray<AUnitBase*>& OutUnits,
		const FSpawnOptions& Options = FSpawnOptions()) const;
	void DrawBonuses(TArray<FStartLoadoutSetRow>& Rows) const;
	const FStartLoadoutSetRow* FindOfferedRow(APlayerController* PC, FName RowName) const;
	bool GroundAt(const FVector& P, FVector& Out) const;
	bool ProjectToNav(const FVector& P, FVector& Out) const;
	void CollectObstacles();

	/** Navmesh height of the main base while a match start is placed (PlaceUnits keeps to it); unset otherwise. */
	mutable TOptional<float> SameLevelZ;

	struct FObstacle
	{
		FVector2D Position;
		float Radius;
	};
	TArray<FObstacle> Obstacles;

	bool bGathered = false;
	bool bBeginPending = false;
	bool bStarted = false;
	bool bSelecting = false;
	bool bChoosingDifficulty = false;
	bool bPaused = false;
	TWeakObjectPtr<APlayerController> DifficultyChooser;
	/** Teams with a human player - the difficulty leaves them alone. */
	TSet<int32> HumanTeams;
	TSet<TWeakObjectPtr<AUnitBase>> DifficultyScaled;
	double EndTime = 0.0;
	FTSTicker::FDelegateHandle Ticker;
	FSelection Active;
	int32 AutoRound = 0;
	TArray<int32> QueuedExtraRounds;
	FTimerHandle RoundTimer;

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerController>, FName> Choices;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerController>> Waiting;

	/** The sets each player was offered - only these count as his choice. */
	TMap<TWeakObjectPtr<APlayerController>, TArray<FName>> Offered;

	/** The offered sets as sent (with their drawn bonus) - what is applied. */
	TMap<TWeakObjectPtr<APlayerController>, TArray<FStartLoadoutSetRow>> OfferedRows;

	/** Teams without a player (bFillEmptyTeams): their random pick of this selection. */
	TMap<int32, FStartLoadoutSetRow> TeamPicks;

	/** Autobattler: every card a team has chosen, spawned again each round. */
	TMap<int32, TArray<FStartLoadoutSetRow>> Collected;

	/** Factions whose "last offer" memory was already renewed in this selection (DrawOffer is const). */
	mutable TSet<FName> LastOfferRenewed;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AWaypoint>> EnemyWaypoints;
};
