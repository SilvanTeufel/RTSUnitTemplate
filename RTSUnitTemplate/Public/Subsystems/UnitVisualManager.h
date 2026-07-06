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
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void AssignUnitVisual(FMassEntityHandle Entity, UInstancedStaticMeshComponent* TemplateISM, AMassUnitBase* Unit);

	void RemoveUnitVisual(FMassEntityHandle Entity);

	void SetUnitVisualVisible(FMassEntityHandle Entity, bool bVisible);

	// Entity-INDEPENDENT release of a single pooled instance (hide + return index to the pool + clear the
	// map slot), guarded so it never touches an index that pooling has already reassigned to a different
	// LIVE unit. Called from AMassUnitBase::EndPlay so a dead actor's pooled instance is always freed even
	// if its Mass entity is already gone (the entity-gated RemoveUnitVisual would no-op -> visible orphan).
	void ReleaseInstanceDirect(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* Owner);

	// Belt-and-suspenders safety net: reclaim pooled instances whose owning actor was destroyed without
	// ever freeing them (any teardown path). Only acts on STALE map slots (destroyed actor), never on live
	// or explicitly-freed slots. Driven by a low-frequency timer started in OnWorldBeginPlay.
	void SweepStaleInstances();

	UInstancedStaticMeshComponent* GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material = nullptr, bool bCastShadow = true);

	AMassUnitBase* GetUnitFromInstance(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex);

protected:
	// Shared free primitive used by RemoveUnitVisual, ReleaseInstanceDirect and SweepStaleInstances.
	// Skips the free if the (ISM,Index) slot currently holds a LIVE unit that is not ExpectedOwner (i.e.
	// pooling already reassigned the index) — prevents hiding/double-freeing a live unit's instance. Uses
	// AddUnique into FreeIndexPool so any combination/order of the free paths is idempotent.
	void ReleasePooledInstanceInternal(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* ExpectedOwner);

	FTimerHandle SweepTimerHandle;

	UPROPERTY()
	TMap<FMeshMaterialKey, UInstancedStaticMeshComponent*> ISMPool;

	TMap<FMeshMaterialKey, TArray<int32>> FreeIndexPool;

	TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, TArray<TWeakObjectPtr<AMassUnitBase>>> ISMToUnitMap;
};
