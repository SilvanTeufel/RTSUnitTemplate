// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "System/PlayerTeamSubsystem.h"
#include "Engine/GameInstance.h"
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
#include "DrawDebugHelpers.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"

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

void UProjectileVisualManager::FireSpawnEffects(const AProjectile* CDO, const FTransform& SpawnTransform, bool bVisible)
{
	if (!CDO || !bVisible) return;
	if (!CDO->SpawnVFX && !CDO->SpawnSound) return;

	// Never CDO->GetWorld() - the CDO is worldless (see Projectile.h). Use the subsystem's world.
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;

	const FVector Loc = SpawnTransform.GetLocation();
	// Quaternion composition, not FRotator addition: adding components is not composing rotations
	// and diverges once pitch and yaw combine - which aimed projectiles always do.
	const FRotator Rot = (SpawnTransform.GetRotation() * CDO->RotateSpawnVFX.Quaternion()).Rotator();
	const float KillDelay = CDO->SpawnEffectKillDelay;

	if (CDO->SpawnVFX)
	{
		// bAutoDestroy (default true) already covers one-shot systems; the kill delay is the backstop
		// for a looping one. Deliberately not registered in APerformanceUnit::ActiveNiagara - that
		// array is only ever emptied by StopAllEffects, so a per-shot effect would accumulate there.
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CDO->SpawnVFX, Loc, Rot, CDO->ScaleSpawnVFX))
		{
			if (KillDelay > 0.f)
			{
				TWeakObjectPtr<UNiagaraComponent> WeakNC = NC;
				FTimerHandle TimerHandle;
				World->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([WeakNC]()
				{
					if (UNiagaraComponent* Comp = WeakNC.Get())
					{
						Comp->Deactivate();
						Comp->DestroyComponent();
					}
				}), KillDelay, false);
			}
		}
	}

	if (CDO->SpawnSound)
	{
		float Multiplier = CDO->ScaleSpawnSound;
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (APlayerController* PC = GI->GetFirstLocalPlayerController())
			{
				if (AControllerBase* ControllerBase = Cast<AControllerBase>(PC))
				{
					Multiplier *= ControllerBase->GetSoundMultiplier();
				}
			}
		}

		if (UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(World, CDO->SpawnSound, Loc, Rot, Multiplier))
		{
			if (KillDelay > 0.f)
			{
				TWeakObjectPtr<UAudioComponent> WeakAC = AC;
				FTimerHandle TimerHandle;
				World->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([WeakAC]()
				{
					if (UAudioComponent* Comp = WeakAC.Get())
					{
						Comp->Stop();
						Comp->DestroyComponent();
					}
				}), KillDelay, false);
			}
		}
	}
}

FMassEntityHandle UProjectileVisualManager::SpawnMassProjectile(TSubclassOf<AProjectile> ProjectileClass, const FTransform& Transform, AActor* Shooter, AActor* Target, FVector TargetLocation, FMassEntityHandle ShooterEntity, FMassEntityHandle TargetEntity, float ProjectileSpeed, int32 ShooterTeamId, bool bFollowTarget, float HomingInitialAngle, float HomingRotationSpeed, float HomingMaxSpiralRadius, float HomingInterpSpeed, FMassCommandBuffer* CommandBuffer, FVector Scale, float Damage, int32 MaxPiercedTargets, bool bIsPredicted, TSubclassOf<class UGameplayEffect> ProjectileEffect, TSubclassOf<class UGameplayEffect> ProjectileEffect2, TSubclassOf<class UGameplayEffect> ProjectileEffect3, FEffectAreaInfo AreaInfo)
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
        
        TargetBuffer.PushCommand<FMassDeferredSetCommand>([this, ProjectileClass, Transform, Shooter, Target, TargetLocation, ShooterEntity, TargetEntity, ProjectileSpeed, ShooterTeamId, bFollowTarget, HomingInitialAngle, HomingRotationSpeed, HomingMaxSpiralRadius, HomingInterpSpeed, Scale, Damage, MaxPiercedTargets, bIsPredicted, ProjectileEffect, ProjectileEffect2, ProjectileEffect3, AreaInfo](FMassEntityManager& Manager)
        {
            // This will run in a safe state (not processing) on the main thread
            SpawnMassProjectile(ProjectileClass, Transform, Shooter, Target, TargetLocation, ShooterEntity, TargetEntity, ProjectileSpeed, ShooterTeamId, bFollowTarget, HomingInitialAngle, HomingRotationSpeed, HomingMaxSpiralRadius, HomingInterpSpeed, nullptr, Scale, Damage, MaxPiercedTargets, bIsPredicted, ProjectileEffect, ProjectileEffect2, ProjectileEffect3, AreaInfo);
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
		Fragments.Add(FMassVisibilityFragment::StaticStruct());
		Fragments.Add(FMassCombatStatsFragment::StaticStruct());
		Fragments.Add(FMassSightFragment::StaticStruct());
		Fragments.Add(FMassAgentCharacteristicsFragment::StaticStruct());
		Fragments.Add(FMassAllianceFragment::StaticStruct());
        
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

	// Entity-Handles vom Server sind auf dem Client nicht gültig. Wir müssen sie lokal auflösen.
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

	FVector AdjustedTargetLocation = TargetLocation;
	// Nur anpassen, wenn wir wirklich ein Homing-Projektil haben oder 
	// wenn wir explizit auf eine Entität schießen ohne Positionsvorgabe.
	if (ResolvedTargetEntity.IsValid() && bFollowTarget)
	{
		if (const FMassAgentCharacteristicsFragment* TargetChar = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(ResolvedTargetEntity))
		{
			AdjustedTargetLocation.Z = (TargetChar->bIsFlying ? TargetChar->FlyHeight : TargetChar->CapsuleHeight) + TargetChar->LastGroundLocation;
		}
	}

	FMassProjectileFragment ProjectileFragment;
	ProjectileFragment.ProjectileClass = ProjectileClass;
	ProjectileFragment.TargetLocation = AdjustedTargetLocation;
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
	ProjectileFragment.ProjectileEffect = (ProjectileEffect != nullptr) ? ProjectileEffect : CDO->ProjectileEffect;
	ProjectileFragment.ProjectileEffect2 = (ProjectileEffect2 != nullptr) ? ProjectileEffect2 : CDO->ProjectileEffect2;
	ProjectileFragment.ProjectileEffect3 = (ProjectileEffect3 != nullptr) ? ProjectileEffect3 : CDO->ProjectileEffect3;
	ProjectileFragment.bIsPredicted = bIsPredicted;
	ProjectileFragment.AreaInfo = AreaInfo;

	if (FMassAllianceFragment* AllianceFrag = EntityManager->GetFragmentDataPtr<FMassAllianceFragment>(Entity))
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UPlayerTeamSubsystem* TeamSubsystem = GI->GetSubsystem<UPlayerTeamSubsystem>())
			{
				AllianceFrag->AlliedTeamsMask = TeamSubsystem->GetAlliedTeamsMask(ProjectileFragment.TeamId);
			}
		}
	}

	ProjectileFragment.bIsHoming = bFollowTarget && CDO->HomingMissleCount > 0;
	ProjectileFragment.HomingInitialAngle = HomingInitialAngle;
	ProjectileFragment.HomingRotationSpeed = HomingRotationSpeed;
	ProjectileFragment.HomingMaxSpiralRadius = HomingMaxSpiralRadius;
	ProjectileFragment.HomingInterpSpeed = HomingInterpSpeed;

    ProjectileFragment.ProjectileScale = Scale;
    ProjectileFragment.bContinueAfterTarget = CDO->bContinueAfterTarget;
    ProjectileFragment.FlightDirection = (AdjustedTargetLocation - Transform.GetLocation()).GetSafeNormal();
    if (ProjectileFragment.FlightDirection.IsNearlyZero())
    {
        ProjectileFragment.FlightDirection = Transform.GetRotation().GetForwardVector();
    }
    ProjectileFragment.TotalDistance = (AdjustedTargetLocation - Transform.GetLocation()).Size();


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
		FVector TraceEnd = TraceStart + ProjectileFragment.FlightDirection * (ProjectileFragment.Speed * 10.f * ProjectileFragment.MaxLifeTime * 1.5f);

		bool bIsArc = ProjectileFragment.ArcHeight > 0.f || ProjectileFragment.ArcHeightDistanceFactor > 0.f;
		if (bIsArc)
		{
			// Effektive Bogenhöhe berechnen
			float EffectiveArcHeight = ProjectileFragment.ArcHeight + (ProjectileFragment.TotalDistance * ProjectileFragment.ArcHeightDistanceFactor);

			// Mittelpunkt zwischen Start und Ziel finden
			FVector MidPoint = Transform.GetLocation() + (ProjectileFragment.FlightDirection * (ProjectileFragment.TotalDistance * 0.5f));

			// Trace am Scheitelpunkt starten und zum Ziel führen
			TraceStart = MidPoint + FVector(0, 0, EffectiveArcHeight);
			TraceEnd = AdjustedTargetLocation;
		}
		
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

		// (2) Robust fallback: vertical down-trace at the target location to find the actual ground.
		//     Reliable in cooked/packaged builds where a horizontal forward trace can miss the terrain,
		//     or where the ground is a static-mesh floor rather than an ALandscape (the forward trace
		//     above only accepts ALandscape/ALandscapeProxy hits, so it silently fails on such ground).
		if (!ProjectileFragment.bHasLandscapeImpact)
		{
			const FVector Base      = AdjustedTargetLocation;
			const FVector DownStart = Base + FVector(0.f, 0.f, 5000.f);
			const FVector DownEnd   = Base - FVector(0.f, 0.f, 100000.f);
			FHitResult GroundResult;
			if (GetWorld()->LineTraceSingleByChannel(GroundResult, DownStart, DownEnd, ECC_WorldStatic, Params))
			{
				ProjectileFragment.bHasLandscapeImpact   = true;
				ProjectileFragment.LandscapeImpactLocation = GroundResult.ImpactPoint;
			}
		}

		// (3) Last-resort fallback for ALL projectiles (previously bIsArc-only):
		//     guarantees GroundHit fires when bEnableLandscapeHit is set, even if every trace missed.
		if (!ProjectileFragment.bHasLandscapeImpact)
		{
			ProjectileFragment.bHasLandscapeImpact   = true;
			ProjectileFragment.LandscapeImpactLocation = AdjustedTargetLocation;
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

	// ResolvedTargetEntity already resolved above
	ProjectileFragment.TargetEntity = ResolvedTargetEntity;

	// Compute initial local visibility BEFORE adding the ISM instance
	int32 LocalTeamId = 0;
	int64 LocalAllianceMask = 0;
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC))
			{
				LocalTeamId = CustomPC->SelectableTeamId;
				LocalAllianceMask = CustomPC->AlliedTeamsMask;

				// Safety repair like UnitVisibilityProcessor does
				if (LocalTeamId != 0 && (LocalAllianceMask & (1LL << LocalTeamId)) == 0)
				{
					if (UGameInstance* GI = World->GetGameInstance())
					{
						if (UPlayerTeamSubsystem* TeamSubsystem = GI->GetSubsystem<UPlayerTeamSubsystem>())
						{
							LocalAllianceMask = TeamSubsystem->GetAlliedTeamsMask(LocalTeamId);
						}
					}
					LocalAllianceMask |= (1LL << LocalTeamId);
				}
			}
		}
	}

	const bool bLocalHasAlliance = (LocalAllianceMask & (1LL << ProjectileFragment.TeamId)) != 0;
	const bool bIsMyTeamInit = bLocalHasAlliance || LocalTeamId == 0; // spectator/neutral sees all
	const bool bInitiallyHidden = CDO->bAffectedByFogOfWar && !bIsMyTeamInit;

	FMassProjectileVisualFragment VisualFragment;
	VisualFragment.ProjectileClass = ProjectileClass;
	VisualFragment.VisualRelativeTransform = CDO->ISMComponent ? CDO->ISMComponent->GetRelativeTransform() : FTransform::Identity;
    
    if (ISM)
    {
		// Initial Transform: Relative Offset * Entity Transform
		FTransform InitialTransform = VisualFragment.VisualRelativeTransform * ScaledTransform;
		if (bInitiallyHidden)
		{
			InitialTransform.SetScale3D(FVector::ZeroVector); // Hide immediately to avoid first-frame blink
		}
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
				NC->SetVisibility(!bInitiallyHidden); // Do NOT Deactivate; Movement toggles visibility later
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
				NC->SetVisibility(!bInitiallyHidden);
				VisualFragment.Niagara_B = NC;
			}
		}
	}

	// One-shot spawn burst. Unlike the Niagara_A/B trails above - persistent components whose
	// visibility the movement processor re-toggles every tick - this fires once and self-destroys,
	// so when fog hides it we skip it entirely rather than spawn-then-hide. There is no later frame
	// in which it could be un-hidden, and an invisible one-shot is pure cost.
	{
		// bIsVisibleEnemy is the one term the projectile cannot supply at t=0: its own value is
		// seeded false below and its sight fragment is left empty. Take it from the SHOOTER's
		// visibility FRAGMENT rather than the shooter actor's mirror fields - the fragment exists
		// for actor-less ISM-only units (the client often has no shooter actor at all) and it does
		// not vanish when the shooter dies the same frame it fires. At t=0 the projectile is still
		// at the muzzle, so "can the local player see the shooter" is "can they see this flash".
		// IsEntityActive, not IsEntityValid: GetFragmentDataPtr check()s the archetype, so a
		// valid-but-reserved handle would assert.
		bool bShooterVisibleEnemy = false;
		if (EntityManager->IsEntityActive(ResolvedShooterEntity))
		{
			if (const FMassVisibilityFragment* ShooterVis = EntityManager->GetFragmentDataPtr<FMassVisibilityFragment>(ResolvedShooterEntity))
			{
				bShooterVisibleEnemy = ShooterVis->bIsVisibleEnemy;
			}
		}

		// The fog switch is the projectile's own CDO flag, never the shooter's fragment flag -
		// MassActorBindingComponent hardcodes that one to true for every unit, so reading it would
		// make bAffectedByFogOfWar dead. bIsMyTeamInit is reused in place from above.
		// bIsOnViewport is deliberately left out: off-screen is not fogged, so a burst for an
		// allied off-screen shot leaks nothing, and Niagara culling plus attenuation absorb it.
		// With no resolvable shooter this degrades to the gate the ISM and trails already use.
		const bool bSpawnFXVisible = !CDO->bAffectedByFogOfWar || bIsMyTeamInit || bShooterVisibleEnemy;
		FireSpawnEffects(CDO, ScaledTransform, bSpawnFXVisible);
	}

    // Apply fragments
    FMassVisibilityFragment VisibilityFragment;
    VisibilityFragment.VisibilityOffset = CDO->VisibilityOffset;
    VisibilityFragment.bAffectedByFogOfWar = CDO->bAffectedByFogOfWar;
	VisibilityFragment.bIsMyTeam = bIsMyTeamInit; // Use alliance-aware value
    VisibilityFragment.bIsOnViewport = true;      // Will be updated by processor
	VisibilityFragment.bIsVisibleEnemy = false;   // Will be updated by processor

    FMassCombatStatsFragment StatsFragment;
    StatsFragment.TeamId = ProjectileFragment.TeamId;
    StatsFragment.Health = 100.f; // Non-zero for sight system to consider it valid
    StatsFragment.SightRadius = 0.f;
    StatsFragment.LoseSightRadius = 0.f;

    FMassSightFragment SightFragment;
    // Sight settings from CDO if needed, but projectiles usually don't "see", they are seen.
    
    FMassAgentCharacteristicsFragment CharFragment;
    // Basic characteristics
    CharFragment.CapsuleRadius = 10.f; // Small radius for projectiles

    EntityManager->GetFragmentDataChecked<FTransformFragment>(Entity) = TransformFragment;
    EntityManager->GetFragmentDataChecked<FMassProjectileFragment>(Entity) = ProjectileFragment;
    EntityManager->GetFragmentDataChecked<FMassProjectileVisualFragment>(Entity) = VisualFragment;
    EntityManager->GetFragmentDataChecked<FMassVisibilityFragment>(Entity) = VisibilityFragment;
    EntityManager->GetFragmentDataChecked<FMassCombatStatsFragment>(Entity) = StatsFragment;
    EntityManager->GetFragmentDataChecked<FMassSightFragment>(Entity) = SightFragment;
    EntityManager->GetFragmentDataChecked<FMassAgentCharacteristicsFragment>(Entity) = CharFragment;
    
	return Entity;
}
