// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "FogOfWarCentralManager.generated.h"

class APerformanceUnit;
class AFogOfWarManager;

UCLASS()
class RTSUNITTEMPLATE_API AFogOfWarCentralManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFogOfWarCentralManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TickInterval = 0.25f;
	
	// A large box that covers the fog area
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FogOfWar")
	//UBoxComponent* CollisionBox;

	// Optionally, maintain a list of individual fog actors to update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar")
	TArray<AFogOfWarManager*> FogManagers;
	
	// Called each tick to update collisions for the fog system
	void UpdateFogCollisions();
};
