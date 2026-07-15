// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Subsystems/ResourceVisualManager.h" // For FMeshMaterialKey
#include "Actors/Projectile.h"
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
    FMassEntityHandle SpawnMassProjectile(TSubclassOf<AProjectile> ProjectileClass, const FTransform& Transform, AActor* Shooter, AActor* Target, FVector TargetLocation, FMassEntityHandle ShooterEntity = FMassEntityHandle(), FMassEntityHandle TargetEntity = FMassEntityHandle(), float ProjectileSpeed = 0.f, int32 ShooterTeamId = -1, bool bFollowTarget = false, float HomingInitialAngle = 0.f, float HomingRotationSpeed = 0.f, float HomingMaxSpiralRadius = 0.f, float HomingInterpSpeed = 2.f, struct FMassCommandBuffer* CommandBuffer = nullptr, FVector Scale = FVector::OneVector, float Damage = -1.f, int32 MaxPiercedTargets = -1, bool bIsPredicted = false, TSubclassOf<class UGameplayEffect> ProjectileEffect = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect2 = nullptr, TSubclassOf<class UGameplayEffect> ProjectileEffect3 = nullptr, FEffectAreaInfo AreaInfo = FEffectAreaInfo());

    /** Returns the transform of a Mass projectile from its fragment */
    FTransform GetProjectileTransform(FMassEntityHandle EntityHandle) const;

    /** Retrieves a pooled ISM matching the template's mesh, material, and shadow settings */
    UInstancedStaticMeshComponent* GetVisualISM(UInstancedStaticMeshComponent* TemplateISM);

private:
	/**
	 * Fires the CDO's one-shot SpawnVFX/SpawnSound at the projectile's spawn transform.
	 * LOCAL ONLY - deliberately not an RPC and not a UFUNCTION: SpawnMassProjectile already runs
	 * independently on the server (AUnitBase::SpawnProjectile*) and on every client
	 * (FUnitReplicationItem::PostReplicatedChange), so a NetMulticast here would fire twice on
	 * remote clients and pay per-shot bandwidth for something both sides already know. Every other
	 * effect call site in the plugin is a server-only origin that needs a multicast to reach
	 * clients; this one is not. Do not "promote" it to an RPC.
	 * @param bVisible  Result of the fog formula; false skips the spawn entirely - a one-shot burst
	 *                  has no later frame in which it could be un-hidden.
	 */
	void FireSpawnEffects(const AProjectile* CDO, const FTransform& SpawnTransform, bool bVisible);

	UPROPERTY()
	TMap<TSubclassOf<AProjectile>, FProjectileClassData> ProjectileDataMap;

    UPROPERTY()
    TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;

    FMassEntityManager* EntityManager = nullptr;
    FMassArchetypeHandle ProjectileArchetype;
};
