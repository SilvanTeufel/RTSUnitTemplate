// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Subsystems/ResourceVisualManager.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "UObject/UObjectIterator.h"
#include "Engine/StaticMesh.h"

void UResourceVisualManager::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    // Try to find the VisualConfig Data Asset in the project
    // In a real project, this would be better assigned via a Developer Settings or a Manager Actor in the level
    for (TObjectIterator<UResourceVisualConfig> It; It; ++It) {
        if (It->GetWorld() == GetWorld() || !It->GetWorld()) {
            VisualConfig = *It;
            break;
        }
    }
    
    if (!VisualConfig) {
        // No config found, will use CDO fallbacks
    }
}

void UResourceVisualManager::AssignResource(FMassEntityHandle Entity, EResourceType ResourceType, TSubclassOf<AWorkResource> ResourceClass) {
    UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem) return;

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    FMassCarriedResourceFragment* ResourceFrag = EntityManager.GetFragmentDataPtr<FMassCarriedResourceFragment>(Entity);
    FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);

    if (!ResourceFrag) {
        return;
    }
    
    if (!ActorFrag) {
        return;
    }

    // Cleanup old if exists
    if (ResourceFrag->bIsCarrying) {
        RemoveResource(Entity);
    }

    AActor* RawActor = ActorFrag->GetMutable();
    if (!RawActor) {
        return;
    }

    AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(RawActor);
    if (!Worker) {
        return;
    }

    UStaticMesh* SelectedMesh = nullptr;
    UMaterialInterface* SelectedMaterial = nullptr;
    FVector SelectedScale = FVector(1.0f);
    FVector SelectedOffset = FVector::ZeroVector;
    bool bHasOverrideOffset = false;

    // 1. Check Worker Overrides
    int32 ResIndex = static_cast<int32>(ResourceType);
    if (Worker->WorkResourceVisuals.IsValidIndex(ResIndex) && Worker->WorkResourceVisuals[ResIndex].Mesh) {
        SelectedMesh = Worker->WorkResourceVisuals[ResIndex].Mesh;
        SelectedMaterial = Worker->WorkResourceVisuals[ResIndex].Material;
        SelectedScale = Worker->WorkResourceVisuals[ResIndex].Scale;
        SelectedOffset = Worker->WorkResourceVisuals[ResIndex].SocketOffset;
        if (!SelectedOffset.IsNearlyZero()) {
            bHasOverrideOffset = true;
        }
    }

    // 2. Check ResourceClass (from WorkArea)
    if (!SelectedMesh && ResourceClass) {
        if (AWorkResource* WRDefault = ResourceClass->GetDefaultObject<AWorkResource>()) {
            if (WRDefault->ResourceMeshes.Contains(ResourceType)) {
                SelectedMesh = WRDefault->ResourceMeshes[ResourceType];
            }
            
            if (!SelectedMesh && WRDefault->Mesh) {
                SelectedMesh = WRDefault->Mesh->GetStaticMesh();
            }

            if (SelectedMesh && WRDefault->Mesh) {
                SelectedScale = WRDefault->Mesh->GetRelativeScale3D();
            }

            if (WRDefault->ResourceMaterials.Contains(ResourceType)) {
                SelectedMaterial = WRDefault->ResourceMaterials[ResourceType];
            }
            
            if (!SelectedMaterial && WRDefault->Mesh) {
                SelectedMaterial = WRDefault->Mesh->GetMaterial(0);
            }

            if (!bHasOverrideOffset) {
                SelectedOffset = WRDefault->SocketOffset;
            }
        }
    }

    // 3. Check Global Config (Legacy fallback)
    if (!SelectedMesh && VisualConfig) {
        if (VisualConfig->DefaultResourceMeshes.Contains(ResourceType)) {
            SelectedMesh = VisualConfig->DefaultResourceMeshes[ResourceType];
        }
    }

    if (!SelectedMesh) {
        // 4. Fallback to AWorkResource base class defaults if nothing else found
        AWorkResource* DefaultResource = AWorkResource::StaticClass()->GetDefaultObject<AWorkResource>();
        if (DefaultResource) {
            if (DefaultResource->ResourceMeshes.Contains(ResourceType)) {
                SelectedMesh = DefaultResource->ResourceMeshes[ResourceType];
            }
            
            if (!SelectedMesh && DefaultResource->Mesh) {
                SelectedMesh = DefaultResource->Mesh->GetStaticMesh();
            }

            if (DefaultResource->ResourceMaterials.Contains(ResourceType)) {
                SelectedMaterial = DefaultResource->ResourceMaterials[ResourceType];
            }
            
            if (!SelectedMaterial && DefaultResource->Mesh) {
                SelectedMaterial = DefaultResource->Mesh->GetMaterial(0);
            }

            if (!bHasOverrideOffset) {
                SelectedOffset = DefaultResource->SocketOffset;
            }
        }
    }

    if (!SelectedMesh) {
        return;
    }

    UInstancedStaticMeshComponent* ISM = GetOrCreateISM(SelectedMesh, SelectedMaterial);
    if (!ISM) {
        return;
    }

    // Add new instance (Transform will be updated by Processor)
    // Initializing with zero scale to avoid flicker at origin
    int32 NewIndex = ISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector));
    
    ResourceFrag->InstanceIndex = NewIndex;
    ResourceFrag->TargetISM = ISM;
    ResourceFrag->bIsCarrying = true;
    ResourceFrag->ResourceType = ResourceType;
    ResourceFrag->ResourceScale = SelectedScale;
    ResourceFrag->SocketOffset = SelectedOffset;
    ResourceFrag->bWasVisible = false;
}

void UResourceVisualManager::RemoveResource(FMassEntityHandle Entity) {
    UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem) return;

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    FMassCarriedResourceFragment* ResourceFrag = EntityManager.GetFragmentDataPtr<FMassCarriedResourceFragment>(Entity);

    if (ResourceFrag && ResourceFrag->bIsCarrying && ResourceFrag->TargetISM.IsValid()) {
        // We set scale to 0 instead of RemoveInstance to avoid shifting indices for other entities
        ResourceFrag->TargetISM->UpdateInstanceTransform(ResourceFrag->InstanceIndex, FTransform::Identity, true, true, true);
        ResourceFrag->bIsCarrying = false;
        ResourceFrag->bWasVisible = false;
        // Optionally: reuse index in the future
    }
}

UInstancedStaticMeshComponent* UResourceVisualManager::GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow) {
    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material;
    Key.bCastShadow = bCastShadow;

    if (ISMPool.Contains(Key)) {
        return ISMPool[Key];
    }

    AActor* ManagerActor = nullptr;
    for (TObjectIterator<AActor> It; It; ++It) {
        if (It->GetWorld() == GetWorld() && It->GetName().Contains(TEXT("ResourceISMManagerActor"))) {
            ManagerActor = *It;
            break;
        }
    }

    if (!ManagerActor) {
        ManagerActor = GetWorld()->SpawnActor<AActor>();
        ManagerActor->SetFlags(RF_Transient);
        
        // Ensure the actor has a root component for proper placement and visibility
        USceneComponent* Root = NewObject<USceneComponent>(ManagerActor, TEXT("Root"));
        ManagerActor->SetRootComponent(Root);
        Root->RegisterComponent();

#if WITH_EDITOR
        ManagerActor->SetActorLabel(TEXT("ResourceISMManagerActor"));
#endif
    }

    UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(ManagerActor);
    NewISM->SetStaticMesh(Mesh);
    if (Material) {
        NewISM->SetMaterial(0, Material);
    }
    
    NewISM->SetMobility(EComponentMobility::Movable);
    NewISM->SetCastShadow(bCastShadow);
    
    // Disable collisions as requested to avoid movement issues
    NewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NewISM->SetCollisionResponseToAllChannels(ECR_Ignore);
    NewISM->SetGenerateOverlapEvents(false);
    NewISM->SetCanEverAffectNavigation(false);

    NewISM->AttachToComponent(ManagerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    NewISM->RegisterComponent();
    
    ISMPool.Add(Key, NewISM);

    return NewISM;
}
