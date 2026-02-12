// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bAddCapsuleHalfHeightToIsm = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bNeedsMassUnitSetup = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
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

	// Client-side request to queue safe Mass link after server creation
	UFUNCTION(BlueprintCallable, Category = Mass)
	void RequestClientMassLink();

	// Client-side request to unlink/destroy the Mass entity when server unregistered
	UFUNCTION(BlueprintCallable, Category = Mass)
	void RequestClientMassUnlink();

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float SightRadius = 2000.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float LoseSightRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float DespawnTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float IsAttackedDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float AvoidanceDistance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float ObstacleSeparationStiffness = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool RotatesToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool CanManipulateNavMesh = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool RotatesToEnemy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float RotationSpeed = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float MinRange = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float MaxAcceleration = 4000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float AdditionalCapsuleRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float FollowRadius = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float FollowOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float InstantLoadRange = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	int32 TransportId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float VerticalDeathRotationMultiplier = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool StopSeparation = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool GroundAlignment = true;
};
