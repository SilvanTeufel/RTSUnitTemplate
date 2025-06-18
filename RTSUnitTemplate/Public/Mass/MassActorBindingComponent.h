// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorComponent.h"
#include "MassEntityTypes.h"
#include "MassRepresentationTypes.h"
#include "Mass/UnitMassTag.h"
// Add near the top of UMassActorBindingComponent.h or .cpp
#include "MassEntityConfigAsset.h"
#include "MassCommandBuffer.h"
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

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	
	FMassEntityHandle MassEntityHandle;

	// Caches the Entity Subsystem pointer
	UPROPERTY() // Don't save this pointer
	UMassEntitySubsystem* MassEntitySubsystemCache;
	
public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass")
	bool bNeedsMassUnitSetup = false;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass")
	bool bNeedsMassBuildingSetup = false;
	
	FMassEntityHandle GetEntityHandle() { return MassEntityHandle; }
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnUnit();

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnBuilding();
	
	UPROPERTY() 
	AActor* MyOwner;
	// Set by your spawner when binding the actor to a Mass entity.
	//void SetMassEntityHandle(FMassEntityHandle InHandle) { MassEntityHandle = InHandle; }

	void ConfigureNewEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity);
	
	FMassEntityHandle CreateAndLinkOwnerToMassEntity();
	
	FMassEntityHandle CreateAndLinkBuildingToMassEntity();

	// Helpers to build archetype and shared values
	bool BuildArchetypeAndSharedValues(FMassArchetypeHandle& OutArchetype,
									   FMassArchetypeSharedFragmentValues& OutSharedValues);

	bool BuildArchetypeAndSharedValuesForBuilding(FMassArchetypeHandle& OutArchetype,
								   FMassArchetypeSharedFragmentValues& OutSharedValues);

	
	// Initialization routines, called by processor
	void InitTransform(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle);
	void InitMovementFragments(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle);
	void InitAIFragments(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle);
	void InitRepresentation(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle);
	void InitStats(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle, AActor* OwnerActor);


	
	void InitializeMassEntityStatsFromOwner(FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle, AActor* OwnerActor); // <<< ADD THIS

	// Getter for the handle
	FMassEntityHandle GetMassEntityHandle() const { return MassEntityHandle; }
	
	void CleanupMassEntity();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float SightRadius = 2000.f; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float LoseSightRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float DespawnTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float IsAttackedDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float AvoidanceDistance = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float ObstacleSeparationStiffness = 2000.f;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	bool RotatesToMovement = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	bool RotatesToEnemy = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass")
	float RotationSpeed = 15.f;
};
