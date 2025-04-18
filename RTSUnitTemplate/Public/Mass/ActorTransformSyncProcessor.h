// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h" // For FTransformFragment
// Include your hypothetical FMassActorFragment or MassRepresentationFragments if using that system
#include "ActorTransformSyncProcessor.generated.h"

// Forward declaration if needed
class UMassRepresentationSubsystem;

UCLASS()
class RTSUNITTEMPLATE_API UActorTransformSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UActorTransformSyncProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float AccumulatedTimeA = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float AccumulatedTimeB = 0.0f;

	/** Geschwindigkeit, mit der sich der Actor in die Bewegungsrichtung dreht (Grad pro Sekunde). */
	UPROPERTY(EditDefaultsOnly, Category = "Rotation")
	float ActorRotationSpeed = 10*360.0f;

	/** Minimale Distanz, die sich die Einheit bewegen muss, damit eine neue Rotation berechnet wird (verhindert Jitter bei Stillstand). */
	UPROPERTY(EditDefaultsOnly, Category = "Rotation")
	float MinMovementDistanceForRotation = 1.0f;
	
protected:
	virtual void ConfigureQueries() override;
	//virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// Cache subsystem pointer for efficiency
	UPROPERTY(Transient)
	UMassRepresentationSubsystem* RepresentationSubsystem; // Example if using Representation Subsystem
};