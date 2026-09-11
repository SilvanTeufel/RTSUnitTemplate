// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS.h"
#include "Abilities/GameplayAbility.h"
#include "Actors/WorkArea.h"
class AUnitBase;
class AGASUnit;
class USoundBase;
#include "GameplayAbilityBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

	
	
public:
	UGameplayAbilityBase();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ManaCost = 0.f;

	// Play a 2D sound only for the owning player of this ability (works from server or client)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void PlayOwnerLocalSound(class USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AbilityCanBeCanceled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bRefundOnCancel = false;
	
	// New: Ability can be globally disabled via this flag (per ability asset)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bDisabled = false;

	// New: Keep units selected after executing this ability
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bKeepSelectionAfterAbility = false;

	// If true and this ability was saved as Disabled, execute it once when loading instead of disabling it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bExecuteOnLoadIfDisabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitAnimOnRotateFinished = UnitData::Attack;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float AnimTimeOnRotateFinished = 0.6f;
	
	// New: Unique key to group abilities, default "None"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString AbilityKey = "None";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UTexture2D* AbilityIcon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= RTSUnitTemplate)
	EGASAbilityInputID AbilityInputID = EGASAbilityInputID::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bExecuteOnPressed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsContinuousAbility = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AAbilityIndicator> AbilityIndicatorClass;

	/** Kanal, gegen den der Mausstrahl fuer den Zielmarker geschossen wird. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Indicator")
	TEnumAsByte<ECollisionChannel> IndicatorTraceChannel = ECC_Visibility;

	/** Wenn wahr, zaehlt ausschliesslich die LANDSCHAFT als Zielflaeche: Marker und Klickpunkt
	 *  werden vom getroffenen Punkt senkrecht auf das Gelaende heruntergezogen. Ohne das klettert
	 *  der Marker auf Einheiten und Gebaeude, weil die den Sichtkanal blockieren. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Indicator")
	bool bTargetLandscapeOnly = false;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnInputReleased();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = RTSUnitTemplate)
	FVector GetTargetLocation() const;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnAbilityCastComplete( const FHitResult& InHitResult = FHitResult());
	
	UFUNCTION(BlueprintNativeEvent, Category = RTSUnitTemplate)
	void OnAbilityMouseHit(const FHitResult& InHitResult);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnProjectileFromClass(FVector Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, float ZOffset, float Scale = 1.f);

	// Override to prevent activation when disabled by flag or by team/key
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	// Track execution to know if an ability class has ever been executed this session
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool UseAbilityQue = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bRotateUnitsToMouse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bStopMovementOnActivation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString AbilityName = "Ability X: \n\n";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost = FBuildingCost{0, 0, 0, 0, 0, 0};

	/**
	 * Population cap for ONE unit type, checked in CanActivateAbility.
	 *
	 * The rule AI presses a single key for a whole group of production buildings, and every building
	 * resolves its own ability from that slot - so one press can produce four different units. A cap
	 * in the rule table would therefore stop all four. Here the cap sits on the ability that spawns
	 * the unit, so a capped type simply stops while the other units of the same slot keep coming -
	 * which is what "cap the Vector so the AI builds something else" actually needs.
	 *
	 * Set UnitCapClass to the unit this ability spawns. Subclasses count too, so pointing it at a
	 * parent Blueprint caps a whole family on purpose. MaxUnitsOfType 0 (default) = no cap, and no
	 * cap is applied while UnitCapClass is unset - existing content keeps its behaviour.
	 *
	 * Deliberately NOT in CheckCost: CommitAbility calls that a second time, and Blueprints such as
	 * GA_BuildUnit_Parent_AH enter the Casting state BEFORE they commit. A refusal at commit would
	 * strand the unit in Casting, and its own "UnitState == Casting -> EndAbility" gate would then
	 * block every later press for good.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cap")
	TSubclassOf<class AUnitBase> UnitCapClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cap", meta = (ClampMin = "0"))
	int32 MaxUnitsOfType = 0;

	/** True when the team already holds MaxUnitsOfType units of UnitCapClass. False when no cap is set. */
	bool IsUnitTypeCapReached(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString KeyboardKey = "X";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText ToolTipText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Description;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateTooltipText();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int ClickCount = 0;

	// ================================================================================================
	// LUX-ANPASSUNG (28.08.2026) - Klick beim Zielen gehoert der zielenden Faehigkeit.
	// Muss beim Uebernehmen ins Original-Template mitwandern. Siehe REAPPLY_AFTER_PLUGIN_SWAP.md.
	// ================================================================================================
	/**
	 * Ein Klick, waehrend der Ziel-Indikator DIESER Faehigkeit steht, zaehlt fuer sie weiter
	 * (ClickCount++ ueber AGASUnit::FireMouseHitAbility), statt eine neue Faehigkeit zu starten.
	 *
	 * Gebraucht wird das nur von der direkt gesteuerten CameraUnit: dort loest der Linksklick
	 * AbilityOne aus (den Schuss). Ohne diesen Schalter startet ein Klick waehrend des Zielens
	 * also den Schuss, statt das Wurfziel zu bestaetigen. Im normalen RTS-Betrieb macht
	 * AControllerBase::LeftClickSelect genau das schon von sich aus (IsAnyAbilityActive ->
	 * FireAbilityMouseHit); allein der Direktsteuerungs-Pfad ging daran vorbei.
	 *
	 * Das Flag gehoert an die Faehigkeit MIT dem Indikator (z. B. die Granate), nicht an die,
	 * die sonst ausgeloest wuerde. Aus (Vorgabe) heisst: unveraendertes Verhalten.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIndicatorClicksAdvanceAbility = false;
	// ===================== ENDE LUX-ANPASSUNG =======================================================

	// ================================================================================================
	// LUX-ANPASSUNG (28.08.2026) - Montage aus einer Faehigkeit abspielen, repliziert.
	// Muss beim Uebernehmen ins Original-Template mitwandern. Siehe REAPPLY_AFTER_PLUGIN_SWAP.md.
	// ================================================================================================
	/**
	 * Spielt eine Montage auf dem Traeger dieser Faehigkeit - und zwar auf ALLEN Maschinen.
	 *
	 * Bewusst ueber UAbilitySystemComponent::PlayMontage statt ueber ein eigenes Multicast-Event:
	 * GAS fuehrt die laufende Montage in RepAnimMontageInfo mit und repliziert sie von sich aus an
	 * jeden Client, der die Einheit sieht (dort OnRep_ReplicatedAnimMontage). Ein handgebauter
	 * Multicast wuerde dasselbe noch einmal daneben tun und bei spaeter dazukommenden Clients
	 * nichts nachholen - die Replikation ueber GAS ist ein Zustand, ein Multicast nur ein Ereignis.
	 *
	 * Nur auf der Autoritaet aufrufen (Faehigkeiten laufen dort ohnehin); auf einem Client
	 * passiert nichts. Ein erneuter Aufruf loest die vorige Montage im selben Slot ab - genau so
	 * ist der Wechsel vom Ziel- auf das Wurf-Bild gedacht.
	 *
	 * @return Laenge der gestarteten Montage in Sekunden, 0 wenn nichts gestartet wurde.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float PlayMontageOnAvatar(class UAnimMontage* Montage, float PlayRate = 1.f, FName StartSection = NAME_None);

	/** Blendet eine ueber PlayMontageOnAvatar gestartete Montage wieder aus. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopMontageOnAvatar(float BlendOutTime = 0.25f);
	// ===================== ENDE LUX-ANPASSUNG =======================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bUseCastingFallbackProcessor = false;

	/**
	 * Beim Wechsel ins Casting aufhoeren, sich zur Maus zu drehen.
	 *
	 * An (Vorgabe): waehrend des Zielens dreht sich die Einheit zur Maus; sobald der Cast beginnt,
	 * bleibt sie in der zuletzt eingeschlagenen Richtung stehen. Nebeneffekt, der so gewollt ist:
	 * ohne FMassRotateToMouseTag leitet UnitClientTagSyncProcessor den Zustand als Casting statt Aim
	 * ab - erst dadurch wird die Cast-Leiste ueberhaupt sichtbar, die am Zustand haengt.
	 *
	 * Aus: die Einheit dreht sich waehrend des Casts weiter mit der Maus (altes Verhalten).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bStopRotateToMouseWhileCasting = true;

	/**
	 * True zwischen AddCastingFallback und dem Ende der Ability.
	 *
	 * Laufzeitwert, keine Einstellung - deshalb Transient und nicht EditAnywhere.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = RTSUnitTemplate)
	bool bBlueprintCastActive = false;

	/**
	 * Meldet dem Cast-Waechter, dass AB JETZT ein echter Cast dieser Ability laeuft.
	 *
	 * Gedacht fuer Abilities, die ihren Cast SELBST im Blueprint starten - typischerweise erst
	 * laufen, dann casten (GA_Mine_AH). Ohne diese Meldung sieht AGASUnit::EnforceCastingInvariant
	 * eine Einheit im Casting-Zustand ohne zugehoerige Cast-Ability, wertet das als verwaisten Cast
	 * und loest ihn nach zwei Takten (~0,5 s) wieder auf - der Cast starb reproduzierbar bei rund
	 * 20 Prozent.
	 *
	 * Warum nicht einfach bUseCastingFallbackProcessor setzen: dieses Flag wird schon bei der
	 * AKTIVIERUNG ausgewertet und laesst die Ability sofort casten, statt die Einheit erst laufen zu
	 * lassen. Es ist eine Einstellung fuer den ganzen Ablauf, nicht fuer einen Zeitpunkt. Und die
	 * Ability-Instanz ueberlebt bei InstancedPerActor die Aktivierung, ein dauerhaft gesetztes Flag
	 * wuerde also ab dem zweiten Einsatz genau dieses Sofort-Casten ausloesen.
	 *
	 * Wird beim Ende der Ability automatisch zurueckgenommen. RemoveCastingFallback gibt es fuer den
	 * Fall, dass das Blueprint den Cast vorzeitig selbst abbricht.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddCastingFallback();

	/** Gegenstueck zu AddCastingFallback - der Cast dieser Ability laeuft nicht mehr. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveCastingFallback();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Range = 0.f;

	// Enable/Disable abilities by AbilityKey for the owner team (uses Owner from ActorInfo)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilitiesEnabledForTeamByKey(const FString& Key, bool bEnable);

	// Enable/Disable only this owner's ability by key (call from within an ability instance)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilityEnabledByKey(const FString& Key, bool bEnable);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilityEnabledByKeyForUnit(AUnitBase* Unit, const FString& Key, bool bEnable);
		
	// Static helpers to toggle/query by key/team id
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void SetAbilitiesEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bEnable);
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyDisabledForTeam(const FString& Key, int32 TeamId);

	// Force-enable abilities by AbilityKey for the owner team (overrides bDisabled and disabled-by-key)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilitiesForceEnabledForTeamByKey(const FString& Key, bool bForceEnable);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void SetAbilitiesForceEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bForceEnable);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyForceEnabledForTeam(const FString& Key, int32 TeamId);

	/**
	 * Resolves whether the enable/disable key gate is open for an ability, with the precedence:
	 *   1) owner force-enable  2) owner disable  3) team force-enable  4) asset bDisabled  5) team disable
	 *
	 * This is the ONLY place that precedence is expressed. CanActivateAbility() delegates to it, and any
	 * code that needs to know "can this ability be used?" outside of GAS activation (the mouse-follow
	 * ability indicator, ability buttons) must use it too. Reading bDisabled directly is a bug: an
	 * ability may be force-enabled for the team/owner at runtime (that is how research unlocks work),
	 * in which case the asset flag is still true but the ability is perfectly usable.
	 *
	 * Does NOT check cost/cooldown/tags -- that is Super::CanActivateAbility's job.
	 */
	static bool IsAbilityKeyGateOpen(const UGameplayAbilityBase* AbilityCDO, int32 TeamId, const class UAbilitySystemComponent* OwnerASC);

	/** IsAbilityKeyGateOpen for callers that have a unit rather than resolved ActorInfo. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = RTSUnitTemplate)
	static bool IsAbilityKeyGateOpenForUnit(const UGameplayAbilityBase* AbilityCDO, const AGASUnit* Unit);

	// Owner-level queries (per AbilitySystemComponent)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyDisabledForOwner(class UAbilitySystemComponent* OwnerASC, const FString& Key);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyForceEnabledForOwner(class UAbilitySystemComponent* OwnerASC, const FString& Key);

	// Apply owner-scoped ability key toggle on the local machine (client/UI side)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void ApplyOwnerAbilityKeyToggle_Local(class UAbilitySystemComponent* OwnerASC, const FString& Key, bool bEnable);

	// Apply team-scoped ability key toggle on the local machine (client/UI side)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void ApplyTeamAbilityKeyToggle_Local(int32 TeamId, const FString& Key, bool bEnable);

	// Returns true if this exact ability BP class was ever executed in this play session
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool WasThisAbilityClassExecuted() const;

	// Returns true if the given ability BP class was ever executed in this play session
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool WasAbilityClassExecuted(TSubclassOf<UGameplayAbilityBase> AbilityClass);

	// Upgrade units by applying a Gameplay Effect to all units of a certain team and matching a specific tag.
	// Also ensures that future units matching these conditions will receive the effect upon spawning.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void UpgradeUnits(TSubclassOf<class UGameplayEffect> UpgradeEffect, int32 TeamId, FGameplayTag Tag);

	static void ApplyActiveUpgradesToUnit(class AUnitBase* Unit);
		
	// Debug: dump disabled/force-enabled keys per team to log
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void Debug_DumpDisabledAbilityKeys();
	 
protected:
	float ActivationStartTime = 0.f;

private:
	FText CreateTooltipText() const;
};
