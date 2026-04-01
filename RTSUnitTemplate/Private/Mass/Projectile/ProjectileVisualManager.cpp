#include "Mass/Projectile/ProjectileVisualManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Actors/Projectile.h"
#include "Actors/EnergyWall.h"
#include "Mass/UnitMassTag.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassCommonFragments.h"
#include "UObject/UObjectIterator.h"

void UProjectileVisualManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		if (UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>())
		{
			EntityManager = &MassSubsystem->GetMutableEntityManager();
		}
	}
}

void UProjectileVisualManager::Deinitialize()
{
	Super::Deinitialize();
}

UInstancedStaticMeshComponent* UProjectileVisualManager::GetOrCreateISMComponent(TSubclassOf<AProjectile> ProjectileClass)
{
	if (!ProjectileClass) return nullptr;

	AProjectile* ProjectileCDO = ProjectileClass->GetDefaultObject<AProjectile>();
	
    // Ensure we register the CDO even if there is no ISM
    if (!ProjectileDataMap.Contains(ProjectileClass))
    {
        FProjectileClassData& Data = ProjectileDataMap.Add(ProjectileClass);
        Data.CDO = ProjectileCDO;
    }

    if (!ProjectileCDO || !ProjectileCDO->ISMComponent) return nullptr;

    UStaticMesh* Mesh = ProjectileCDO->ISMComponent->GetStaticMesh();
    UMaterialInterface* Material = ProjectileCDO->ISMComponent->GetMaterial(0);
    bool bCastShadow = ProjectileCDO->ISMComponent->CastShadow;

    if (!Mesh) return nullptr;

    return GetOrCreatePooledISM(Mesh, Material, bCastShadow);
}

AActor* UProjectileVisualManager::GetOrCreateManagerActor()
{
    AActor* ManagerActor = nullptr;
    for (TObjectIterator<AActor> It; It; ++It)
    {
        if (It->GetWorld() == GetWorld() && (It->ActorHasTag(TEXT("ProjectileVisualISMManager")) || It->GetName().Contains(TEXT("ProjectileVisualISMManagerActor"))))
        {
            ManagerActor = *It;
            break;
        }
    }

    if (!ManagerActor)
    {
        ManagerActor = GetWorld()->SpawnActor<AActor>();
        if (GetWorld()->GetNetMode() == NM_Client)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Created NEW ProjectileVisualISMManagerActor"));
        }
        ManagerActor->Tags.Add(TEXT("ProjectileVisualISMManager"));
        ManagerActor->SetFlags(RF_Transient);
        ManagerActor->SetHidden(false);
        ManagerActor->SetCanBeDamaged(false);
        
        USceneComponent* Root = NewObject<USceneComponent>(ManagerActor, TEXT("Root"));
        ManagerActor->SetRootComponent(Root);
        Root->RegisterComponent();
        Root->SetVisibility(true);

#if WITH_EDITOR
        ManagerActor->SetActorLabel(TEXT("ProjectileVisualISMManagerActor"));
#endif
    }
    return ManagerActor;
}

UInstancedStaticMeshComponent* UProjectileVisualManager::GetOrCreatePooledISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow)
{
    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material;
    Key.bCastShadow = bCastShadow;

    if (ISMPool.Contains(Key))
    {
        return ISMPool[Key];
    }

    AActor* ManagerActor = GetOrCreateManagerActor();

    UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(ManagerActor);
    NewISM->SetStaticMesh(Mesh);
    if (Material)
    {
        NewISM->SetMaterial(0, Material);
    }
    
    NewISM->SetMobility(EComponentMobility::Movable);
    NewISM->SetCastShadow(bCastShadow);
    
    NewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NewISM->SetCollisionResponseToAllChannels(ECR_Ignore);
    NewISM->SetGenerateOverlapEvents(false);
    NewISM->SetCanEverAffectNavigation(false);

    NewISM->SetVisibility(true);
    NewISM->SetHiddenInGame(false);
    NewISM->AttachToComponent(ManagerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    NewISM->RegisterComponent();
    
    UE_LOG(LogTemp, Log, TEXT("Created NEW pooled ISM for Mesh: %s, Material: %s on [NetMode: %d]"), 
        Mesh ? *Mesh->GetName() : TEXT("None"), 
        Material ? *Material->GetName() : TEXT("None"),
        (int32)GetWorld()->GetNetMode());

    ISMPool.Add(Key, NewISM);

    return NewISM;
}

const AProjectile* UProjectileVisualManager::GetProjectileCDO(TSubclassOf<AProjectile> ProjectileClass)
{
	if (!ProjectileClass) return nullptr;
    
	if (FProjectileClassData* ExistingData = ProjectileDataMap.Find(ProjectileClass))
	{
		return ExistingData->CDO;
	}
    
	return Cast<AProjectile>(ProjectileClass->GetDefaultObject());
}

FMassEntityHandle UProjectileVisualManager::SpawnMassProjectile(TSubclassOf<AProjectile> ProjectileClass, const FTransform& Transform, AActor* Shooter, AActor* Target, FVector TargetLocation, FMassEntityHandle ShooterEntity, FMassEntityHandle TargetEntity, float ProjectileSpeed, int32 ShooterTeamId)
{
    UWorld* World = GetWorld();
    bool bIsClient = World && World->GetNetMode() == NM_Client;

    if (bIsClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CLIENT] SpawnMassProjectile [NetMode: %d] in World: %s"), (int32)NM_Client, World ? *World->GetName() : TEXT("None"));
    }

    // Lazy-Initialisierung des EntityManager (besonders wichtig auf Clients)
    if (!EntityManager && World)
    {
        if (UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>())
        {
            EntityManager = &MassSubsystem->GetMutableEntityManager();
            if (bIsClient)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Successfully resolved EntityManager during Spawn."));
            }
        }
    }

	if (!EntityManager || !ProjectileClass)
    {
        if (bIsClient)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CLIENT] SpawnMassProjectile failed: EntityManager %p, ProjectileClass %s"), EntityManager, ProjectileClass ? *ProjectileClass->GetName() : TEXT("None"));
        }
        return FMassEntityHandle();
    }

	UInstancedStaticMeshComponent* ISM = GetOrCreateISMComponent(ProjectileClass);
    AActor* ManagerActor = GetOrCreateManagerActor();

	// Ensure archetype is created
	if (!ProjectileArchetype.IsValid())
	{
        UE_LOG(LogTemp, Log, TEXT("Creating Projectile Archetype"));
		TArray<const UScriptStruct*> Fragments;
		Fragments.Add(FTransformFragment::StaticStruct());
		Fragments.Add(FMassProjectileFragment::StaticStruct());
		Fragments.Add(FMassProjectileVisualFragment::StaticStruct());
        
		TArray<const UScriptStruct*> Tags;
		Tags.Add(FMassProjectileTag::StaticStruct());
		Tags.Add(FMassProjectileActiveTag::StaticStruct());
        
		TArray<const UScriptStruct*> AllFragmentsAndTags;
		AllFragmentsAndTags.Append(Fragments);
		AllFragmentsAndTags.Append(Tags);

		ProjectileArchetype = EntityManager->CreateArchetype(AllFragmentsAndTags);
		UE_LOG(LogTemp, Log, TEXT("Projectile Archetype Created."));
	}

	// Create Mass Entity
	FMassEntityHandle Entity = EntityManager->CreateEntity(ProjectileArchetype);

    if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
    {
	    UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Entity Created: %d"), Entity.Index);

	    if (EntityManager->IsEntityValid(Entity))
	    {
		    bool bHasTransform = DoesEntityHaveFragment<FTransformFragment>(*EntityManager, Entity);
		    bool bHasProjectile = DoesEntityHaveFragment<FMassProjectileFragment>(*EntityManager, Entity);
		    bool bHasActiveTag = DoesEntityHaveTag(*EntityManager, Entity, FMassProjectileActiveTag::StaticStruct());
		    UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Entity %d Status: Transform:%d, Projectile:%d, ActiveTag:%d"), Entity.Index, bHasTransform, bHasProjectile, bHasActiveTag);
	    }
	    else
	    {
		    UE_LOG(LogTemp, Error, TEXT("[CLIENT] Entity %d is NOT Valid immediately after creation!"), Entity.Index);
	    }
    }

	// Initialize fragments
	FTransformFragment& TransformFragment = EntityManager->GetFragmentDataChecked<FTransformFragment>(Entity);
	TransformFragment.SetTransform(Transform);

	const AProjectile* CDO = GetProjectileCDO(ProjectileClass);

	FMassProjectileFragment& ProjectileFragment = EntityManager->GetFragmentDataChecked<FMassProjectileFragment>(Entity);
	ProjectileFragment.ProjectileClass = ProjectileClass;
	ProjectileFragment.TargetLocation = TargetLocation;
	ProjectileFragment.Speed = (ProjectileSpeed > 0.f) ? ProjectileSpeed : CDO->MovementSpeed;
	ProjectileFragment.Damage = CDO->Damage;
	ProjectileFragment.MaxLifeTime = CDO->MaxLifeTime;
	ProjectileFragment.LifeTime = 0.f;
	ProjectileFragment.ArcHeight = CDO->ArcHeight;
	ProjectileFragment.ArcStartLocation = Transform.GetLocation();
	ProjectileFragment.bFollowTarget = CDO->FollowTarget;
	ProjectileFragment.TeamId = (ShooterTeamId != -1) ? ShooterTeamId : CDO->TeamId;
	ProjectileFragment.IsHealing = CDO->IsHealing;
	ProjectileFragment.MaxPiercedTargets = CDO->MaxPiercedTargets;

	// Homing and Rotation parameters from CDO
	ProjectileFragment.bIsHoming = CDO->HomingMissleCount > 0;
	if (ProjectileFragment.bIsHoming)
	{
		ProjectileFragment.HomingRotationSpeed = CDO->HomingRotationSpeed * FMath::RandRange(0.8f, 2.0f);
		if (FMath::RandBool()) ProjectileFragment.HomingRotationSpeed *= -1.f;
		ProjectileFragment.HomingInitialAngle = FMath::RandRange(0.f, 360.f);
		ProjectileFragment.HomingMaxSpiralRadius = CDO->HomingMaxSpiralRadius * FMath::RandRange(0.6f, 1.5f);
		ProjectileFragment.Speed += FMath::RandRange(-CDO->HomingSpeedVariation, CDO->HomingSpeedVariation);
	}
	else
	{
		ProjectileFragment.HomingRotationSpeed = CDO->HomingRotationSpeed;
		ProjectileFragment.HomingMaxSpiralRadius = CDO->HomingMaxSpiralRadius;
		ProjectileFragment.HomingInitialAngle = CDO->HomingInitialAngle;
	}
	ProjectileFragment.HomingInterpSpeed = CDO->HomingInterpSpeed;

	ProjectileFragment.RotationOffset = CDO->RotationOffset;
	ProjectileFragment.RotationSpeed = CDO->RotationSpeed;
	ProjectileFragment.bDisableAnyRotation = CDO->DisableAnyRotation;
	ProjectileFragment.bRotateMesh = CDO->RotateMesh;
	
	// EnergyWall Interaktion
	ProjectileFragment.bCanBeRepelledByEnergyWall = CDO->bCanBeRepelledByEnergyWall;
	if (ProjectileFragment.bCanBeRepelledByEnergyWall)
	{
		FHitResult WallHit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Shooter);
		if (ISM && ISM->GetOwner()) Params.AddIgnoredActor(ISM->GetOwner());

		FVector TraceStart = Transform.GetLocation();
		FVector TraceEnd = TargetLocation;
		
		// Falls kein TargetLocation (z.B. Aiming an eine Stelle im Raum ohne Actor)
		if (TraceEnd.IsNearlyZero()) {
			TraceEnd = TraceStart + Transform.GetRotation().GetForwardVector() * 10000.f;
		}

		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_WorldDynamic, Params))
		{
			if (AEnergyWall* EnergyWall = Cast<AEnergyWall>(WallHit.GetActor()))
			{
				ProjectileFragment.bHasWallImpact = true;
				ProjectileFragment.WallImpactLocation = WallHit.ImpactPoint;
				ProjectileFragment.WallActor = EnergyWall;
				UE_LOG(LogTemp, Log, TEXT("SpawnMassProjectile: Wall Impact predicted at %s"), *WallHit.ImpactPoint.ToString());
			}
		}
	}

	// Entity-Handles vom Server sind auf dem Client nicht gültig. Wir müssen sie lokal auflösen.
	FMassEntityHandle ResolvedShooterEntity = ShooterEntity;
	if (bIsClient || !ResolvedShooterEntity.IsValid())
	{
		if (Shooter)
		{
			if (UMassActorBindingComponent* Binding = Shooter->FindComponentByClass<UMassActorBindingComponent>())
			{
				ResolvedShooterEntity = Binding->GetEntityHandle();
				if (bIsClient && ResolvedShooterEntity.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Resolved local ShooterEntity for projectile."));
				}
				else if (!bIsClient && ResolvedShooterEntity.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[SERVER] Resolved local ShooterEntity for projectile."));
				}
			}
		}
	}
	ProjectileFragment.ShooterEntity = ResolvedShooterEntity;

	FMassEntityHandle ResolvedTargetEntity = TargetEntity;
	if (bIsClient || !ResolvedTargetEntity.IsValid())
	{
		if (Target)
		{
			if (UMassActorBindingComponent* Binding = Target->FindComponentByClass<UMassActorBindingComponent>())
			{
				ResolvedTargetEntity = Binding->GetEntityHandle();
				if (bIsClient && ResolvedTargetEntity.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Resolved local TargetEntity for projectile."));
				}
				else if (!bIsClient && ResolvedTargetEntity.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[SERVER] Resolved local TargetEntity for projectile."));
				}
			}
		}
	}
	ProjectileFragment.TargetEntity = ResolvedTargetEntity;

	FMassProjectileVisualFragment& VisualFragment = EntityManager->GetFragmentDataChecked<FMassProjectileVisualFragment>(Entity);
	VisualFragment.ProjectileClass = ProjectileClass;
	VisualFragment.VisualRelativeTransform = CDO->ISMComponent ? CDO->ISMComponent->GetRelativeTransform() : FTransform::Identity;
    
    if (ISM)
    {
		// Initial Transform: Relative Offset * Entity Transform
		FTransform InitialTransform = VisualFragment.VisualRelativeTransform * Transform;
        VisualFragment.InstanceIndex = ISM->AddInstance(InitialTransform, true);
        VisualFragment.ISMComponent = ISM;
    }
    else
    {
        VisualFragment.InstanceIndex = INDEX_NONE;
        VisualFragment.ISMComponent = nullptr;
    }

	VisualFragment.Niagara_A_RelativeTransform = (CDO->Niagara_A) ? CDO->Niagara_A->GetRelativeTransform() : CDO->Niagara_A_Start_Transform;
	VisualFragment.Niagara_B_RelativeTransform = (CDO->Niagara_B) ? CDO->Niagara_B->GetRelativeTransform() : CDO->Niagara_B_Start_Transform;

	if (bIsClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Niagara_A Offset Variable: %s"), *CDO->Niagara_A_Start_Transform.ToString());
		if (CDO->Niagara_A) UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Niagara_A Component Relative: %s"), *CDO->Niagara_A->GetRelativeTransform().ToString());
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Niagara_B Offset Variable: %s"), *CDO->Niagara_B_Start_Transform.ToString());
		if (CDO->Niagara_B) UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Niagara_B Component Relative: %s"), *CDO->Niagara_B->GetRelativeTransform().ToString());
	}

	// Niagara initialization
	if (ManagerActor)
	{
		if (CDO->Niagara_A && CDO->Niagara_A->GetAsset())
		{
			// Berechne initiale Welt-Position unter Berücksichtigung des Offsets
			FTransform InitialTransform = VisualFragment.Niagara_A_RelativeTransform * Transform;
			UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(CDO->Niagara_A->GetAsset(), ManagerActor->GetRootComponent(), NAME_None, InitialTransform.GetLocation(), InitialTransform.Rotator(), EAttachLocation::KeepWorldPosition, false);
			if (NC)
			{
				NC->SetWorldTransform(InitialTransform);
				VisualFragment.Niagara_A = NC;
			}
		}
		if (CDO->Niagara_B && CDO->Niagara_B->GetAsset())
		{
			// Berechne initiale Welt-Position unter Berücksichtigung des Offsets
			FTransform InitialTransform = VisualFragment.Niagara_B_RelativeTransform * Transform;
			UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(CDO->Niagara_B->GetAsset(), ManagerActor->GetRootComponent(), NAME_None, InitialTransform.GetLocation(), InitialTransform.Rotator(), EAttachLocation::KeepWorldPosition, false);
			if (NC)
			{
				NC->SetWorldTransform(InitialTransform);
				VisualFragment.Niagara_B = NC;
			}
		}
	}

   	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CLIENT] SpawnMassProjectile: Created Entity %d for class %s (ISM Index %d)"), Entity.Index, *ProjectileClass->GetName(), VisualFragment.InstanceIndex);
    }
    
	return Entity;
}
