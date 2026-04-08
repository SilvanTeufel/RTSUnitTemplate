#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Subsystems/ResourceVisualManager.h" // For FMeshMaterialKey
#include "EffectAreaVisualManager.generated.h"

class AEffectArea;

/**
 * Subsystem to manage effect area visualization via ISM and Mass.
 */
UCLASS()
class RTSUNITTEMPLATE_API UEffectAreaVisualManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

    /** Helper for pooled ISMs */
    UInstancedStaticMeshComponent* GetOrCreatePooledISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow);

	/** Helper to find or create a central manager actor */
	AActor* GetOrCreateManagerActor();

    /** Registers a visual instance for an effect area entity */
    void AddVisualInstance(FMassEntityHandle EntityHandle, AEffectArea* EffectAreaActor);

private:
    UPROPERTY()
    TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;

    UPROPERTY()
    TObjectPtr<AActor> ManagerActor = nullptr;
};
