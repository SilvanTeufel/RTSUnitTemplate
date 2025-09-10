// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Projectile.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AProjectile : public AActor
{
	GENERATED_BODY()

private:
	// Timer to control the frequency of the overlap check
	float OverlapCheckTimer = 0.0f;
public:
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = RTSUnitTemplate)
	float OverlapCheckInterval = 0.1f;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool DebugTargetLocation = false;
	
	// Sets default values for this actor's properties
	AProjectile();

	// OnConstruction is the ideal place to create the instance
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void InitISMComponent(FTransform Transform);
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = RTSUnitTemplate)
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(Replicated)
	int32 InstanceIndex;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TickInterval = 0.025f;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//FVector ScaleISM = FVector(0.25, 0.25, 0.25);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FlightDirection;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsInitialized = false;;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void Init(AActor* TargetActor, AActor* ShootingActor);

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void InitForAbility(AActor* TargetActor, AActor* ShootingActor);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category=RTSUnitTemplate)
	void SetProjectileVisibility();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void InitForLocationPosition(FVector Aim, AActor* ShootingActor);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool UseAttributeDamage = true;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Target", Keywords = "RTSUnitTemplate Target"), Category = RTSUnitTemplate)
	AActor* Target;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* Shooter;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector TargetLocation;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ShooterLocation;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator RotationOffset = FRotator(0.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityOffset = 0.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool FollowTarget = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int MaxPiercedTargets = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	int PiercedTargets = 0;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateMesh = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RotationSpeed = FVector(0.5f);
	
	UFUNCTION(NetMulticast, Reliable, Category = RTSUnitTemplate)
	void Multicast_UpdateISMTransform(const FTransform& NewTransform);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_A;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_A_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_B;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_B_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraSystem* ImpactVFX;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* ImpactSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ScaleImpactVFX = FVector(1.f,1.f,1.f);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ScaleImpactSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	float Damage;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LifeTime = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxLifeTime = 2.f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MovementSpeed = 1000.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> ProjectileEffect;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsHealing = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsBouncingBack = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsBouncingNext = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool BouncedBack = false;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = true;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckViewport();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInViewport(FVector WorldPosition, float Offset);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CollisionRadius = 0.f;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void FlyToUnitTarget();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void FlyToLocationTarget(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate) //UFUNCTION(Server, Reliable)
	void Impact(AActor* ImpactTarget);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ImpactHeal(AActor* ImpactTarget);
	
	UFUNCTION(Server, Reliable)
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroyWhenMaxPierced();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroyProjectileWithDelay();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroyProjectile();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetIsAttacked(AUnitBase* UnitToHit);
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DestructionDelayTime = 0.1f;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBackBouncing(AUnitBase* ShootingUnit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetNextBouncing(AUnitBase* ShootingUnit, AUnitBase* UnitToHit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* GetNextUnitInRange(AUnitBase* ShootingUnit, AUnitBase* UnitToHit);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetVisibility(bool Visible);
};
