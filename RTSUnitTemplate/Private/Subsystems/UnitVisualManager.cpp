// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Subsystems/UnitVisualManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "Characters/Unit/MassUnitBase.h"

void UUnitVisualManager::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
}

void UUnitVisualManager::AssignUnitVisual(FMassEntityHandle Entity, UInstancedStaticMeshComponent* TemplateISM, AMassUnitBase* Unit) {
	if (!TemplateISM || !TemplateISM->GetStaticMesh()) {
		UE_LOG(LogTemp, Warning, TEXT("UUnitVisualManager::AssignUnitVisual: TemplateISM is null or has no static mesh!"));
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) {
		UE_LOG(LogTemp, Error, TEXT("UUnitVisualManager::AssignUnitVisual: EntitySubsystem is null!"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	if (!VisualFrag) {
		UE_LOG(LogTemp, Warning, TEXT("UUnitVisualManager::AssignUnitVisual: VisualFrag is null for entity %s!"), *Entity.DebugGetDescription());
		return;
	}

	UStaticMesh* Mesh = TemplateISM->GetStaticMesh();
	UMaterialInterface* Material = TemplateISM->GetMaterial(0);

	UE_LOG(LogTemp, Log, TEXT("UUnitVisualManager::AssignUnitVisual: Entity %s, Mesh %s, Material %s, Unit %s"), 
		*Entity.DebugGetDescription(), *Mesh->GetName(), Material ? *Material->GetName() : TEXT("None"), Unit ? *Unit->GetName() : TEXT("None"));

	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Mesh, Material);
	if (!ISM) {
		UE_LOG(LogTemp, Error, TEXT("UUnitVisualManager::AssignUnitVisual: Failed to get or create ISM for mesh %s!"), *Mesh->GetName());
		return;
	}

	// Copy collision settings from template to the pooled ISM
	// Note: Since ISMs are pooled by mesh/material, the last one assigned will dictate collision for all instances.
	ISM->SetCollisionEnabled(TemplateISM->GetCollisionEnabled());
	ISM->SetCollisionResponseToChannels(TemplateISM->GetCollisionResponseToChannels());
	ISM->SetCollisionObjectType(TemplateISM->GetCollisionObjectType());
	if (ISM->GetCollisionObjectType() == ECC_WorldStatic) { ISM->SetCollisionObjectType(ECC_WorldDynamic); }
	
	UE_LOG(LogTemp, Log, TEXT("UUnitVisualManager::AssignUnitVisual: Transferred collision from %s to pooled ISM %s (Enabled: %d)"), 
		*TemplateISM->GetName(), *ISM->GetName(), (int32)ISM->GetCollisionEnabled());

	// Create a new instance with zero scale to avoid flicker
	int32 NewIndex = ISM->AddInstance(FTransform::Identity);
	ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true);

	// Map ISM instance to MassUnitBase
	TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = ISMToUnitMap.FindOrAdd(ISM);
	if (UnitArray.Num() <= NewIndex) {
		UnitArray.SetNum(NewIndex + 1);
	}
	UnitArray[NewIndex] = Unit;

	FMassUnitVisualInstance NewInstance;
	NewInstance.TemplateISM = TemplateISM;
	NewInstance.TargetISM = ISM;
	NewInstance.InstanceIndex = NewIndex;
	NewInstance.BaseOffset = TemplateISM->GetRelativeTransform();
	NewInstance.CurrentRelativeTransform = NewInstance.BaseOffset;

	UE_LOG(LogTemp, Log, TEXT("UUnitVisualManager::AssignUnitVisual: NewInstance at Index %d, BaseOffset: %s"), 
		NewIndex, *NewInstance.BaseOffset.ToString());

	VisualFrag->VisualInstances.Add(NewInstance);
}

void UUnitVisualManager::RemoveUnitVisual(FMassEntityHandle Entity) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	if (VisualFrag) {
		for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
			if (Instance.TargetISM.IsValid() && Instance.InstanceIndex != INDEX_NONE) {
				Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true);
				
				// Clear map entry
				if (TArray<TWeakObjectPtr<AMassUnitBase>>* UnitArray = ISMToUnitMap.Find(Instance.TargetISM.Get())) {
					if (UnitArray->IsValidIndex(Instance.InstanceIndex)) {
						(*UnitArray)[Instance.InstanceIndex] = nullptr;
					}
				}
			}
		}
		VisualFrag->VisualInstances.Empty();
	}
}

UInstancedStaticMeshComponent* UUnitVisualManager::GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material) {
    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material;

    if (ISMPool.Contains(Key)) {
        return ISMPool[Key];
    }

    AActor* ManagerActor = nullptr;
    for (TObjectIterator<AActor> It; It; ++It) {
        if (It->GetWorld() == GetWorld() && It->GetName().Contains(TEXT("UnitVisualISMManagerActor"))) {
            ManagerActor = *It;
            break;
        }
    }

    if (!ManagerActor) {
        ManagerActor = GetWorld()->SpawnActor<AActor>();
        ManagerActor->SetFlags(RF_Transient);
        USceneComponent* Root = NewObject<USceneComponent>(ManagerActor, TEXT("Root"));
        ManagerActor->SetRootComponent(Root);
        Root->RegisterComponent();
#if WITH_EDITOR
        ManagerActor->SetActorLabel(TEXT("UnitVisualISMManagerActor"));
#endif
    }

    UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(ManagerActor);
    NewISM->SetStaticMesh(Mesh);
    if (Material) {
        NewISM->SetMaterial(0, Material);
    }
    
    NewISM->SetMobility(EComponentMobility::Movable);
    NewISM->SetCastShadow(true);
    // Collision will be set in AssignUnitVisual from Template
    NewISM->SetGenerateOverlapEvents(false);
    NewISM->SetCanEverAffectNavigation(false);

    UE_LOG(LogTemp, Log, TEXT("UUnitVisualManager::GetOrCreateISM: Created new ISM for mesh %s in ManagerActor %s"), *Mesh->GetName(), *ManagerActor->GetName());

    NewISM->AttachToComponent(ManagerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    NewISM->RegisterComponent();
    
	ISMPool.Add(Key, NewISM);

	return NewISM;
}

AMassUnitBase* UUnitVisualManager::GetUnitFromInstance(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex) {
	if (!ISM) return nullptr;
	if (TArray<TWeakObjectPtr<AMassUnitBase>>* UnitArray = ISMToUnitMap.Find(ISM)) {
		if (UnitArray->IsValidIndex(InstanceIndex)) {
			return (*UnitArray)[InstanceIndex].Get();
		}
	}
	return nullptr;
}
