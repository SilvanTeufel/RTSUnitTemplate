// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedCameraBase.h"
#include "Actors/UnitSpawnPlatform.h"
#include "AdvancedCameraBase.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AAdvancedCameraBase : public AExtendedCameraBase
{
	GENERATED_BODY()

public:
	AAdvancedCameraBase(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void CustomSpawnPlatform();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector CustomPlatformOffset = FVector(500.0f, 0.0f, -300.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AUnitSpawnPlatform> CustomSpawnPlatformClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitSpawnPlatform* CustomAttachedPlatform;
};