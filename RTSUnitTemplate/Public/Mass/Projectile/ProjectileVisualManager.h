#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Subsystems/ResourceVisualManager.h" // For FMeshMaterialKey
#include "ProjectileVisualManager.generated.h"

class AProjectile;

USTRUCT(BlueprintType)
struct FProjectileClassData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AProjectile> CDO = nullptr;
};

/**
 * Subsystem to manage projectile visualization via ISM and Mass.
 */
UCLASS()
class RTSUNITTEMPLATE_API UProjectileVisualManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Get or create ISM for a projectile class */
	UInstancedStaticMeshComponent* GetOrCreateISMComponent(TSubclassOf<AProjectile> ProjectileClass);

    /** Helper for pooled ISMs */
    UInstancedStaticMeshComponent* GetOrCreatePooledISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow);

	/** Helper to find or create a central manager actor */
	AActor* GetOrCreateManagerActor();

	/** Register a projectile class and return its default values */
	const AProjectile* GetProjectileCDO(TSubclassOf<AProjectile> ProjectileClass);

    /** Spawns a new mass projectile */
    FMassEntityHandle SpawnMassProjectile(TSubclassOf<AProjectile> ProjectileClass, const FTransform& Transform, AActor* Shooter, AActor* Target, FVector TargetLocation, FMassEntityHandle ShooterEntity = FMassEntityHandle(), FMassEntityHandle TargetEntity = FMassEntityHandle(), float ProjectileSpeed = 0.f, int32 ShooterTeamId = -1, bool bFollowTarget = false, float HomingInitialAngle = 0.f, float HomingRotationSpeed = 0.f, float HomingMaxSpiralRadius = 0.f, float HomingInterpSpeed = 2.f, struct FMassCommandBuffer* CommandBuffer = nullptr, FVector Scale = FVector::OneVector);

private:
	UPROPERTY()
	TMap<TSubclassOf<AProjectile>, FProjectileClassData> ProjectileDataMap;

    UPROPERTY()
    TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;

    FMassEntityManager* EntityManager = nullptr;
    FMassArchetypeHandle ProjectileArchetype;
};
