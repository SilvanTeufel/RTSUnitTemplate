// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdAgentInterface.h"
#include "Navigation/CrowdFollowingComponent.h"
//#include "MassUnitBase.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "WorkingUnitBase.h"
#include "Actors/Projectile.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "UnitBase.generated.h"

class UBoxComponent;

UCLASS()
class RTSUNITTEMPLATE_API AUnitBase : public AWorkingUnitBase
{
	GENERATED_BODY()
	
public:
	
	// Unit this unit will follow when assigned via PlayerController right-click on a friendly unit.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* FollowUnit = nullptr;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int64 AlliedTeamsMask = 0;
	
	static const FName BoxCollisionTag;

	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	UBoxComponent* BoxCollisionComponent = nullptr;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanMove = true;

	// Opt-in (default OFF): run the Mass animation processor for this actor EVEN when it is stationary
	// (CanMove=false / StopMovement, e.g. a building). Independent of CanMove — the building stays put but
	// animates. Requires per-actor anim CONTENT to show motion (ISMAnimationDataTable etc.).
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanAnimate = false;

	UPROPERTY()
	TObjectPtr<AActor> NavObstacleProxy;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float NavObstaclePadding = 5.0f;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RegisterBuildingAsObstacle();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UnregisterObstacle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bIsBuilding = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bIsConstructionUnit = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UTexture2D* UnitIcon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Name = "Unit";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Description;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool IsPlayer = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool CanActivateAbilities = true;
	
	AUnitBase(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool ControlUnitIntoMouseDirection = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TickInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CollisionCooldown = 3.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CollisionUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector CollisionLocation;

protected:
// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/**
	 * Nur fuer die Messung: Gegenstueck zu [EinheitAuf] in BeginPlay.
	 * Destroyed() feuert garantiert genau EINMAL je Actor - anders als der
	 * Signalweg ueber UUnitStateProcessor::HandleStartDead, der in Runde 120
	 * fuer EIN totes Gebaeude ueber tausendmal ausgeloest hat und damit
	 * Signalaufrufe statt Tode zaehlte. Vorbild ist ABuildingBase::Destroyed.
	 */
	virtual void Destroyed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Visuals")
	virtual void SetDeathVisualState(bool bShouldHide);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator ServerMeshRotation = FRotator(0.f, -90.f, 0.f);
	
	UPROPERTY(Replicated, BlueprintReadWrite, ReplicatedUsing=OnRep_MeshAssetPath, Category = RTSUnitTemplate)
	FString MeshAssetPath;

	UPROPERTY(Replicated, BlueprintReadWrite, ReplicatedUsing=OnRep_MeshMaterialPath, Category = RTSUnitTemplate)
	FString MeshMaterialPath;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnRep_MeshAssetPath();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnRep_MeshMaterialPath();

	float GetCollisionRadiusInDirection(const FVector& Direction) const;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetMeshRotationServer();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool CanOnlyAttackGround = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool CanOnlyAttackFlying = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool CanDetectInvisible = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool CanAttack = true;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool bHoldPosition = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool bCanBeInvisible = false;

	/**
	 * Nimmt keinen Schaden.
	 *
	 * Vorgabe AUS, und zwar fuer jede Einheit - das ist ein Werkzeug fuer Faehigkeiten, kein
	 * Dauerzustand. Der Wert wird repliziert und ausserdem ins Mass-Fragment gespiegelt
	 * (FMassAgentCharacteristicsFragment::bIsInvulnerable).
	 *
	 * Der Waechter sitzt in UAttributeSetBase::PostGameplayEffectExecute, NICHT in
	 * AUnitBase::SetHealth. Gemessen am 30.08.: der gesamte Kampfschaden laeuft ueber
	 * SetAttributeHealth und ruft SetHealth nie auf - ein Waechter dort wird schlicht umgangen.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool bIsInvulnerable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UEPathfindingUsed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UsingUEPathfindingPatrol = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool SetUEPathfinding = true;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		//bool ReCalcRandomLocation = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AutoSetUEPathfindingTimer = 0.f;
	
// RTSHud related //////////////////////////////////////////
public:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartAttackEvent();

	UFUNCTION(NetMulticast, Reliable)
	void MultiCastStartAttackEvent();
	
	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void StartAttackEvent();
	
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerMeeleImpactEvent();

	UFUNCTION(NetMulticast, Reliable)
	void MultiCastMeeleImpactEvent();
	
	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void MeeleImpactEvent();
	
	// Called when this unit has been attacked by another actor (attacker can be nullptr)
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void Attacked(AActor* AttackingActor);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void IsAttacked(AActor* AttackingCharacter); // AActor* SelectedCharacter

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetRunLocation(FVector Location);

	// ---- Flying-building helpers (server-authoritative building blocks; compose these in a BP ability) ----
	// Take off: FlyHeight=InFlyHeight, IsFlying=true, CanMove=true, drop the navmesh obstacle + StopMovement
	// freeze so the building rises (HandleGroundAndHeight interps Z up) and can move.
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void StartBuildingFlight(float InFlyHeight = 500.f);

	// Move this unit to a world location via the Mass mover (Run). Ideal for flyers (skips navmesh).
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void MoveUnitToLocation(FVector WorldLocation, float MoveSpeed = 600.f, float AcceptanceRadius = 50.f);

	// Begin a smooth landing: IsFlying=false so the unit interps down to the ground (keeps CanMove=true).
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void BeginLanding();

	// Finish landing: re-freeze (CanMove=false + StopMovement) and optionally re-carve the navmesh obstacle.
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void FinishLanding(bool bReRegisterObstacle = true);

	// One-call: take off, fly to WorldLocation, and auto-land there once the unit's X/Y arrives.
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void FlyUnitToLocationAndLand(FVector WorldLocation, float InFlyHeight = 500.f, float MoveSpeed = 600.f, float AcceptanceRadius = 60.f, float DescendTime = 2.5f);

	// True when this unit's X/Y is within AcceptanceRadius of WorldLocation (altitude-agnostic).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Flight")
	bool IsUnitAtLocation2D(FVector WorldLocation, float AcceptanceRadius = 60.f) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Flight")
	bool IsUnitFlying() const { return IsFlying; }

private:
	// Internal auto-land lifecycle state for FlyUnitToLocationAndLand.
	FVector FlyLandTarget = FVector::ZeroVector;
	float FlyLandAcceptance = 60.f;
	float FlyLandDescendTime = 2.5f;
	FTimerHandle FlyLandArrivalTimer;
	FTimerHandle FlyLandDescendTimer;
	void PollFlyArrival();
	void FinishLandingDefault();
public:

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWalkSpeed(float Speed);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float MovementAcceptanceRadius = 50.f;

	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	float LastVisibleTime = -100.f;
///////////////////////////////////////////////////////////////////

// related to Animations  //////////////////////////////////////////
public:
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "UnitToChase", Keywords = "RTSUnitTemplate UnitToChase"), Category = RTSUnitTemplate)
	AActor* UnitToChase = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "UnitsToChase", Keywords = "RTSUnitTemplate UnitsToChase"), Category = RTSUnitTemplate)
	TArray <AActor*> UnitsToChase;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextUnitToChase", Keywords = "RTSUnitTemplate SetNextUnitToChase"), Category = RTSUnitTemplate)
	bool SetNextUnitToChase();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "DistanceToUnitToChase", Keywords = "RTSUnitTemplate DistanceToUnitToChase"), Category = RTSUnitTemplate)
	float DistanceToUnitToChase;
///////////////////////////////////////////////////////

// related to Waypoints  //////////////////////////////////////////
public:
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "NextWaypoint", Keywords = "RTSUnitTemplate NextWaypoint"), Category = RTSUnitTemplate)
	class AWaypoint* NextWaypoint = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = RTSUnitTemplate)
	class AWaypoint* GetNextWaypoint() const;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetWaypoint", Keywords = "RTSUnitTemplate SetWaypoint"), Category = RTSUnitTemplate)
	void SetWaypoint(class AWaypoint* NewNextWaypoint);

	/**
	 * Diese Einheit soll nicht herumstehen; gesetzt aus FUnitSpawnParameter::bPreventIdling.
	 *
	 * Fuer sich genommen wirkungslos - erst ein AIdlePatrolEnforcer im Level liest das
	 * Flag und schickt die Einheit aus PatrolIdle/Idle zurueck auf Patrouille. Das Flag
	 * sitzt an der Einheit statt am Actor, damit pro Spawn-Zeile entschieden werden kann.
	 *
	 * Bewusst nicht repliziert: der Enforcer laeuft nur auf dem Server.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bPreventIdling = false;
///////////////////////////////////////////////////////////////////



// related to Healthbar  //////////////////////////////////////////
public:

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void UnitWillDespawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DeadEffectsExecuted = false;
	
	virtual void SetHealth_Implementation(float NewHealth) override;

	// Fires when health crosses 25% or 50% thresholds either upwards or downwards.
	// DidIncrease: true if health rose above the threshold, false if it dropped below it.
	// LowThreshold: true if 25% threshold is concerned.
	// HighThreshold: true if 50% threshold is concerned.
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnHealthThresholdCrossed(bool DidIncrease, bool LowThreshold, bool HighThreshold, float NewHealth);
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void DeadMultiCast();

	// Multicast to switch this unit to Idle state on all clients
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SwitchToIdle();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DeadEffectsAndEvents();
	
	virtual void SetShield_Implementation(float NewShield) override;
	virtual void SetMana_Implementation(float NewMana) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitHealthbarOwner();
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void EnsureSquadHealthbarState();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = RTSUnitTemplate)
	bool IsSquadHealthbarOwner() const;
	
	void OnAttributeChanged(const struct FOnAttributeChangeData& Data);

	// ---- Shield-impact effect (optional, client + server) ---------------------------------------
	/** When enabled, ShieldImpactMaterial flashes on the mesh whenever incoming damage is absorbed
	 *  by Shield (i.e. Shield decreases). Purely visual; each machine triggers its own flash because
	 *  the Shield attribute change fires on both server (GE execute) and client (OnRep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|ShieldImpact")
	bool bEnableShieldImpactEffect = false;

	/** Overlay (Surface-domain) material shown on the mesh when Shield absorbs damage. A scalar
	 *  parameter (ShieldImpactTimeParam) is set to the world hit-time so the material can flash+fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|ShieldImpact")
	TObjectPtr<UMaterialInterface> ShieldImpactMaterial = nullptr;

	/** How long (s) the shield-impact overlay stays before it is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|ShieldImpact")
	float ShieldImpactDuration = 0.6f;

	/** Scalar parameter set to the hit time (world seconds) so the material drives its own flash/fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|ShieldImpact")
	FName ShieldImpactTimeParam = FName("HitTime");

	/** Flashes the shield-impact overlay now (local/visual only). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|ShieldImpact")
	void TriggerShieldImpact();

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> ShieldImpactMID = nullptr;

	FTimerHandle ShieldImpactTimerHandle;
	void ClearShieldImpact();
///////////////////////////////////////////////////////////////////


// HUDBase related ///////////
public:
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateSquadHealthBar();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void IncreaseExperience();
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void Selected();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void Deselected();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSelected();

	UFUNCTION(BlueprintCallable,  Category = RTSUnitTemplate)
	void SetDeselected();

	UPROPERTY(BlueprintReadWrite,  Category = RTSUnitTemplate)
	TArray <FVector> RunLocationArray;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 RunLocationArrayIterator = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RunLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RandomPatrolLocation;

	/////////////////////////////
	
	// Projectile related /////////
public:

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SpawnProjectile", Keywords = "RTSUnitTemplate SpawnProjectile"), Category = RTSUnitTemplate)
	void SpawnProjectile(AActor* Target, AActor* Attacker);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnProjectileFromClass(AActor* Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, bool FollowTarget, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, bool DisableAutoZOffset, float ZOffset, float Scale = 1.f, FVector SpawnOffset = FVector(0.f, 0.f, 0.f));

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SpawnProjectileFromClassWithAim", Keywords = "RTSUnitTemplate SpawnProjectileFromClassWithAim"), Category = RTSUnitTemplate)
	void SpawnProjectileFromClassWithAim(FVector Aim, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, float ZOffset, float Scale = 1.f, FVector SpawnOffset = FVector(0.f, 0.f, 0.f), float ExtraDamage = 0.f, TSubclassOf<class UGameplayEffect> NewEffect = nullptr, TSubclassOf<class UGameplayEffect> NewEffect2 = nullptr, TSubclassOf<class UGameplayEffect> NewEffect3 = nullptr, FEffectAreaInfo AreaInfo = FEffectAreaInfo());

    /** Version that accepts Mass Entity handles for direct registration */
    void SpawnProjectileWithEntities(AActor* Target, AActor* Attacker, FMassEntityHandle ShooterEntity = FMassEntityHandle(), FMassEntityHandle TargetEntity = FMassEntityHandle());

	void IncrementMassProjectileFireCounter(TSubclassOf<class AProjectile> ProjectileClass, float Speed, FMassEntityHandle ShooterEntity = FMassEntityHandle(), FMassEntityHandle TargetEntity = FMassEntityHandle(),
		float InitialAngle = 0.f, float RotSpeed = 0.f, float MaxRadius = 0.f, float InterpSpeed = 0.f, bool bFollow = false, FVector TargetLocation = FVector::ZeroVector, FVector Scale = FVector::OneVector, float Spread = 0.f, float Damage = -1.f, int32 MaxPiercedTargets = -1,
		int32 ProjectileCount = 1, bool IsBouncingNext = false, bool IsBouncingBack = false, float ZOffset = 0.f, FVector SpawnOffset = FVector::ZeroVector, bool DisableAutoZOffset = false, float TwinProjectileDistance = 0.f, TSubclassOf<class UGameplayEffect> ProjectileEffect = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect2 = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect3 = nullptr, FEffectAreaInfo AreaInfo = FEffectAreaInfo());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
   	void HandleProjectileImpact(AActor* Shooter, const FVector& ImpactLocation, TSubclassOf<class AProjectile> ProjectileClass, float DamageOverride = -1.f, TSubclassOf<class UGameplayEffect> ProjectileEffect = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect2 = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect3 = nullptr);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RTSUnitTemplate")
	void HandleEffectAreaImpact(float Damage, bool IsHealing, TSubclassOf<class UGameplayEffect> Effect1, TSubclassOf<class UGameplayEffect> Effect2, TSubclassOf<class UGameplayEffect> Effect3);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Spawn")
	void SpawnEffectArea(int InTeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn);
	
	/** 
	 * Returns the world location for spawning projectiles.
	 * Checks for a USceneComponent with the tag "ProjectileSpawn" first.
	 * Falls back to standard offset logic if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Projectile")
	FVector GetProjectileSpawnLocation(const FVector& AdditionalOffset = FVector::ZeroVector) const;


	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, meta = (DisplayName = "UseProjectile", Keywords = "RTSUnitTemplate UseProjectile"), Category = RTSUnitTemplate)
	bool UseProjectile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileSpawnOffset", Keywords = "RTSUnitTemplate ProjectileSpawnOffset"), Category = RTSUnitTemplate)
	FVector ProjectileSpawnOffset = FVector(0.f,0.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileScale", Keywords = "RTSUnitTemplate ProjectileScale"), Category = RTSUnitTemplate)
	FVector ProjectileScale = FVector(1.f,1.f,1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator ProjectileRotationOffset = FRotator(0.f,90.f,-90.f);
	//////////////////////////////////////
	
	
// Used for Despawn  //////////////////////////////////////////
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DestroyAfterDeath", Keywords = "RTSUnitTemplate DestroyAfterDeath"), Category = RTSUnitTemplate)
		bool DestroyAfterDeath = true;
///////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, meta = (DisplayName = "PauseDuration", Keywords = "RTSUnitTemplate PauseDuration"), Category = RTSUnitTemplate)
		float PauseDuration = 0.6f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AttackDuration = 0.6f;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float CastTime = 5.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ReduceCastTime = 0.5f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ReduceRootedTime = 0.1f;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		//FVector TimerWidgetCompLocation = FVector (0.f, 0.f, -180.f);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetupTimerWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTimerWidgetCastingColor(FLinearColor Color);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	TArray<FUnitSpawnData> SummonedUnitsDataSet;
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	TArray<AUnitBase*> SpawnUnitsFromParameters(
		TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
		TEnumAsByte<UnitData::EState> UState,
		TEnumAsByte<UnitData::EState> UStatePlaceholder,
		int NewTeamId, FBuildingCost UsedConstructionCost, AWaypoint* Waypoint = nullptr, int UnitCount = 1, bool SummonContinuously = true, bool SpawnAsSquad = true, bool UseSummonDataSet = false, bool bSelectable = true,
		bool bDoGroundTrace = true, float WaypointDirectionOffset = 50.f, FVector OffsetLocation = FVector(0.f, 0.f, 0.f));

	// Applies/clears a follow target for this single unit on the server and updates the Mass AI fragment flag.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ApplyFollowTarget(AUnitBase* NewFollowTarget);

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool IsSpawnedUnitDead(int UIndex);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	int32 GetAliveUnitsInDataSet();
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetUnitBase(int UIndex, AUnitBase* NewUnit);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnPlattform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsDragged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float EnergyCost = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost;

	/**
	 * Supply a unit that is already standing on the map when the match starts occupies.
	 *
	 * A trained unit pays supply through its production ability, but a level-placed one never pays
	 * anything - so a faction that starts with an army got it for free and its cap only ever counted
	 * what it built afterwards. Charged once on BeginPlay, server-side, for level-placed units only.
	 *
	 * Zero falls back to UnitSpaceNeeded, which every unit already authors (worker 1, heavy unit 4),
	 * so this takes effect on existing content without hand-filling it everywhere. Note the fallback
	 * is a size proxy, not the real training cost - set this explicitly where the two should match.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	int32 StartupSupplyCost = 0;

	/** Charges StartupSupplyCost against the team's supply. Level-placed units only, server only. */
	void ApplyStartupSupplyCost();

	/**
	 * Skips the death VFX/sound for this unit.
	 *
	 * The Xeno worker is killed at the moment its building finishes - it is hidden by then, but the
	 * death effects still played at its position, which reads as a unit exploding for no reason.
	 */
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bSuppressDeathEffects = false;

	/** Kills the unit without the death effects. Use instead of SetHealth(0) where the death is cosmetic. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void KillSilently();

	/** Sets the suppression flag on every machine before the death multicast, so clients skip it too. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSuppressDeathEffects();

	/** Guards against charging twice (BeginPlay can run more than once for a pooled actor). */
	bool bStartupSupplyCharged = false;

	/**
	 * Hands this unit's supply footprint back to its team. Only buildings released capacity before, so
	 * every fallen unit kept blocking supply forever: a faction that loses fights ran into its own cap
	 * and could not even train workers any more (measured 57 used against ~30 built capacity).
	 */
	void ReleaseUnitSupply();

	/** Guards against releasing twice - SetHealth can be entered again after death. */
	bool bSupplyReleased = false;

	/**
	 * What this unit actually paid in supply, so death gives back exactly that.
	 * Releasing UnitSpaceNeeded instead was only an estimate and could drive the team's used supply
	 * negative when the two differ. 0 means "never charged here" (trained units pay through their
	 * build ability), and then UnitSpaceNeeded is still the best available footprint.
	 */
	int32 ChargedSupplyAmount = 0;

	/**
	 * True once ChargedSupplyAmount holds the amount that was really billed - zero included.
	 *
	 * Without this flag a charge of zero was indistinguishable from "never recorded", and the
	 * refund fell back to UnitSpaceNeeded. That gap is what let the used supply go negative.
	 */
	UPROPERTY(Transient)
	bool bSupplyAmountKnown = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ScheduleDelayedNavigationUpdate();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateUnitNavigation();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilityEnabledByKey(const FString& Key, bool bEnable);

	UPROPERTY()
	float DisplayedHealthPct = -1.f;

	UPROPERTY()
	float DisplayedShieldPct = -1.f;

	UPROPERTY()
	float DisplayedManaPct = -1.f;
};








