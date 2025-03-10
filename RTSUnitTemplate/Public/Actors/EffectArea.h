// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"
#include "EffectArea.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AEffectArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEffectArea();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	FTimerHandle DamageTimerHandle;

	FTimerHandle ScaleTimerHandle;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ScaleMesh();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetScaleTimer();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_A;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float LifeTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxLifeTime = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsGettingBigger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BiggerScaler = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BiggerScaleInterval = 1.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectOne;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectTwo;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffectThree;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsHealing = false;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent(AUnitBase* Unit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
};
