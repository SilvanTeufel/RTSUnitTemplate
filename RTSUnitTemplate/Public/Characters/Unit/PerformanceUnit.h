// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
#include "LevelUnit.h"
#include "GameFramework/Character.h"
#include "Core/UnitData.h"
#include "Actors/DijkstraCenter.h"
#include "Core/DijkstraMatrix.h"
#include "Actors/Projectile.h"
#include "Components/PointLightComponent.h"
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

	// Light component for Fog of War
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar")
	UPointLightComponent* FogOfWarLight; // Or USpotLightComponent if using a spotlight

	// Function to update light range
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void UpdateFogOfWarLight(int PlayerTeamId, float SightRange);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HealthCompCreated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int HideHealthBarUnitCount = 200;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* HealthWidgetComp;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector HealthWidgetCompLocation = FVector (0.f, 0.f, 180.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UUserWidget> HealthBarWidgetClass;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* TimerWidgetComp;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetVisibility(bool IsVisible, int PlayerTeamId);
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckVisibility(int PlayerTeamId);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void VisibilityTick();
private:
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	bool IsInViewport(FVector WorldPosition, float Offset);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckHealthBarVisibility();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckTimerVisibility();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileBaseClass", Keywords = "TopDownRTSTemplate ProjectileBaseClass"), Category = RTSUnitTemplate)
	TSubclassOf<class AProjectile> ProjectileBaseClass;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Projectile", Keywords = "RTSUnitTemplate Projectile"), Category = RTSUnitTemplate)
	class AProjectile* Projectile;
	
};
