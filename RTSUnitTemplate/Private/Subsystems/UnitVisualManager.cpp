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
	bool bCastShadow = TemplateISM->CastShadow;

	// Check for reuse and cleanup stale instances
	for (int32 i = VisualFrag->VisualInstances.Num() - 1; i >= 0; --i) {
		FMassUnitVisualInstance& Existing = VisualFrag->VisualInstances[i];
		
		// Reuse if same template pointer OR if stale instance matches the new mesh/material/shadow settings
		bool bIsSameTemplate = (Existing.TemplateISM == TemplateISM);
		bool bIsStaleMatch = (!Existing.TemplateISM.IsValid() && Existing.TargetISM.IsValid() && 
							 Existing.TargetISM->GetStaticMesh() == Mesh && 
							 Existing.TargetISM->GetMaterial(0) == Material &&
							 Existing.TargetISM->CastShadow == bCastShadow);

		if (bIsSameTemplate || bIsStaleMatch) {
			Existing.TemplateISM = TemplateISM;
			Existing.BaseOffset = TemplateISM->GetRelativeTransform();
			Existing.CurrentRelativeTransform = Existing.BaseOffset;

			// Suche nach ProjectileSpawn Child unter dem TemplateISM
			const FName ProjectileSpawnTag = TEXT("ProjectileSpawn");
			TArray<USceneComponent*> Children;
			TemplateISM->GetChildrenComponents(true, Children);
			Existing.bHasMuzzle = false;
			for (USceneComponent* Child : Children) {
				if (Child->ComponentHasTag(ProjectileSpawnTag)) {
					Existing.MuzzleOffset = Child->GetRelativeTransform();
					Existing.bHasMuzzle = true;
					break;
				}
			}
			
			// Re-map the unit in the manager
			TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = ISMToUnitMap.FindOrAdd(Existing.TargetISM.Get());
			if (UnitArray.IsValidIndex(Existing.InstanceIndex)) {
				UnitArray[Existing.InstanceIndex] = Unit;
			}
			return; // Successfully reused/updated
		}
	}

	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Mesh, Material, bCastShadow);
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
	ISM->SetReceivesDecals(TemplateISM->bReceivesDecals);
	ISM->SetGenerateOverlapEvents(TemplateISM->GetGenerateOverlapEvents());
	ISM->SetRenderCustomDepth(TemplateISM->bRenderCustomDepth);
	ISM->SetCustomDepthStencilValue(TemplateISM->CustomDepthStencilValue);
	// Create a new instance with zero scale to avoid flicker
	int32 NewIndex = ISM->AddInstance(FTransform::Identity);
	ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);

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
	NewInstance.bWasVisible = false;

	// Suche nach ProjectileSpawn Child unter dem TemplateISM
	const FName ProjectileSpawnTag = TEXT("ProjectileSpawn");
	TArray<USceneComponent*> Children;
	TemplateISM->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children) {
		if (Child->ComponentHasTag(ProjectileSpawnTag)) {
			NewInstance.MuzzleOffset = Child->GetRelativeTransform();
			NewInstance.bHasMuzzle = true;
			break;
		}
	}

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
				// Move to origin and zero scale to "hide" it while staying in the pool
				FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
				Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, HiddenTransform, true, true, true);
				
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

void UUnitVisualManager::SetUnitVisualVisible(FMassEntityHandle Entity, bool bVisible) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
	FMassVisualEffectFragment* EffectFrag = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity);

	if (EffectFrag) {
		EffectFrag->bForceHidden = !bVisible;
		UE_LOG(LogTemp, Log, TEXT("UUnitVisualManager::SetUnitVisualVisible: Entity %s, Visible %d"), 
			*Entity.DebugGetDescription(), (int32)bVisible);
	}

	if (VisualFrag) {
		if (!bVisible) {
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
				if (Instance.TargetISM.IsValid() && Instance.InstanceIndex != INDEX_NONE) {
					// Move to origin and zero scale to "hide" it while staying in the pool
					FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
					Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, HiddenTransform, true, true, true);
				}
			}
		}
	} else {
		UE_LOG(LogTemp, Warning, TEXT("UUnitVisualManager::SetUnitVisualVisible: Entity %s HAS NO VISUAL FRAGMENT!"), *Entity.DebugGetDescription());
	}
}

UInstancedStaticMeshComponent* UUnitVisualManager::GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow) {
    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material;
    Key.bCastShadow = bCastShadow;

    if (ISMPool.Contains(Key)) {
        return ISMPool[Key];
    }

    AActor* ManagerActor = nullptr;
    for (TObjectIterator<AActor> It; It; ++It) {
        if (It->GetWorld() == GetWorld() && (It->ActorHasTag(TEXT("UnitVisualISMManager")) || It->GetName().Contains(TEXT("UnitVisualISMManagerActor")))) {
            ManagerActor = *It;
            break;
        }
    }

    if (!ManagerActor) {
        ManagerActor = GetWorld()->SpawnActor<AActor>();
        ManagerActor->Tags.Add(TEXT("UnitVisualISMManager"));
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
    NewISM->SetCastShadow(bCastShadow);
    // Collision will be set in AssignUnitVisual from Template
    NewISM->SetCanEverAffectNavigation(false);

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
