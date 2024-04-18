// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "AutoCamWaypoint.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AAutoCamWaypoint : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAutoCamWaypoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Tag;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UBoxComponent* BoxComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AAutoCamWaypoint* NextWaypoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitTime = 15.0f;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = RTSUnitTemplate)
		void OnPlayerEnter(UPrimitiveComponent* OverlapComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
