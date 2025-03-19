// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* IndicatorMesh;
	
private:
	// Root Scene Component
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot;
};

