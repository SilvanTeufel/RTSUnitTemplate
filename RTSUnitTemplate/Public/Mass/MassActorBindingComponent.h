// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorComponent.h"
#include "MassEntityTypes.h"
#include "MassRepresentationTypes.h"
#include "Mass/UnitMassTag.h"
// Add near the top of UMassActorBindingComponent.h or .cpp
#include "MassEntityConfigAsset.h"
#include "MassEntityUtils.h" // For CreateEntityFromConfig helper
#include "MassActorBindingComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTSUNITTEMPLATE_API UMassActorBindingComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMassActorBindingComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	FMassEntityHandle MassEntityHandle;

	// Caches the Entity Subsystem pointer
	UPROPERTY() // Don't save this pointer
	UMassEntitySubsystem* MassEntitySubsystemCache;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//UPROPERTY()
	// UMassEntityConfigAsset* UnitEntityConfig;
	
	UPROPERTY() 
	AActor* MyOwner;
	// Set by your spawner when binding the actor to a Mass entity.
	//void SetMassEntityHandle(FMassEntityHandle InHandle) { MassEntityHandle = InHandle; }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mass ) 
	UStaticMesh* UnitMassMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig;
	
	FMassEntityHandle CreateAndLinkOwnerToMassEntity();

	void InitializeMassEntityStatsFromOwner(FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle, AActor* OwnerActor); // <<< ADD THIS

	// Getter for the handle
	FMassEntityHandle GetMassEntityHandle() const { return MassEntityHandle; }

	FStaticMeshInstanceVisualizationDescHandle RegisterIsmDesc(UWorld* World, UStaticMesh* UnitStaticMesh);
	
	void SpawnMassUnitIsm(
	FMassEntityManager& EntityManager,
	UStaticMesh* UnitStaticMesh,
	const FVector SpawnLocation,
	UWorld* World);

	void CleanupMassEntity();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float SightRadius = 2000.f; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float LoseSightRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float DespawnTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float IsAttackedDuration = 0.3f;
};
