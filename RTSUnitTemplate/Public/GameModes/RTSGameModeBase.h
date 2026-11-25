// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Waypoint.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstance.h"
#include "Core/UnitData.h"
#include "Core/WorkerData.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayEffect.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Developer/GraphColor/Private/appconst.h"
#include "GameplayTagContainer.h"
#include "RTSGameModeBase.generated.h"

class ARLAgent;
class ACameraControllerBase;
class APlayerStartBase;
class ARTSBTController;
class AController;
class UBehaviorTree;
class UWorld;
class AUnitBase;
class AWinLoseConfigActor;
class ULoadingWidget;

/** Feuert, wenn der Ladebildschirm abgelaufen ist und das Match sichtbar beginnt. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRTSOnLoadingScreenFinished);

USTRUCT(BlueprintType)
struct FTimerHandleMapping
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	int32 Id = 0;

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	FTimerHandle Timer;

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	bool SkipTimer = false;
};
USTRUCT()
struct FTagCountMap
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, int32> TagCounts;
};

UCLASS()
class RTSUNITTEMPLATE_API ARTSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSGameModeBase();

	/** Procedural ponds (water drawn by the landscape material, navmesh blocked). Off by default - bEnabled. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	TObjectPtr<class UProceduralPondComponent> ProceduralPonds;

	/**
	 * Full procedural map: terrain, main bases, spawn points and waypoints at random at every match start.
	 * Off by default - bEnabled. Needs a level actor with IRTSProceduralTerrain (the terrain comes from the game).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	TObjectPtr<class UFullProceduralMapComponent> FullProcedural;

	/**
	 * Start loadout: at match start every player picks a card set (workers, army, tech and production building) from
	 * a data table, AI players at random. Off by default - bEnabled. See UStartLoadoutComponent.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<class UStartLoadoutComponent> StartLoadout;

	/** Places the procedural ponds - after the procedural resources (AResourceGameMode::StartPlay), before any BeginPlay. */
	virtual void StartPlay() override;

	/**
	 * NUR FUER MESSLAEUFE. 0 = aus, jede Partie laeuft mit echtem Zufall wie im Spiel.
	 * Ein Wert != 0 setzt den globalen Zufall bei JEDEM Partiestart auf diesen Startwert.
	 *
	 * Warum das noetig ist: die Engine ruft RandInit/SRandInit genau EINMAL beim
	 * Prozessstart (LaunchEngineLoop.cpp, FEngineLoop::PreInit). `-FixedSeed` macht
	 * damit den PROZESS reproduzierbar, aber der zweite PIE-Lauf im selben Editor
	 * laeuft im Zufallsstrom einfach weiter - zwei Laeufe sind nie vergleichbar.
	 * Erst ein Startwert pro Partie erlaubt den gepaarten Vergleich: dieselbe Partie
	 * einmal mit und einmal ohne die zu pruefende Aenderung.
	 *
	 * Standard 0, damit das im Spiel garantiert nichts aendert.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Messung")
	int32 FesterZufallsStartwert = 0;

	// Pawn class to use when spawning AI players for AI PlayerStarts
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ARLAgent> AIPlayerPawnClass;

 // PlayerController class to use when spawning AI players (must derive from ACameraControllerBase)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ACameraControllerBase> AIPlayerControllerClass;

	// Optional: AI orchestrator controller class (non-possessing) that runs the Behavior Tree
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ARTSBTController> AIOrchestratorClass;

	// Optional: Behavior Tree to assign to the orchestrator if its StrategyBehaviorTree is not set
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TObjectPtr<UBehaviorTree> AIBehaviorTree;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void ApplyCustomizationsFromPlayerStart(APlayerController* PC, const APlayerStartBase* CustomStart);
		
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FTimerHandleMapping> SpawnTimerHandleMap;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TimerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DisableSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelaySpawnTableTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int GatherControllerTimer = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LoadingTimePerUnit = 0.025f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxLoadingTime = 40.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	TSubclassOf<class ULoadingWidget> LoadingWidgetClass;

	/**
	 * Fires once the loading widget's duration has elapsed - the moment the player actually sees
	 * the level. Anything that must not happen behind the loading screen (starting a scripted
	 * battle, a capture, a cinematic) should hang off this instead of BeginPlay.
	 */
	UPROPERTY(BlueprintAssignable, Category = RTSUnitTemplate)
	FRTSOnLoadingScreenFinished OnLoadingScreenFinished;

	UFUNCTION()
	void HandleLoadingScreenFinished();

	UPROPERTY()
	int32 LoadingWidgetTriggerId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	TArray<AWinLoseConfigActor*> WinLoseConfigActors;

	FTimerHandle WinLoseTimerHandle;

	void CheckWinLoseConditionTimer();

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	AWinLoseConfigActor* WinLoseConfigActor; // Primary or first found config actor

	/** True once the match was decided (won or lost) - nothing new (card offers, ...) should start after that. */
	bool IsWinLoseTriggered() const { return bWinLoseTriggered; }

	bool bWinLoseTriggered = false;
	bool bInitialSpawnFinished = false;
	bool bBuildingsEverExisted = false;

	UPROPERTY()
	TMap<FGameplayTag, int32> TagsDestroyedCountMap;

	UPROPERTY()
	TMap<FGameplayTag, int32> TagsAliveCountMap;

	UPROPERTY()
	TMap<int32, FTagCountMap> TeamTagsDestroyedCountMap;

	UPROPERTY()
	TMap<int32, FTagCountMap> TeamTagsAliveCountMap;

	bool IsAnyUnitWithTagAlive(const FGameplayTag& Tag, const TMap<FGameplayTag, int32>& AliveTagCounts) const;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	void InitializeWinLoseConfigActors();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	void UpdateTagProgressForConfig(AWinLoseConfigActor* Config);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	virtual float GetResource(int32 TeamId, EResourceType ResourceType) const;

	virtual void CheckWinLoseCondition(AUnitBase* DestroyedUnit = nullptr);

	void TriggerWinLoseForPlayer(ACameraControllerBase* PC, bool bWon, AWinLoseConfigActor* Config);

	/**
	 * Ist ausser diesem Spieler noch jemand in der Partie, fuer den Sieg oder Niederlage noch NICHT
	 * gefallen ist?
	 *
	 * Genau die Bedingung, unter der Zuschauen ueberhaupt Sinn ergibt: laeuft fuer niemanden mehr
	 * ein Spiel, gibt es auch nichts zu sehen. Nur serverseitig sinnvoll - ein Client kennt die
	 * fremden PlayerController nicht.
	 *
	 * @param Ausser  Der Spieler, der fragt; er selbst zaehlt nicht mit.
	 */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Spectator")
	bool AreOtherPlayersStillPlaying(ACameraControllerBase* Ausser) const;

	/**
	 * Loest Sieg oder Niederlage fuer ein ganzes Team von aussen aus.
	 *
	 * Gedacht fuer Siegbedingungen, die sich nicht ueber die Aufzaehlung EWinLoseCondition
	 * abbilden lassen - etwa "einen bestimmten Punkt der Karte erreichen". Ein Auslaeser im
	 * Level ruft das hier auf, statt dass die Aufzaehlung fuer jeden Sonderfall waechst.
	 *
	 * @param TeamId  Das Team, fuer das gewertet wird.
	 * @param bWon    true = Sieg, false = Niederlage.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	void TriggerWinLoseForTeam(int32 TeamId, bool bWon);

	/** Traegt die ueberlebte Zeit in die Survival-Bestenliste ein (nur Endlos-Karten). */
	void MeldeSurvivalZeit(ACameraControllerBase* PC);

	// Enter read-only spectate for the given controller (called on the defeat branch of TriggerWinLoseForPlayer).
	// Base implementation is an intentional no-op so all existing games are unaffected;
	// AExodusGameMode overrides this to swap the defeated player's pawn for a spectator camera.
	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate|Spectator")
	virtual void EnterSpectate(AController* Controller, bool bRevealAll);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TriggerWinLoseUI(bool bWon, TSubclassOf<class UWinLoseWidget> InWidgetClass, const FString& InMapName, FName DestinationSwitchTagToEnable);
	
	virtual void BeginPlay() override;

	void ReleaseEffectAreas();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DataTableTimerStart();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool PathfindingIsRdy = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsPathfindingRdy(){ return PathfindingIsRdy; };
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void NavInitialisation();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	
	void SetupLoadingWidgetForPlayer(APlayerController* NewPlayer);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamIdAndDefaultWaypoint(int Id, AWaypoint* Waypoint, ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void FillUnitArrays();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamIdsAndWaypoints();

	/** True, sobald SetTeamIdsAndWaypoints gelaufen ist. Wer danach joint, wird einzeln eingerichtet. */
	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bControllersGathered = false;

	/** Spieler, die nach GatherControllerTimer joinen, automatisch einrichten (PostLogin). Aus fuer GameModes mit eigener Team-Logik. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bInitializeLateJoiners = true;

	/**
	 * Richtet einen Spieler ein, der erst nach GatherControllerTimer verbunden wurde (PostLogin / Seamless Travel):
	 * Team, PlayerStart, Kamera, HUD, Fog, Minimap, WinLose - dieselbe Kette wie in SetTeamIdsAndWaypoints.
	 * Tut nichts, wenn der Controller schon eingerichtet ist (bRTSPlayerInitialized).
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool InitializeLateJoiningPlayer(ACameraControllerBase* CameraControllerBase);

protected:
	/** Pro-Spieler-Teil von SetTeamIdsAndWaypoints. Entfernt den gewaehlten Start aus PlayerStarts und zaehlt PlayerIndex hoch. */
	bool SetupHumanController(ACameraControllerBase* CameraControllerBase, TArray<APlayerStartBase*>& PlayerStarts, class UPlayerTeamSubsystem* TeamSubsystem, int32& PlayerIndex);

	/** Wartet nach PostLogin, bis der Spieler Kamera-Pawn und PlayerState hat, und richtet ihn dann ein. */
	void TryInitializeLateJoiner(TWeakObjectPtr<ACameraControllerBase> CameraControllerBase, int32 Attempt);

	/** Nicht-KI-Starts, die beim Sammeln frei geblieben sind - fuer spaeter joinende Spieler. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerStartBase>> FreePlayerStarts;

	/** Naechster Index fuer Character.CameraUnit.N und die Start-Rueckfallwahl. */
	int32 NextPlayerIndex = 0;

public:
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetupTimerFromDataTable(FVector Location, AUnitBase* UnitToChase);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetupUnitsFromDataTable(FVector Location, AUnitBase* UnitToChase, const TArray<class UDataTable*>& UnitTable); // , int TeamId, const FString& WaypointTag, int32 UnitIndex = 0, AUnitBase* SummoningUnit = nullptr, int SummonIndex = -1
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FTimerHandleMapping GetTimerHandleMappingById(int32 SearchId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSkipTimerMappingById(int32 SearchId, bool Value);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase); // , int TeamId, AWaypoint* Waypoint = nullptr

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsUnitWithIndexDead(int32 UnitIndex);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool RemoveDeadUnitWithIndexFromDataSet(int32 UnitIndex);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 CheckAndRemoveDeadUnits(int32 SpawnParaId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnits(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase); // , int TeamId, AWaypoint* Waypoint = nullptr, int32 UnitIndex = 0, AUnitBase* SummoningUnit = nullptr, int SummonIndex = -1

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int AddUnitIndexAndAssignToAllUnitsArray(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int AssignNewHighestIndex(AUnitBase* Unit);

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 HighestUnitIndex = 0;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddUnitIndexAndAssignToAllUnitsArrayWithIndex(AUnitBase* UnitBase, int32 Index, FUnitSpawnParameter SpawnParameter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AssignWaypointToUnit(AUnitBase* UnitBase, const FString& WaypointTag);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector CalcLocation(FVector Offset, FVector MinRange, FVector MaxRange);

	/**
	 * Live head count of a team. Pass bInvert = true to count every unit whose TeamId is
	 * NOT InTeamId (used for "all opponents" when no explicit opponent team is configured).
	 * Counts only valid, non-dead units from AllUnits, so hand-placed units count too.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 CountAliveUnitsForTeam(int32 InTeamId, bool bInvert = false) const;

	/**
	 * Multiplier the adaptive reinforcement loop wants to apply to a row right now.
	 * Returns 1.0 when the row has bAdaptiveSpawn off, so callers can multiply blindly.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetAdaptiveSpawnMultiplier(const FUnitSpawnParameter& SpawnParameter) const;

	// --- Player pressure (survival): enemy waves follow how well the player is doing ---------------------

	/**
	 * Enemy spawn rows (TeamId != PressureTeamId) get PlayerPressureMultiplier on their wave size and cap:
	 * down to -PressureMaxReduction while the pressured team loses many units (optionally up to +PressureMaxBonus
	 * with a high income and few losses, see bPressureIncomeRaisesWaves). Measured over PressureWindowSeconds every
	 * PressureUpdateInterval from two counters (worker deliveries, unit losses) - nothing scans per frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure")
	bool bUsePlayerPressureSpawns = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns"))
	int32 PressureTeamId = 2;

	/** Off (default): only lost units change the waves (they can only get easier). On: a high income with few losses adds up to PressureMaxBonus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns"))
	bool bPressureIncomeRaisesWaves = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0"))
	float PressureMaxBonus = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0", ClampMax = "0.95"))
	float PressureMaxReduction = 0.5f;

	/** Gathered resources per minute (all types) at which the bonus starts ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0"))
	float PressureIncomeNeutralPerMinute = 800.f;

	/** ... and at which it is full (when the team also loses nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "1"))
	float PressureIncomeHighPerMinute = 2000.f;

	/** Share of the team's units lost within the window that means the full reduction (0.2 = a fifth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0.01"))
	float PressureLossShareForFullReduction = 0.2f;

	/** Seconds from match start in which the waves can only get easier, never harder: the opening economy is no proof of a good defence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0"))
	float PressureGraceSeconds = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "10"))
	float PressureWindowSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "1"))
	float PressureUpdateInterval = 5.f;

	/** The multiplier rises at most this much per update, so waves never jump up... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0.01"))
	float PressureMaxStep = 0.01f;

	/** ... and falls at most this much per update: relief after losses comes quickly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Pressure", meta = (EditCondition = "bUsePlayerPressureSpawns", ClampMin = "0.01"))
	float PressureMaxStepDown = 0.15f;

	/** Current factor on enemy waves (1 = neutral). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "RTSUnitTemplate|Pressure")
	float PlayerPressureMultiplier = 1.f;

	/** Server: a worker delivered mined resources for TeamId (the income the pressure measures). */
	void NoteGatheredResources(int32 InTeamId, float Amount);

	/** Factor for a spawn row: PlayerPressureMultiplier for enemy rows, else 1. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Pressure")
	float GetPressureSpawnMultiplier(const FUnitSpawnParameter& SpawnParameter) const;

	/**
	 * Late-game enemy levels: from EnemyLevelBonusStartSeconds of match time on, every unit the spawn system
	 * spawns for a team without a human player gets +EnemyLevelBonusPerStep levels, and EnemyLevelBonusPerStep
	 * more every EnemyLevelBonusIntervalSeconds (20:00 +3, 23:00 +6, ...). Their level cap is lifted to
	 * EnemyLevelBonusMaxLevel (normal units stop at LevelUpData.MaxCharacterLevel).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|EnemyLevelBonus")
	bool bUseEnemyLevelBonus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|EnemyLevelBonus", meta = (EditCondition = "bUseEnemyLevelBonus", ClampMin = "0"))
	float EnemyLevelBonusStartSeconds = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|EnemyLevelBonus", meta = (EditCondition = "bUseEnemyLevelBonus", ClampMin = "1"))
	float EnemyLevelBonusIntervalSeconds = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|EnemyLevelBonus", meta = (EditCondition = "bUseEnemyLevelBonus", ClampMin = "1"))
	int32 EnemyLevelBonusPerStep = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|EnemyLevelBonus", meta = (EditCondition = "bUseEnemyLevelBonus", ClampMin = "1"))
	int32 EnemyLevelBonusMaxLevel = 25;

	/** Extra levels for an enemy unit spawned now (0 before EnemyLevelBonusStartSeconds or when disabled). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|EnemyLevelBonus")
	int32 GetEnemyLevelBonus() const;

	/** Whether a human player controls TeamId (their units never get the enemy level bonus). */
	bool IsHumanPlayerTeam(int32 InTeamId) const;

	/** Server: raises a freshly spawned enemy unit by GetEnemyLevelBonus() (after its attribute setup). */
	void ApplyEnemyLevelBonus(class AUnitBase* Unit, int32 EarnedLevel);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FUnitSpawnData> UnitSpawnDataSets;

	/**
	 * Highest level the units of a spawn row (by Id) have reached. Rows with LoadLevelAfterSpawn start their
	 * new units at this level - a boss that comes back keeps what it earned last time.
	 */
	TMap<int32, int32> EarnedLevelBySpawnId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UDataTable*> UnitSpawnParameters;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 HighestSquadId = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AActor*> AllUnits;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AActor*> CameraUnits;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AWorkingUnitBase*> WorkingUnits;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <ASpeakingUnit*> SpeakingUnits;


	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	

private:
	/** Total resources delivered by workers, per team id. */
	TArray<float> TeamGatheredTotal;

	struct FPressureSample
	{
		double Time = 0.0;
		float Gathered = 0.f;
		int32 Lost = 0;
	};
	TArray<FPressureSample> PressureSamples;
	FTimerHandle PressureTimer;
	double LastPressureLogTime = -1000.0;

	void UpdatePlayerPressure();
};