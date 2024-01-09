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

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// DamageIndicator related /////////
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	class UWidgetComponent* DamageIndicatorComp;

	UPROPERTY(Replicated, BlueprintReadWrite,  Category = TopDownRTSTemplate)
	float LastDamage;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	FVector DamageIndicatorCompLocation = FVector (0.f, 0.f, 50.f);
	
	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
	void SpawnDamageIndicator(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float LifeTime = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxLifeTime = 3.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MinDrift = -10.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxDrift = 10.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ZDrift = 10.f;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UpdateWidget(float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset);

	///////////////////////////////////////////////////////////////////
};
