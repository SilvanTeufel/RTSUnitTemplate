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
	
	virtual void BeginPlay() override;

	virtual void Destroyed() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	TSubclassOf<AFogOfWarManager> FogOfWarManagerClass;
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SpawnFogOfWarManager();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SetInvisibileIfNoOverlap();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	bool IsOverlappingFogOfWarManager(int PlayerTeamId);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerMultiplier = FVector(0.008, 0.008, 0.0004);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerPositionOffset = FVector(0, 0, -250.f);
	// Function to update light range

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightRadius", Keywords = "RTSUnitTemplate SightRadius"), Category = RTSUnitTemplate)
	float SightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsMyTeam = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnemyStartVisibility = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogSight = 1500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CurrentVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsVisibileEnemy = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnableFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int FogManagerOverlaps = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityOffset = -100.0f;

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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetEnemyVisibility(bool IsVisible, int PlayerTeamId);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckViewport();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckTeamVisibility();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetCharacterVisibility(bool desiredVisibility);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void VisibilityTickFog();
private:
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInViewport(FVector WorldPosition, float Offset);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckHealthBarVisibility();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckTimerVisibility();
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AProjectile> ProjectileBaseClass;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	class AProjectile* Projectile;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AFogOfWarManager* SpawnedFogManager;
	
};
