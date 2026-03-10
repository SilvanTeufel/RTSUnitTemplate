// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/WorkerData.h"
#include "Core/ResourceVisualConfig.h"
#include "MassEntityTypes.h"
#include "ResourceVisualManager.generated.h"

USTRUCT()
struct FMeshMaterialKey
{
    GENERATED_BODY()

    UPROPERTY()
    UStaticMesh* Mesh = nullptr;

    UPROPERTY()
    UMaterialInterface* Material = nullptr;

    bool operator==(const FMeshMaterialKey& Other) const
    {
        return Mesh == Other.Mesh && Material == Other.Material;
    }

    friend uint32 GetTypeHash(const FMeshMaterialKey& Key)
    {
        return HashCombine(GetTypeHash(Key.Mesh), GetTypeHash(Key.Material));
    }
};

UCLASS()
class RTSUNITTEMPLATE_API UResourceVisualManager : public UWorldSubsystem {
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(Category = "RTS|Resources")
    void AssignResource(FMassEntityHandle Entity, EResourceType ResourceType, TSubclassOf<class AWorkResource> ResourceClass = nullptr);

    UFUNCTION(Category = "RTS|Resources")
    void RemoveResource(FMassEntityHandle Entity);

    UInstancedStaticMeshComponent* GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material = nullptr);

protected:
    UPROPERTY()
    UResourceVisualConfig* VisualConfig;

    UPROPERTY()
    TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;
};
