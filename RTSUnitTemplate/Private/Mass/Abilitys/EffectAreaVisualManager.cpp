#include "Mass/Abilitys/EffectAreaVisualManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Actors/EffectArea.h"
#include "Mass/UnitMassTag.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassCommonFragments.h"

void UEffectAreaVisualManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UEffectAreaVisualManager::Deinitialize()
{
    Super::Deinitialize();
}

AActor* UEffectAreaVisualManager::GetOrCreateManagerActor()
{
    if (IsValid(ManagerActor))
    {
        return ManagerActor;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ManagerActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    
#if WITH_EDITOR
    if (ManagerActor)
    {
        ManagerActor->SetActorLabel(TEXT("EffectAreaVisualManagerActor"));
    }
#endif

    return ManagerActor;
}

UInstancedStaticMeshComponent* UEffectAreaVisualManager::GetOrCreatePooledISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow)
{
    if (!Mesh) return nullptr;

    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material ? Material : Mesh->GetMaterial(0);
    Key.bCastShadow = bCastShadow;

    if (ISMPool.Contains(Key))
    {
        return ISMPool[Key];
    }

    AActor* Owner = GetOrCreateManagerActor();
    if (!Owner) return nullptr;

    UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Owner);
    ISM->SetStaticMesh(Mesh);
    if (Material)
    {
        ISM->SetMaterial(0, Material);
    }
    ISM->SetCastShadow(bCastShadow);
    ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ISM->RegisterComponent();

    ISMPool.Add(Key, ISM);
    return ISM;
}

void UEffectAreaVisualManager::AddVisualInstance(FMassEntityHandle EntityHandle, AEffectArea* EffectAreaActor)
{
    if (!EffectAreaActor || !EntityHandle.IsValid()) return;

    UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem) return;

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    FEffectAreaVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FEffectAreaVisualFragment>(EntityHandle);
    if (!VisualFrag) return;

    UStaticMesh* Mesh = nullptr;
    UMaterialInterface* Material = nullptr;

    if (EffectAreaActor->ISMTemplate)
    {
        Mesh = EffectAreaActor->ISMTemplate->GetStaticMesh();
        Material = EffectAreaActor->ISMTemplate->GetMaterial(0);
    }

    if (!Mesh) return;

    UInstancedStaticMeshComponent* ISM = GetOrCreatePooledISM(Mesh, Material, false);
    if (!ISM) return;

    VisualFrag->ISMComponent = ISM;
    // Add instance with zero scale to avoid flickering before the first processor update
    VisualFrag->InstanceIndex = ISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector));
    FTransform VisualOffsetTransform(EffectAreaActor->VisualRotationOffset);

    VisualFrag->VisualRelativeTransform = (EffectAreaActor->ISMTemplate ? EffectAreaActor->ISMTemplate->GetRelativeTransform() : FTransform::Identity) * VisualOffsetTransform;
    VisualFrag->BaseMeshRadius = Mesh->GetBounds().BoxExtent.X;
    
    if (EffectAreaActor->Niagara_A)
    {
        VisualFrag->Niagara_A = EffectAreaActor->Niagara_A;
        VisualFrag->Niagara_A_RelativeTransform = EffectAreaActor->Niagara_A->GetRelativeTransform() * VisualOffsetTransform;
    }

    // Clear the template after registering the visual instance
    if (EffectAreaActor->ISMTemplate && GetWorld()->IsGameWorld())
    {
        EffectAreaActor->ISMTemplate->SetStaticMesh(nullptr);
        EffectAreaActor->ISMTemplate->DestroyComponent();
        EffectAreaActor->ISMTemplate = nullptr;
    }
}

void UEffectAreaVisualManager::RemoveVisualInstance(FMassEntityHandle EntityHandle)
{
    if (!EntityHandle.IsValid()) return;

    UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem) return;

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    FEffectAreaVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FEffectAreaVisualFragment>(EntityHandle);
    
    if (VisualFrag && VisualFrag->ISMComponent.IsValid() && VisualFrag->InstanceIndex != INDEX_NONE)
    {
        // Hide instance by setting scale to zero
        VisualFrag->ISMComponent->UpdateInstanceTransform(VisualFrag->InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
        VisualFrag->InstanceIndex = INDEX_NONE;
    }
}
