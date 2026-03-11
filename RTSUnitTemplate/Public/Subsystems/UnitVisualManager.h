// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassEntityTypes.h"
#include "Subsystems/ResourceVisualManager.h" // For FMeshMaterialKey
#include "UnitVisualManager.generated.h"

class AMassUnitBase;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
struct FMassEntityHandle;
struct FMeshMaterialKey;

UCLASS()
class RTSUNITTEMPLATE_API UUnitVisualManager : public UWorldSubsystem {
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void AssignUnitVisual(FMassEntityHandle Entity, UInstancedStaticMeshComponent* TemplateISM, AMassUnitBase* Unit);

	void RemoveUnitVisual(FMassEntityHandle Entity);

	UInstancedStaticMeshComponent* GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material = nullptr, bool bCastShadow = true);

	AMassUnitBase* GetUnitFromInstance(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex);

protected:
	UPROPERTY()
	TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;

	TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, TArray<TWeakObjectPtr<AMassUnitBase>>> ISMToUnitMap;
};
