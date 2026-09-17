// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/UnitData.h"
#include "LevelUnit.h"
#include "TimerManager.h"
#include "AbilityUnit.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AAbilityUnit : public ALevelUnit
{
	GENERATED_BODY()

private:
	FTimerHandle AccelerationTimerHandle;
	FTimerHandle StartAbilitiesActivationTimer;
	bool bStartAbilitiesActivationScheduled = false;
	/** Ist mindestens eine StartAbility wirklich angelaufen? Erst danach kann sie "fertig" sein. */
	bool bStartAbilitiesLaunched = false;
	/**
	 * Spielzeit seit dem Erschaffen beim ERSTEN Aktivierungsversuch, -1 = noch keiner.
	 *
	 * Die Uhr fuer Nachversuche und fuer den KI-Riegel laeuft ab hier und NICHT ab dem
	 * Erschaffen: PossessedBy und BeginPlay rechnen GatherControllerTimer auf die Verzoegerung
	 * drauf, der erste Versuch kann also erst nach 20 s und mehr kommen. Ein Fenster ab
	 * Erschaffungszeit waere dann laengst abgelaufen, bevor ueberhaupt etwas versucht wurde.
	 */
	float StartAbilitiesFirstAttemptTime = -1.f;
	/** Das Nachversuchsfenster ist abgelaufen, ohne dass etwas aktivierte. */
	bool bStartAbilitiesGaveUp = false;
	bool bAbilitiesGranted = false;
	FVector TargetVelocity;
	FVector CurrentVelocity;
	float AccelerationRate;
	FVector ChargeDestination;
	float RequiredDistanceToStart;
	
	void Accelerate();
	void AccelerateFrom();

public:	
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void BeginPlay() override;

	virtual void LevelUp_Implementation() override;

	UFUNCTION(BlueprintCallable, Category=Ability)
	void ActivateStartAbilitiesOnSpawn();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool IsWorker = false;

	/** Wiedereintrittsschutz fuer die Bauuebergabe - siehe AAbilityUnit::SetUnitState. */
	bool bUebergabeLaeuft = false;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Ability)
	void TeleportToValidLocation(const FVector& Destination, float MaxZDifference = 1000.f, float ZOffset = 70.f);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void StartAcceleratingFromDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);
	
	// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);

	/**
	 * True while this unit must not receive ANY new order.
	 *
	 * Declared here and not on the transport class because SetUnitState (this class) and
	 * SwitchEntityTagByState (AMassUnitBase) are the two chokepoints every order-giving path runs
	 * through - and both sit ABOVE ATransportUnit in the hierarchy, so they cannot see its flag.
	 * ATransportUnit overrides this for loaded cargo.
	 */
	virtual bool IsOrderLocked() const { return false; }

	//FTimerHandle CollisionTimerHandle;
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void IsDead();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedMoving();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StoppedMoving();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GotAttacked();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToResource();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToBuild();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToBase();
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void WorkerGoToOther();
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ChangedUnitState(UnitData::EState OldUnitState, UnitData::EState NewUnitState);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	TEnumAsByte<UnitData::EState> GetUnitState() const;

	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void GetAbilitiesArrays();

	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void GetSelectedAbilitiesArray(TSubclassOf<UGameplayAbilityBase>& SAbility);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> StoredUnitState = UnitData::Idle;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ContinuousAttackDuration = 1.0f;
	///////////////////////////////////////////////////////////////////
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>StartAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>SelectableAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>OffensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>AttackAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>ThrowAbilities;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID OffensiveAbilityID;

	/**
	 * Fire the offensive ability at most ONCE per unit lifetime, on first engagement,
	 * instead of on every attack event. Off by default, so existing units are unaffected.
	 *
	 * Needed for one-shot toggles like the Siege tank: GetAbilityForInputID resolves the
	 * ability by ARRAY INDEX (InputID - AbilityOne), and the AI always passes
	 * OffensiveAbilityID, so it can only ever reach index 0. A Siege/UnSiege pair therefore
	 * never un-sieges from the offensive slot - the unit just re-triggers Siege forever.
	 * With this ticked the unit sieges once when it first engages and then stays that way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	bool bFireOffensiveAbilityOnce = false;

	/** Runtime latch for bFireOffensiveAbilityOnce. Not for authoring. */
	UPROPERTY(BlueprintReadWrite, Category = Ability)
	bool bOffensiveAbilityFired = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID ThrowAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID DefaultAbilityID;
	
	UPROPERTY(EditAnywhere, Category = Ability)
	int32 AutoAbilitySequence[4] = {0, 1, 2, 3};

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetAutoAbilitySequence(int Index, int32 Value);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	bool AutoApplyAbility = true;

	// Delay before attempting to activate StartAbilities on spawn (server-only). Adjustable in Details panel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability, meta=(ClampMin="0.0"))
	float StartAbilitiesActivationDelay = 0.1f;

	/**
	 * Wie lange nach dem Erschaffen die Aktivierung nachversucht wird, in Sekunden.
	 *
	 * StartAbilitiesActivationDelay ist eine Wette auf die Uhr. Frueher gab es genau EINEN
	 * Nachversuch, zusammen also 0,2 s - braucht der Spielstart laenger (viele Einheiten, ein
	 * spaeter beitretender Client, langsame Maschine), lief die StartAbility nie. Bei
	 * GA_ActivateAbilities_Build_Barracks bedeutet das: die Faehigkeiten werden nicht
	 * freigeschaltet, und zwar fuer alle.
	 *
	 * Im Normalfall aendert das nichts - der erste Versuch klappt. 0 schaltet das Nachversuchen
	 * ab und stellt das alte Verhalten wieder her.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability, meta=(ClampMin="0.0"))
	float StartAbilitiesRetryWindow = 10.0f;

	/**
	 * Laufen die StartAbilities noch? Solange das stimmt, laesst die KI die Einheit in Ruhe.
	 *
	 * Der BroodHive hat GA_UpgradeBuildingExtension_Slime_Free als StartAbility. Die KI drueckte
	 * aber, sobald sie Rohstoffe hatte, eigene Faehigkeiten auf demselben Gebaeude - und weil
	 * ActivateAbilityByInputID jede weitere Faehigkeit ablehnt, solange eine laeuft, kam die
	 * StartAbility nicht zum Zug. Sie soll EINMAL durchlaufen, bevor die KI eingreift.
	 *
	 * Drei Zustaende, in dieser Reihenfolge geprueft:
	 *   keine StartAbilities        -> nie blockieren
	 *   Zeitfenster abgelaufen      -> nicht mehr blockieren (siehe StartAbilitiesBlockTimeout)
	 *   noch nicht losgelaufen      -> blockieren
	 *   losgelaufen und noch aktiv  -> blockieren
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Ability)
	bool AreStartAbilitiesPending() const;

	/**
	 * Nach so vielen Sekunden seit dem Erschaffen wird nicht mehr blockiert - Sicherheitsnetz.
	 *
	 * Ohne das koennte eine StartAbility, die nie aktiviert (fehlende Vorbedingung, kaputte
	 * Klasse), die KI fuer den Rest der Partie von diesem Gebaeude aussperren. 0 schaltet das
	 * Netz ab und blockiert dann wirklich unbegrenzt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability, meta=(ClampMin="0.0"))
	float StartAbilitiesBlockTimeout = 15.0f;

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool IsAbilityAllowed(EGASAbilityInputID AbilityID, int Ability);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SpendAbilityPoints( EGASAbilityInputID AbilityID, int AbilityIndex);

	/** Setzt einen Faehigkeits-Slot direkt aus der Vorlage des AbilityChoosers (02.09.2026).
	 *
	 *  Anders als SpendAbilityPoints verlangt und verbraucht das keine AbilityPoints: die Wahl
	 *  im Chooser ist eine Vorlage je Tierklasse und soll unabhaengig vom Punktestand gelten,
	 *  sonst muesste der Spieler nach jeder neuen Einheit erneut klicken.
	 *
	 *  Gibt true zurueck, wenn sich dadurch etwas geaendert hat. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	bool ApplyAbilityFromTemplate(EGASAbilityInputID AbilityID, int32 AbilityIndex);

	UFUNCTION(BlueprintCallable, Category = Ability)
	int32 DetermineAbilityID(int32 Level);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AutoAbility();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AddAbilityPoint();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SaveAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void LoadAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void ResetAbility();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void RollRandomAbilitys();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityResetPenalty = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityCostIncreaser = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int MaxAbilityPointsToInvest = 5;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	//TArray<FUnitSpawnData> SummonedUnitsDataSet;

//	UFUNCTION(BlueprintCallable, Category = Ability)
//	void SpawnUnitsFromParameters(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int NewTeamId, AWaypoint* Waypoint, int UIndex);


};
