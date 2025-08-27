// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
//#include "LevelUnit.h"
#include "MassEntityTypes.h"
#include "MassUnitBase.h"
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
class RTSUNITTEMPLATE_API APerformanceUnit : public AMassUnitBase
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
	
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	//TSubclassOf<AFogOfWarManager> FogOfWarManagerClass;
	
	//UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	//void SpawnFogOfWarManager(APlayerController* PC);

	//UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	//void SpawnFogOfWarManagerTeamIndependent(APlayerController* PC);
	
	//UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	//void DestroyFogManager();

	
	//UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category=RTSUnitTemplate)
	//void SetOwningPlayerController();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerMultiplier = FVector(0.01, 0.01, 200);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerPositionOffset = FVector(0, 0, 50.f);
	// Function to update light range
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool StopVisibilityTick = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsMyTeam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnemyStartVisibility = true;
	
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
	float VisibilityOffset = 150.f;

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
	FVector HealthWidgetRelativeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UUserWidget> HealthBarWidgetClass;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* TimerWidgetComp;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector TimerWidgetRelativeOffset;
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
	void ShowAbilityIndicator(AAbilityIndicator* AbilityIndicator);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AbilityIndicatorVisibility = false;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void HideAbilityIndicator(AAbilityIndicator* AbilityIndicator);
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffects(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay = 0.f, float SoundDelay = 0.f);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffectsAtLocation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound,const FVector Location, float KillDelay, FRotator Rotation = FRotator(0.0f, 0.0f, 0.0f));
	
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
	
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	//AFogOfWarManager* SpawnedFogManager;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	APlayerController* OwningPlayerController;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsNetVisible() const;

	// Server RPC for the client to report its own visibility
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetClientVisibility(bool bVisible);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSetEnemyVisibility(APerformanceUnit* DetectingActor, bool bVisible);

	// Pure compute helper if you ever need the raw bool:
	bool ComputeLocalVisibility() const;
protected:
	
	UPROPERTY(VisibleAnywhere, Replicated, Category = RTSUnitTemplate)
	bool bClientIsVisible = false;

	// Renamed to reflect that it *updates* (and/or computes) the client‐side flag:
	void UpdateClientVisibility();
};
