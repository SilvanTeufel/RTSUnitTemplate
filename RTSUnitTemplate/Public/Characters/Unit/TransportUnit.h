// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "Components/CapsuleComponent.h"
#include "TransportUnit.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ATransportUnit : public AAbilityUnit
{
	GENERATED_BODY()

public:

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool IsInitialized = true;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void BindTransportOverlap();
	
	UFUNCTION(NetMulticast, Reliable, Category = RTSUnitTemplate)
	void SetCollisionAndVisibility(bool IsVisible);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void KillLoadedUnits();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetRdyForTransport(bool Rdy);
	// Loads a unit into this transport.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LoadUnit(AUnitBase* UnitToLoad);

	// Unloads all loaded units.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UnloadAllUnits();

	// Replication of properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Overlap callback function.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnCapsuleOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
	
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	bool IsATransporter = false;
	// Whether this unit can be transported.
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	bool CanBeTransported = false;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = Transport)
	bool RdyForTransport = false;
	
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	float InstantLoadRange = 300.f;

	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	FVector VoidLocation = FVector(-50000.f, -50000.f, -50000.f);


	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	FVector UnloadOffset = FVector(0.f, 0.f, 100.f);


	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	float UnloadVariationMin = 100.f;

	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	float UnloadVariatioMax = 200.f;


	// Maximum number of units this transport can hold.
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	int MaxTransportUnits = 4;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = Transport)
	int CurrentUnitsLoaded = 0;
protected:

	// Timer handle used to schedule unloading.
	FTimerHandle UnloadTimerHandle;

	// Helper function to unload the next unit.
	void UnloadNextUnit();
	
	// Array holding the loaded units.
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = Transport)
	TArray<ATransportUnit*> LoadedUnits;
	
};