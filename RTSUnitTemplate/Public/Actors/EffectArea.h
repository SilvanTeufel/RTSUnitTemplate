// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassVisibilityInterface.h"
#include "EffectArea.generated.h"

class UMassActorBindingComponent;
class UInstancedStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEffectAreaDead);

UCLASS()
class RTSUNITTEMPLATE_API AEffectArea : public AActor, public IMassVisibilityInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEffectArea();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;
	

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_A;
	
	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|Events")
	FOnEffectAreaDead OnEffectAreaDead;

	UFUNCTION(BlueprintImplementableEvent, Category = "Mass")
	void OnMassRegistrationFinished();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Health = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Effects")
	class UNiagaraSystem* DeathVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Effects")
	class USoundBase* DeathSound;

	void HandleDeath(bool bIsVisible);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float LifeTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxLifeTime = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	bool bUseEffectAreaImpactProcessor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	float StartRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	float EndRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	float TimeToEndRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	bool ScaleMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	bool bIsRadiusScaling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	float BaseRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	bool bPulsate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	bool bDestroyOnImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass", meta = (EditCondition = "bUseEffectAreaImpactProcessor"))
	bool bScaleOnImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	float DuplicationRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	float DuplicationTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	float RandomAngleRange = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	FVector LastDuplicationDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	int32 MaxDuplicationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CapsuleHeight = 50.f;

	UFUNCTION(Server, Reliable)
	void HandleProjectileImpact(AActor* Shooter, const FVector& ImpactLocation, TSubclassOf<class AProjectile> ProjectileClass, float DamageOverride = -1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Duplication")
	int32 DuplicationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	UInstancedStaticMeshComponent* ISMTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	class UNiagaraSystem* ImpactVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	class USoundBase* ImpactSound;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	bool bImpactVFXTriggered = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	bool bIsScalingAfterImpact = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Mass")
	bool bImpactScaleTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn")
	TSubclassOf<class AUnitBase> SpawnClassOnDestruction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn", meta = (ClampMin = "1"))
	int32 SpawnCountOnDestruction = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn")
	bool bSpawnDoGroundTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn")
	float SpawnVerticalOffset = 0.f;

	// Random XY offset applied per spawned unit (radius in cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn", meta=(ClampMin="0.0"))
	float SpawnRandomOffsetMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn", meta=(ClampMin="0.0"))
	float SpawnRandomOffsetMax = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Spawn")
	float EarlySpawnTime = 1.0f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "RTSUnitTemplate|Shutdown")
	bool bPendingDestructionRep = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate")
	void OnEffectAreaDestructionStarted();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Visuals")
	virtual void SetDeathVisualState(bool bShouldHide);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	float VisibilityOffset = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	bool bAffectedByFogOfWar = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	bool bInvisibleToEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	bool bIsInvisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Visibility")
	bool bCanBeInvisible = false;

	UPROPERTY(Transient)
	bool bIsVisibleByFog = false;

	bool bLocalDeathEffectsExecuted = false;

	UPROPERTY()
	UNiagaraComponent* DeathNiagaraComp = nullptr;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectOne;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectTwo;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectThree;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsHealing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, Replicated)
	float BeaconRange = 0.f;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBeaconRange(float NewRange);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInBeaconRange() const;

	// Returns true if the location is within range of any Beacon (Building or EffectArea)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate, meta = (WorldContext = "World"))
	static bool IsLocationInBeaconRange(UWorld* World, const FVector& Location);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	UMassActorBindingComponent* MassBindingComponent;

	// IMassVisibilityInterface
	virtual void SetActorVisibility(bool bVisible) override;
	virtual void SetEnemyVisibility(AActor* DetectingActor, bool bVisible) override;
	virtual bool ComputeLocalVisibility() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent(AUnitBase* Unit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
};
