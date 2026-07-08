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
#include "Net/UnrealNetwork.h"
#include "MassActorBindingComponent.generated.h"


UENUM(BlueprintType)
enum class EMassBindingType : uint8
{
	Unit,
	Building,
	EffectArea
};

class UStaticMesh;
class UMaterialInterface;

// One selectable ruin variant. On death a dead ISM unit/building picks one of these at random (seeded
// by the replicated UnitIndex so client & server agree) and swaps its pooled ISM visual to Mesh. The
// ruin is FIRST auto-fitted to roughly match the original building mesh's world bounds, THEN
// StretchFactor is applied on top (per-axis) for mesh-dependent manual size correction.
USTRUCT(BlueprintType)
struct FRuinMeshEntry
{
	GENERATED_BODY()

	// The ruin static mesh to display. Required for the entry to be usable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruin")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Extra squash/stretch applied AFTER the automatic building-size fit (multiplied per-axis onto the
	// auto-fit scale). (1,1,1) = pure auto-fit. Use to correct meshes whose natural proportions differ
	// from the building footprint. This is the "obendrauf / danach" adjustment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruin")
	FVector StretchFactor = FVector::OneVector;
};

// Delegate-Definition
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMassArchetypeBuilding, AActor* /*Owner*/, TArray<const UScriptStruct*>& /*FragmentsAndTags*/);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTSUNITTEMPLATE_API UMassActorBindingComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	static FOnMassArchetypeBuilding OnMassArchetypeBuilding;
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	EMassBindingType BindingType = EMassBindingType::Unit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool bDebugLogs = false;

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnActor();

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnUnit();

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnBuilding();

	UFUNCTION(BlueprintCallable, Category = Mass)
	void SetupMassOnEffectArea();
	
	UPROPERTY() 
	AActor* MyOwner;
	// Set by your spawner when binding the actor to a Mass entity.
	//void SetMassEntityHandle(FMassEntityHandle InHandle) { MassEntityHandle = InHandle; }

	void ConfigureNewEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity);
	
	FMassEntityHandle CreateAndLinkOwnerToMassEntity();
	
	FMassEntityHandle CreateAndLinkBuildingToMassEntity();

	FMassEntityHandle CreateAndLinkEffectAreaToMassEntity();

	// Client-side request to queue safe Mass link after server creation
	UFUNCTION(BlueprintCallable, Category = Mass)
	void RequestClientMassLink();

	// Verifies if all replication data (NetID + Bubble data) are available on the client
	bool IsReadyForClientMassLink() const;

	// Controls visual freeze/visibility during initialization
	void SetVisualFreeze(bool bFrozen);

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
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float SightRadius = 2000.f; 

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float LoseSightRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float LoseSightRadiusFaktor = 2.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float LoseSightRadiusFaktorTimer = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float DespawnTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float HideActorTime = 5.0f;

	// --- Ruin-on-death (ISM buildings/units only; skeletal-driven visuals are not supported) ---
	// Master toggle. When true and RuinMeshArray is non-empty, the dead unit swaps its pooled ISM visual
	// to a random ruin mesh at SwitchToRuinMeshTime.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Ruin")
	bool bSpawnRuinOnDeath = false;

	// Seconds since death at which the ruin appears. Clamped to >= HideActorTime at bind (building hides
	// first, then the ruin). Keep < DespawnTime so the ruin is visible before the entity despawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Ruin", meta = (EditCondition = "bSpawnRuinOnDeath"))
	float SwitchToRuinMeshTime = 6.0f;

	// Candidate ruin meshes; one is chosen at random (seeded by the replicated UnitIndex so all machines
	// pick the same one) and auto-fitted to the original building mesh, then its StretchFactor applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Ruin", meta = (EditCondition = "bSpawnRuinOnDeath"))
	TArray<FRuinMeshEntry> RuinMeshArray;

	// Optional material override for the ruin ISM. If null, the ruin mesh's own default material is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Ruin", meta = (EditCondition = "bSpawnRuinOnDeath"))
	TObjectPtr<UMaterialInterface> RuinMaterialOverride = nullptr;

	// Whether the ruin ISM casts a shadow.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Ruin", meta = (EditCondition = "bSpawnRuinOnDeath"))
	bool bRuinCastShadow = true;

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

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float FollowRadius = 200.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float FollowOffset = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float VerticalDeathRotationMultiplier = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool StopSeparation = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool GroundAlignment = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool UseRotateToTargetProcessor = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool CanMoveWhileAttacking = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	bool RotatesToMovementIfMoveWhileAttacking = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float RotateToTargetDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float RotateToTargetExp = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	float RotateToTargetYawOffset = 0.f;
};
