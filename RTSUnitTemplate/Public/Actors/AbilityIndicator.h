// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"

// Forward declarations for properties used below
class UMaterialInterface;
class UStaticMeshComponent;

// NOTE: generated.h must remain the last include in this header
#include "AbilityIndicator.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AAbilityIndicator : public AActor
{
	GENERATED_BODY()
	
public: 
	// Sets default values for this actor's properties
	AAbilityIndicator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	// Sets the position of the indicator
	void SetIndicatorLocation(const FVector& NewLocation);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	UStaticMeshComponent* IndicatorMesh;

	// If true, the controller will detect overlaps with WorkAreas and highlight this indicator
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DetectOverlapWithWorkArea = false;

	// Cached original material of the indicator; set at BeginPlay if not assigned
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* OriginalMaterial = nullptr;

	// Temporary highlight material used while overlapping a WorkArea
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* TemporaryHighlightMaterial = nullptr;

	// True while the indicator is overlapping any WorkArea (for UI/logic)
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOverlappedWithWorkArea = false;

private:
	// Root Scene Component
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;
	
};



