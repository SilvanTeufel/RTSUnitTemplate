#include "Mass/Projectile/ProjectileVisualManager.h"
#include "MassCommandBuffer.h"
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
#include "LandscapeProxy.h"
#include "Landscape.h"

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

FTransform UProjectileVisualManager::GetProjectileTransform(FMassEntityHandle EntityHandle) const
{
    if (!EntityManager || !EntityHandle.IsValid())
    {
        return FTransform::Identity;
    }

    if (const FTransformFragment* TransformFragment = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle))
    {
        return TransformFragment->GetTransform();
    }

    return FTransform::Identity;
}

UInstancedStaticMeshComponent* UProjectileVisualManager::GetVisualISM(UInstancedStaticMeshComponent* TemplateISM)
{
    if (!TemplateISM) return nullptr;

    UStaticMesh* Mesh = TemplateISM->GetStaticMesh();
    UMaterialInterface* Material = TemplateISM->GetMaterial(0);
    bool bCastShadow = TemplateISM->CastShadow;

    if (!Mesh) return nullptr;

    return GetOrCreatePooledISM(Mesh, Material, bCastShadow);
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

FMassEntityHandle UProjectileVisualManager::SpawnMassProjectile(TSubclassOf<AProjectile> ProjectileClass, const FTransform& Transform, AActor* Shooter, AActor* Target, FVector TargetLocation, FMassEntityHandle ShooterEntity, FMassEntityHandle TargetEntity, float ProjectileSpeed, int32 ShooterTeamId, bool bFollowTarget, float HomingInitialAngle, float HomingRotationSpeed, float HomingMaxSpiralRadius, float HomingInterpSpeed, FMassCommandBuffer* CommandBuffer, FVector Scale, float Damage, int32 MaxPiercedTargets)
{
    UWorld* World = GetWorld();
    bool bIsClient = World && World->GetNetMode() == NM_Client;

    // Lazy-Initialisierung des EntityManager (besonders wichtig auf Clients)
    if (!EntityManager && World)
    {
        if (UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>())
        {
            EntityManager = &MassSubsystem->GetMutableEntityManager();
        }
    }

	if (!EntityManager || !ProjectileClass)
    {
        return FMassEntityHandle();
    }

    // Defer the call if we are processing or if a command buffer is provided
    if (CommandBuffer || EntityManager->IsProcessing())
    {
        FMassCommandBuffer& TargetBuffer = CommandBuffer ? *CommandBuffer : EntityManager->Defer();
        
        TargetBuffer.PushCommand<FMassDeferredSetCommand>([this, ProjectileClass, Transform, Shooter, Target, TargetLocation, ShooterEntity, TargetEntity, ProjectileSpeed, ShooterTeamId, bFollowTarget, HomingInitialAngle, HomingRotationSpeed, HomingMaxSpiralRadius, HomingInterpSpeed, Scale, Damage, MaxPiercedTargets](FMassEntityManager& Manager)
        {
            // This will run in a safe state (not processing) on the main thread
            SpawnMassProjectile(ProjectileClass, Transform, Shooter, Target, TargetLocation, ShooterEntity, TargetEntity, ProjectileSpeed, ShooterTeamId, bFollowTarget, HomingInitialAngle, HomingRotationSpeed, HomingMaxSpiralRadius, HomingInterpSpeed, nullptr, Scale, Damage, MaxPiercedTargets);
        });
        return FMassEntityHandle();
    }

	UInstancedStaticMeshComponent* ISM = GetOrCreateISMComponent(ProjectileClass);
    AActor* ManagerActor = GetOrCreateManagerActor();

	// Ensure archetype is created - Note: CreateArchetype must be done on main thread usually or protected
	if (!ProjectileArchetype.IsValid())
	{
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
	}

	// Create Mass Entity
	FMassEntityHandle Entity = EntityManager->CreateEntity(ProjectileArchetype);

	// Initialize fragments
	FTransformFragment TransformFragment;
    FTransform ScaledTransform = Transform;
    ScaledTransform.SetScale3D(Scale);
	TransformFragment.SetTransform(ScaledTransform);

	const AProjectile* CDO = GetProjectileCDO(ProjectileClass);

	FMassProjectileFragment ProjectileFragment;
	ProjectileFragment.ProjectileClass = ProjectileClass;
	ProjectileFragment.TargetLocation = TargetLocation;
	ProjectileFragment.Speed = ProjectileSpeed;
	ProjectileFragment.Damage = (Damage >= 0.f) ? Damage : CDO->Damage;
	ProjectileFragment.CollisionRadius = CDO->CollisionRadius;
	ProjectileFragment.MaxLifeTime = CDO->MaxLifeTime;
	ProjectileFragment.LifeTime = 0.f;
	ProjectileFragment.WorldContext = World;
    ProjectileFragment.bEnableLandscapeHit = CDO->bEnableLandscapeHit;
	ProjectileFragment.ArcHeight = CDO->ArcHeight;
    ProjectileFragment.ArcHeightDistanceFactor = CDO->ArcHeightDistanceFactor;
	ProjectileFragment.ArcStartLocation = Transform.GetLocation();
	ProjectileFragment.bFollowTarget = bFollowTarget;
	ProjectileFragment.TeamId = (ShooterTeamId != -1) ? ShooterTeamId : CDO->TeamId;
	ProjectileFragment.IsHealing = CDO->IsHealing;
	ProjectileFragment.MaxPiercedTargets = (MaxPiercedTargets >= 0) ? MaxPiercedTargets : CDO->MaxPiercedTargets;
    ProjectileFragment.ProjectileScale = Scale;
    ProjectileFragment.bContinueAfterTarget = CDO->bContinueAfterTarget;
    ProjectileFragment.FlightDirection = (TargetLocation - Transform.GetLocation()).GetSafeNormal();
    if (ProjectileFragment.FlightDirection.IsNearlyZero())
    {
        ProjectileFragment.FlightDirection = Transform.GetRotation().GetForwardVector();
    }

	// Homing and Rotation parameters from server
	ProjectileFragment.bIsHoming = bFollowTarget && CDO->HomingMissleCount > 0;
	ProjectileFragment.HomingInitialAngle = HomingInitialAngle;
	ProjectileFragment.HomingRotationSpeed = HomingRotationSpeed;
	ProjectileFragment.HomingMaxSpiralRadius = HomingMaxSpiralRadius;
	ProjectileFragment.HomingInterpSpeed = HomingInterpSpeed;

	ProjectileFragment.RotationOffset = CDO->RotationOffset;
	ProjectileFragment.RotationSpeed = CDO->RotationSpeed;
	ProjectileFragment.bDisableAnyRotation = CDO->DisableAnyRotation;
	ProjectileFragment.bRotateMesh = CDO->RotateMesh;

	// Trigger BeginSpawn on CDO
	if (AProjectile* MutableCDO = const_cast<AProjectile*>(CDO))
	{
		MutableCDO->BeginSpawn(ScaledTransform, World);
	}
	
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
			}
		}
	}

	// Landscape Collision Check (Optimization: trace once at start)
	if (ProjectileFragment.bEnableLandscapeHit && GetWorld())
	{
		TArray<FHitResult> HitResults;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Shooter);
		if (ManagerActor) Params.AddIgnoredActor(ManagerActor);

		FVector TraceStart = Transform.GetLocation();
		// Trace far ahead (beyond max life time distance)
		FVector TraceEnd = TraceStart + ProjectileFragment.FlightDirection * (ProjectileFragment.Speed * ProjectileFragment.MaxLifeTime * 1.5f);

		if (GetWorld()->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			for (const FHitResult& Hit : HitResults)
			{
				if (Hit.GetActor() && (Hit.GetActor()->IsA<ALandscapeProxy>() || Hit.GetActor()->IsA<ALandscape>()))
				{
					ProjectileFragment.bHasLandscapeImpact = true;
					ProjectileFragment.LandscapeImpactLocation = Hit.ImpactPoint;
					break; // Found it
				}
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
			}
		}
	}
	ProjectileFragment.TargetEntity = ResolvedTargetEntity;

	FMassProjectileVisualFragment VisualFragment;
	VisualFragment.ProjectileClass = ProjectileClass;
	VisualFragment.VisualRelativeTransform = CDO->ISMComponent ? CDO->ISMComponent->GetRelativeTransform() : FTransform::Identity;
    
    if (ISM)
    {
		// Initial Transform: Relative Offset * Entity Transform
		FTransform InitialTransform = VisualFragment.VisualRelativeTransform * ScaledTransform;
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

	// Niagara initialization
	if (ManagerActor)
	{
		if (CDO->Niagara_A && CDO->Niagara_A->GetAsset())
		{
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
			FTransform InitialTransform = VisualFragment.Niagara_B_RelativeTransform * Transform;
			UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(CDO->Niagara_B->GetAsset(), ManagerActor->GetRootComponent(), NAME_None, InitialTransform.GetLocation(), InitialTransform.Rotator(), EAttachLocation::KeepWorldPosition, false);
			if (NC)
			{
				NC->SetWorldTransform(InitialTransform);
				VisualFragment.Niagara_B = NC;
			}
		}
	}

    // Apply fragments
    EntityManager->GetFragmentDataChecked<FTransformFragment>(Entity) = TransformFragment;
    EntityManager->GetFragmentDataChecked<FMassProjectileFragment>(Entity) = ProjectileFragment;
    EntityManager->GetFragmentDataChecked<FMassProjectileVisualFragment>(Entity) = VisualFragment;
    
	return Entity;
}
