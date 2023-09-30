// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SelectedIcon.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API ASelectedIcon : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASelectedIcon();

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* IconMesh;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* BlueMaterial;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* ActionMaterial;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInstanceDynamic* DynMaterial;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(Category = TopDownRTSTemplate)
	void ChangeMaterialColour(FVector4d Colour);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ChangeMaterialToAction();

	float TriggerCapsuleRadius = 300.f;
	float TriggerCapsuleHeight = 300.f;
};
