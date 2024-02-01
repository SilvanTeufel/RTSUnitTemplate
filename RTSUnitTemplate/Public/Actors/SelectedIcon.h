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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	UStaticMeshComponent* IconMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	UMaterialInterface* BlueMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	UMaterialInterface* ActionMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	UMaterialInstanceDynamic* DynMaterial;

	// Global string definitions
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FString SphereMeshAssetPath = TEXT("/Engine/BasicShapes/Plane");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FString MaterialBluePath = TEXT("Material'/RTSUnitTemplate/RTSUnitTemplate/Materials/M_Ring_Aura.M_Ring_Aura'");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FString MaterialActionPath = TEXT("Material'/RTSUnitTemplate/RTSUnitTemplate/Materials/M_Ring_Aura_Red.M_Ring_Aura_Red'");

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

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent();
};
