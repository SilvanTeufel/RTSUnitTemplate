// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Subsystems/UnitVisualManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "Characters/Unit/MassUnitBase.h"
#include "MassActorSubsystem.h"
#include "TimerManager.h"

void UUnitVisualManager::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
}

void UUnitVisualManager::OnWorldBeginPlay(UWorld& InWorld) {
	Super::OnWorldBeginPlay(InWorld);
	// Low-frequency safety net: reclaim any pooled instance whose owning actor died without freeing it
	// (any teardown path). Complements the actor-cache release in AMassUnitBase::EndPlay.
	InWorld.GetTimerManager().SetTimer(SweepTimerHandle, this, &UUnitVisualManager::SweepStaleInstances, 7.0f, true);
}

void UUnitVisualManager::AssignUnitVisual(FMassEntityHandle Entity, UInstancedStaticMeshComponent* TemplateISM, AMassUnitBase* Unit) {
	if (!TemplateISM || !TemplateISM->GetStaticMesh()) {
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) {
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	if (!VisualFrag) {
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
			if (Unit) Unit->CachePooledVisual(Existing.TargetISM.Get(), Existing.InstanceIndex);
				return; // Successfully reused/updated
		}
	}

	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Mesh, Material, bCastShadow);
	if (!ISM) {
		return;
	}

	FMeshMaterialKey Key;
	Key.Mesh = Mesh;
	Key.Material = Material;
	Key.bCastShadow = bCastShadow;

	int32 NewIndex = INDEX_NONE;
	if (FreeIndexPool.Contains(Key) && FreeIndexPool[Key].Num() > 0)
	{
		NewIndex = FreeIndexPool[Key].Pop();
		ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
	}
	else
	{
		// Create a new instance with zero scale to avoid flicker
		NewIndex = ISM->AddInstance(FTransform::Identity);
		ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
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
	// Never inherit a too-small value from the editor template. The animation processor needs 13
	// custom-data floats (indices 1..12); clamp up so a misconfigured template can't undersize the
	// pooled ISM and force a destructive runtime resize later (which would zero all instances).
	ISM->SetNumCustomDataFloats(FMath::Max(13, TemplateISM->NumCustomDataFloats));

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

	// Cache on the actor for entity-independent release in EndPlay.
	if (Unit) Unit->CachePooledVisual(ISM, NewIndex);
}

void UUnitVisualManager::RemoveUnitVisual(FMassEntityHandle Entity) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	// The actor that owns this entity, used as the reuse-guard identity in the shared free primitive.
	AMassUnitBase* OwnerActor = nullptr;
	if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity)) {
		OwnerActor = Cast<AMassUnitBase>(ActorFrag->GetMutable());
	}

	if (VisualFrag) {
		for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
			ReleasePooledInstanceInternal(Instance.TargetISM.Get(), Instance.InstanceIndex, OwnerActor);
		}
		VisualFrag->VisualInstances.Empty();
	}
}

void UUnitVisualManager::ReleasePooledInstanceInternal(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* ExpectedOwner) {
	if (!ISM || InstanceIndex == INDEX_NONE) return;

	// Reuse guard: if the slot currently belongs to a DIFFERENT live unit, pooling already reassigned this
	// index (indices are popped from FreeIndexPool and remapped on reuse). Freeing it would blank a live
	// unit's instance AND double-hand the index. Only proceed when the slot is our own, stale, or null.
	if (TArray<TWeakObjectPtr<AMassUnitBase>>* UnitArray = ISMToUnitMap.Find(ISM)) {
		if (UnitArray->IsValidIndex(InstanceIndex)) {
			AMassUnitBase* Cur = (*UnitArray)[InstanceIndex].Get();
			if (Cur != nullptr && Cur != ExpectedOwner) return;
			(*UnitArray)[InstanceIndex] = nullptr;
		}
	}

	// Hide (origin + zero scale) and return the index to the pool. AddUnique = idempotent double-free guard.
	const FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	ISM->UpdateInstanceTransform(InstanceIndex, HiddenTransform, true, true, true);

	FMeshMaterialKey Key;
	Key.Mesh = ISM->GetStaticMesh();
	Key.Material = ISM->GetMaterial(0);
	Key.bCastShadow = ISM->CastShadow;
	FreeIndexPool.FindOrAdd(Key).AddUnique(InstanceIndex);
}

void UUnitVisualManager::ReleaseInstanceDirect(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* Owner) {
	ReleasePooledInstanceInternal(ISM, InstanceIndex, Owner);
}

void UUnitVisualManager::SweepStaleInstances() {
	// Reclaim any pooled instance whose owning actor was destroyed without a prior successful free.
	for (TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, TArray<TWeakObjectPtr<AMassUnitBase>>>& Pair : ISMToUnitMap) {
		UInstancedStaticMeshComponent* ISM = Pair.Key.Get();
		if (!ISM) continue;
		TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = Pair.Value;
		for (int32 Index = 0; Index < UnitArray.Num(); ++Index) {
			// IsStale(true) is true ONLY for a slot that pointed at a now-destroyed actor — false for a
			// live unit (IsValid) and false for an explicitly-nulled/freed slot (plain null). So this only
			// ever reclaims genuine orphans and is fully idempotent.
			if (UnitArray[Index].IsStale(true)) {
				ReleasePooledInstanceInternal(ISM, Index, nullptr);
			}
		}
	}
}

void UUnitVisualManager::SetUnitVisualVisible(FMassEntityHandle Entity, bool bVisible) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
	FMassVisualEffectFragment* EffectFrag = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity);

	if (EffectFrag) {
		EffectFrag->bForceHidden = !bVisible;
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
    // Pre-size custom data ONCE, before any instance is added (so nothing is wiped). The animation
    // processor writes custom-data indices 1..12, i.e. it needs 13 floats. Doing this here means it
    // never has to resize a live, shared ISM at runtime (which would zero EVERY instance's data).
    NewISM->SetNumCustomDataFloats(13);
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
