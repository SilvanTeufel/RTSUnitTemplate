// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
#include "LevelUnit.h"
#include "GameFramework/Character.h"
#include "Core/UnitData.h"
#include "Actors/DijkstraCenter.h"
#include "Actors/FogOfWarManager.h"
#include "Core/DijkstraMatrix.h"
#include "Actors/Projectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "PerformanceUnit.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API APerformanceUnit : public ALevelUnit
{
	GENERATED_BODY()

public:
	APerformanceUnit(const FObjectInitializer& ObjectInitializer);
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraSystem* MeleeImpactVFX;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MeeleImpactVFXDelay = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraSystem* DeadVFX;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ScaleDeadVFX = FVector(1.f, 1.f,1.f);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ScaleImpactVFX = FVector(1.f, 1.f,1.f);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* MeleeImpactSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MeleeImpactSoundDelay = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DeadSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ScaleImpactSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ScaleDeadSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelayDeadVFX = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelayDeadSound = 0.f;
	
	virtual void BeginPlay() override;

	virtual void Destroyed() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	TSubclassOf<AFogOfWarManager> FogOfWarManagerClass;
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SpawnFogOfWarManager(APlayerController* PC);
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void DestroyFogManager();
	/*
	FTimerHandle PlayerControllerRetryHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	float PlayerControllerMaxWaitTime = 60.0f;          // Maximum time to wait for the controller

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	float PlayerControllerRetryInterval = 5.f;        // Retry every 0.1 seconds

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	float PlayerControllerTimeWaited = 0.0f;           // Tracks total wait time
*/
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SetOwningPlayerControllerAndSpawnFogManager();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerMultiplier = FVector(0.01, 0.01, 200);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerPositionOffset = FVector(0, 0, 50.f);
	// Function to update light range

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightRadius", Keywords = "RTSUnitTemplate SightRadius"), Category = RTSUnitTemplate)
	float SightRadius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsMyTeam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnemyStartVisibility = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogSight = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsVisibleEnemy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogDeadVisibilityTime = 10.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnableFog = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int FogManagerOverlaps = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int DetectorOverlaps = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HealthCompCreated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int HideHealthBarUnitCount = 200;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool OpenHealthWidget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HealthBarUpdateTriggered = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* HealthWidgetComp;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector HealthWidgetCompLocation = FVector (0.f, 0.f, 180.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UUserWidget> HealthBarWidgetClass;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* TimerWidgetComp;
	
	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	//void SetEnemyVisibility(bool IsVisible, int PlayerTeamId);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckViewport();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckTeamVisibility();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetCharacterVisibility(bool desiredVisibility);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void VisibilityTickFog();

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnDamageIndicator(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset);
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ShowWorkAreaIfNoFog(AWorkArea* WorkArea);

	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffects(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay = 0.f, float SoundDelay = 0.f);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffectsAtLocation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound,const FVector Location, float KillDelay);
	
private:
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInViewport(FVector WorldPosition, float Offset);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckHealthBarVisibility();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckTimerVisibility();
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AProjectile> ProjectileBaseClass;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AProjectile* Projectile;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	AFogOfWarManager* SpawnedFogManager;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	APlayerController* OwningPlayerController;
	
};
