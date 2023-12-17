// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Widgets/DamageIndicator.h"
#include "Components/WidgetComponent.h"
#include "IndicatorActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AIndicatorActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AIndicatorActor(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// DamageIndicator related /////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	class UWidgetComponent* DamageIndicatorComp;

	UPROPERTY(BlueprintReadWrite,  Category = TopDownRTSTemplate)
	float LastDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	FVector DamageIndicatorCompLocation = FVector (0.f, 0.f, 50.f);
	
	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
	void SpawnDamageIndicator(float Damage);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float LifeTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxLifeTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MinDrift = -10.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxDrift = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ZDrift = 10.f;
	///////////////////////////////////////////////////////////////////
};
