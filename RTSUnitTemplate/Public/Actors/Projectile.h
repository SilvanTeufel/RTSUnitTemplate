// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProjectile();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	void Init(AActor* TargetActor, AActor* ShootingActor);

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Target", Keywords = "RTSUnitTemplate Target"), Category = RTSUnitTemplate)
	AActor* Target;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Target", Keywords = "RTSUnitTemplate Target"), Category = RTSUnitTemplate)
	AActor* Shooter;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* Material;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector TargetLocation;

	UPROPERTY(Replicated, EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	float Damage;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LifeTime = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxLifeTime = 2.f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MovementSpeed = 50.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(Server, Reliable)
	void Impact(AActor* ImpactTarget);
	
	UFUNCTION(Server, Reliable)
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void ImpactEvent();

	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void DestroyProjectile();

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DestructionDelayTime = 0.1f;
};
