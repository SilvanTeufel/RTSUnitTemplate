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
#include "PerformanceUnit.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API APerformanceUnit : public ALevelUnit
{
	GENERATED_BODY()

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	
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
private:
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckVisibility();

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
