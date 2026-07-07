// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
class AUnitBase;
class AEffectArea;

#include "Projectile.generated.h"

USTRUCT(BlueprintType)
struct FEffectAreaInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float Radius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<class UGameplayEffect> Effect1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<class UGameplayEffect> Effect2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<class UGameplayEffect> Effect3 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float Amount = 0.f;
};

UCLASS()
class RTSUNITTEMPLATE_API AProjectile : public AActor
{
	GENERATED_BODY()

private:
	// Timer to control the frequency of the overlap check
	float OverlapCheckTimer = 0.0f;
public:
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "RTSUnitTemplate")
	float OverlapCheckInterval = 0.1f;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bContinueAfterTarget = true;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "RTSUnitTemplate")
	bool bUseMass = true;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "RTSUnitTemplate")
	bool DebugTargetLocation = false;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Movement")
	float ArcHeight = 0.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Movement")
	float ArcHeightDistanceFactor = 0.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	float TwinProjectileDistance = 0.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	int32 HomingMissleCount = 0;

	// Internal property to handle irregular flight paths for homing missiles
	UPROPERTY(Replicated)
	FVector HomingOffset = FVector::ZeroVector;

	// Speed at which the irregular offset converges to the target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	float HomingInterpSpeed = 2.0f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	float HomingRotationSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	float HomingMaxSpiralRadius = 200.0f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Projectile")
	float HomingSpeedVariation = 50.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RTSUnitTemplate|Projectile")
	float HomingInitialAngle = 0.f;
	// Sets default values for this actor's properties
	AProjectile();

	// OnConstruction is the ideal place to create the instance
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void InitISMComponent(FTransform Transform);
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "RTSUnitTemplate")
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(Replicated)
	int32 InstanceIndex;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float TickInterval = 0.025f;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	//FVector ScaleISM = FVector(0.25, 0.25, 0.25);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	USceneComponent* SceneRoot;

	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector FlightDirection;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bIsInitialized = false;

	UPROPERTY(ReplicatedUsing = OnRep_bImpacted)
	bool bImpacted = false;

	UFUNCTION()
	void OnRep_bImpacted();
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void Init(AActor* TargetActor, AActor* ShootingActor);

	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void InitForAbility(AActor* TargetActor, AActor* ShootingActor);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="RTSUnitTemplate")
	void SetProjectileVisibility();
	
	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void InitForLocationPosition(FVector Aim, AActor* ShootingActor);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool UseAttributeDamage = true;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<AActor>> PiercedActors;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Target", Keywords = "RTSUnitTemplate Target"), Category = "RTSUnitTemplate")
	AActor* Target;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	AActor* Shooter;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector TargetLocation;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector ShooterLocation;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FRotator RotationOffset = FRotator(0.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	float VisibilityOffset = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	bool bAffectedByFogOfWar = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool FollowTarget = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	int MaxPiercedTargets = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite,  Category = "RTSUnitTemplate")
	int PiercedTargets = 0;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool RotateMesh = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool DisableAnyRotation = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector RotationSpeed = FVector(0.5f);
	
	UFUNCTION(NetMulticast, Reliable, Category = "RTSUnitTemplate")
	void Multicast_UpdateISMTransform(const FTransform& NewTransform);
 
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	UNiagaraComponent* Niagara_A;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FTransform Niagara_A_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	UNiagaraComponent* Niagara_B;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FTransform Niagara_B_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	UNiagaraSystem* ImpactVFX;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	USoundBase* ImpactSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector ScaleImpactVFX = FVector(1.f,1.f,1.f);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ScaleImpactSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere,BlueprintReadWrite, Category = "RTSUnitTemplate")
	float Damage;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float LifeTime = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float MaxLifeTime = 2.f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	int TeamId = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float MovementSpeed = 1000.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<UGameplayEffect> ProjectileEffect;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<UGameplayEffect> ProjectileEffect2;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<UGameplayEffect> ProjectileEffect3;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool IsHealing = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bEnableLandscapeHit = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool IsBouncingBack = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool IsBouncingNext = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bCanBeRepelledByEnergyWall = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool BouncedBack = false;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void InitArc(FVector ArcBeginLocation);

	UPROPERTY(Replicated)
	FVector ArcStartLocation;
	FVector PreviousLocation;
	FVector LastWallHitLocation;
	float ArcTotalDistance;
	UPROPERTY(Replicated)
	float ArcTravelTime = 0.f;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool IsOnViewport = true;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckViewport();
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	bool IsInViewport(FVector WorldPosition, float Offset);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float CollisionRadius = 0.f;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void FlyToUnitTarget(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void FlyToLocationTarget(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void FlyInArc(float DeltaTime);
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate") //UFUNCTION(Server, Reliable)
	void Impact(AActor* ImpactTarget);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void ImpactHeal(AActor* ImpactTarget);
	
	UFUNCTION(Server, Reliable)
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate")
	void BeginSpawn(FTransform Transform, UObject* WorldContext);

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate")
	void ImpactEvent(FVector ImpactLocation, UObject* WorldContext, AActor* HitActor, int32 InTeamId);

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate")
	void GroundHit(FVector ImpactLocation, UObject* WorldContext, FEffectAreaInfo AreaInfo, int32 InTeamId);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void DestroyWhenMaxPierced();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void DestroyProjectileWithDelay();
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void DestroyProjectile();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetIsAttacked(AUnitBase* UnitToHit);
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float DestructionDelayTime = 0.1f;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBackBouncing(AUnitBase* ShootingUnit);
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetNextBouncing(AUnitBase* ShootingUnit, AActor* UnitToHit);
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	AActor* GetNextUnitInRange(AUnitBase* ShootingUnit, AActor* UnitToHit);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetVisibility(bool Visible);

    /** Returns the transform of the projectile from Mass if it's bound to an entity */
    UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Projectile")
    FTransform GetMassProjectileTransform(UObject* WorldContext = nullptr) const;

    /** Retrieves a pooled ISM matching the template's mesh, material, and shadow settings */
    UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Projectile")
    UInstancedStaticMeshComponent* GetVisualISM(UInstancedStaticMeshComponent* TemplateISM, UObject* WorldContext = nullptr);

	/** Spawns an EffectArea actor and optionally attaches it to a unit */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Projectile")
	void SpawnEffectArea(UObject* WorldContext, int32 InTeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, FEffectAreaInfo AreaInfo, AUnitBase* ActorToLockOn = nullptr, bool UseAreaInfo = false);

	/**
	 * Instantaneous radial gameplay-effect apply around a point (AoE-on-impact helper).
	 *
	 * SAFE TO CALL FROM ImpactEvent / GroundHit / BeginSpawn: those BlueprintImplementableEvents
	 * fire on the projectile CDO, which has NO world - so DrawDebug / SphereOverlap / SpawnActor
	 * called with 'self' silently fail. This function takes an explicit WorldContextObject: wire
	 * the event's "World Context" output pin into it (do NOT use Self), exactly like SpawnEffectArea.
	 *
	 * Every AUnitBase whose capsule overlaps the sphere is classified against InTeamId:
	 *   - enemy  (different, non-allied team) -> receives EnemyEffect
	 *   - friendly (same team OR allied via UPlayerTeamSubsystem) -> receives FriendlyEffect
	 * Pass a null effect for a side you want to leave untouched. Dead units are skipped (their
	 * actor collision is off, so the overlap can't return them anyway). Gameplay effects apply on
	 * the server only; the optional debug spheres (yellow = area, green = friendly, red = enemy)
	 * draw locally. NOTE: the Mass ImpactEvent fires server-only, so debug drawn from there is
	 * visible on the host/listen-server/standalone but NOT on remote clients.
	 *
	 * @return number of units that actually received an effect (server); 0 on a pure client.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Projectile", meta = (AdvancedDisplay = "bDrawDebug,DebugDuration,DebugThickness"))
	int32 ApplyRadialGameplayEffects(
		UObject* WorldContextObject,
		FVector Center,
		float Radius,
		int32 InTeamId,
		TSubclassOf<UGameplayEffect> EnemyEffect,
		TSubclassOf<UGameplayEffect> FriendlyEffect,
		bool bDrawDebug = false,
		float DebugDuration = 2.0f,
		float DebugThickness = 1.5f);
};
