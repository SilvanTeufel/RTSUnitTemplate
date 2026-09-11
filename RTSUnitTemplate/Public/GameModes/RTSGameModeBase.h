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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FUnitSpawnData> UnitSpawnDataSets;

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

	
};