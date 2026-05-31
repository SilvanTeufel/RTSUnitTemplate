// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EffectArea.h"
#include "System/PlayerTeamSubsystem.h"
#include "Engine/GameInstance.h"
#include "Actors/AreaDecalComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassCommonFragments.h"
#include "Actors/Projectile.h"
#include "Mass/MassActorBindingComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "System/RTSBeaconSubsystem.h"
#include "Mass/Abilitys/EffectAreaVisualManager.h"

// Sets default values
AEffectArea::AEffectArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ISMTemplate = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMTemplate"));
	ISMTemplate->SetupAttachment(SceneRoot);
	// Template is only for Blueprint setup; no collision/overlap
	ISMTemplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMTemplate->SetGenerateOverlapEvents(false);
	ISMTemplate->SetHiddenInGame(true);

	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(SceneRoot);
	Niagara_A->SetAutoActivate(true);
	Niagara_A->SetVisibility(false); // Hide initially to avoid flickering before processor update
	
	MassBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassBindingComponent"));
	MassBindingComponent->HideActorTime = 0.0f;
	MassBindingComponent->DespawnTime = 0.5f;

	bUseEffectAreaImpactProcessor = false;
	StartRadius = 100.f;
	EndRadius = 500.f;
	TimeToEndRadius = 5.f;
	ScaleMesh = false;
	bIsRadiusScaling = true;
	BaseRadius = 100.f;
	BaseDamage = 0.f;
	bPulsate = false;
	bDestroyOnImpact = false;
	bScaleOnImpact = false;
	bIsScalingAfterImpact = false;
	bImpactScaleTriggered = false;
	RadiusAtImpactStart = 0.f;
	DuplicationRadius = 0.f;
	DuplicationTime = 0.f;
	RandomAngleRange = 360.f;
	MaxDuplicationCount = 0;
	DuplicationId = 0;
	EarlySpawnTime = 1.0f;
	SpawnCountOnDestruction = 1;
	bSpawnDoGroundTrace = true;
	SpawnVerticalOffset = 0.f;
	SpawnRandomOffsetMin = 0.f;
	SpawnRandomOffsetMax = 0.f;
	Health = 0.f;
	StartScaleTime = 0.f;
	CapsuleHeight = 50.f;
	bLocalDeathEffectsExecuted = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	SetMinNetUpdateFrequency(1.f);
}

// Called when the game starts or when spawned
void AEffectArea::BeginPlay()
{
	Super::BeginPlay();
	SetReplicateMovement(true); // Re-enabled for smooth client-side transform synchronization

	if (HasAuthority() && AreaIndex == -1)
	{
		if (ARTSGameModeBase* GM = GetWorld()->GetAuthGameMode<ARTSGameModeBase>())
		{
			AreaIndex = ++GM->HighestUnitIndex;
		}

		if (UPlayerTeamSubsystem* TeamSubsystem = GetGameInstance()->GetSubsystem<UPlayerTeamSubsystem>())
		{
			AlliedTeamsMask = TeamSubsystem->GetAlliedTeamsMask(TeamId);
		}
	}

	if (MassBindingComponent)
	{
		MassBindingComponent->SetupMassOnEffectArea();
	}
}

void AEffectArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (UWorld* World = GetWorld())
    {
        if (UEffectAreaVisualManager* VisualManager = World->GetSubsystem<UEffectAreaVisualManager>())
        {
            if (MassBindingComponent)
            {
                VisualManager->RemoveVisualInstance(MassBindingComponent->GetEntityHandle());
            }
        }
    }
}

// Called every frame
void AEffectArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bUseEffectAreaImpactProcessor)
	{
		LifeTime += DeltaTime;
		if (MaxLifeTime > 0.f && LifeTime >= MaxLifeTime) {
			Destroy(true, false);
		}
	}
}

void AEffectArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Adds an instance for preview in the editor if a mesh is selected
	if (ISMTemplate)
	{
		if (ISMTemplate->GetInstanceCount() == 0 && ISMTemplate->GetStaticMesh())
		{
			ISMTemplate->AddInstance(FTransform::Identity);
		}
	}
}

void AEffectArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AEffectArea, AreaEffectOne);
	//DOREPLIFETIME(AEffectArea, AreaEffectTwo);
	DOREPLIFETIME(AEffectArea, TeamId);
	DOREPLIFETIME(AEffectArea, Niagara_A);
	DOREPLIFETIME(AEffectArea, BeaconRange);
	DOREPLIFETIME(AEffectArea, VisualRotationOffset);
	DOREPLIFETIME(AEffectArea, AreaIndex);
	DOREPLIFETIME(AEffectArea, StartRadius);
	DOREPLIFETIME(AEffectArea, EndRadius);
	DOREPLIFETIME(AEffectArea, BaseRadius);
	DOREPLIFETIME(AEffectArea, BaseDamage);

	DOREPLIFETIME(AEffectArea, AlliedTeamsMask);
	DOREPLIFETIME(AEffectArea, DuplicationId);
	DOREPLIFETIME(AEffectArea, MaxDuplicationCount);
	DOREPLIFETIME(AEffectArea, bUseEffectAreaImpactProcessor);
	DOREPLIFETIME(AEffectArea, TimeToEndRadius);
	DOREPLIFETIME(AEffectArea, bIsRadiusScaling);
	DOREPLIFETIME(AEffectArea, bPulsate);
	DOREPLIFETIME(AEffectArea, StartScaleTime);
	DOREPLIFETIME(AEffectArea, MaxLifeTime);
	DOREPLIFETIME(AEffectArea, bDestroyOnImpact);
	DOREPLIFETIME(AEffectArea, bScaleOnImpact);
	DOREPLIFETIME(AEffectArea, bIsScalingAfterImpact);
	DOREPLIFETIME(AEffectArea, bImpactScaleTriggered);
	DOREPLIFETIME(AEffectArea, RadiusAtImpactStart);
	DOREPLIFETIME(AEffectArea, ScaleMesh);
}

FQuat AEffectArea::CalculateGroundRotationOffset(const FVector& Normal, const FVector& Forward, float RandomZRotation)
{
	FVector SafeForward = Forward.GetSafeNormal();
	if (SafeForward.IsNearlyZero()) SafeForward = FVector::ForwardVector;

	FVector Right = FVector::CrossProduct(Normal, SafeForward).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		// Wenn Normal und Forward parallel sind, nehmen wir ein alternatives Right
		FVector Up = FMath::Abs(Normal.Z) < 0.999f ? FVector::UpVector : FVector::ForwardVector;
		Right = FVector::CrossProduct(Normal, Up).GetSafeNormal();
	}

	FVector AdjustedForward = FVector::CrossProduct(Right, Normal).GetSafeNormal();
	FQuat TargetWorldRotation = FRotationMatrix::MakeFromXY(AdjustedForward, Right).ToQuat();
    
	if (RandomZRotation != 0.f)
	{
		FQuat RandomRot = FQuat(Normal, FMath::DegreesToRadians(RandomZRotation));
		TargetWorldRotation = RandomRot * TargetWorldRotation;
	}

	// Aktuelle Actor-Rotation: Nur Yaw basierend auf Forward (auf XY-Ebene)
	FVector YawForward = SafeForward;
	YawForward.Z = 0.f;
	if (YawForward.IsNearlyZero())
	{
		YawForward = FVector::ForwardVector;
	}
	else
	{
		YawForward.Normalize();
	}

	FQuat CurrentActorRotation = FRotationMatrix::MakeFromX(YawForward).ToQuat();
    
	// Der Offset ist das, was man auf den Actor anwenden muss, um TargetWorldRotation zu erhalten
	return TargetWorldRotation * CurrentActorRotation.Inverse();
}

void AEffectArea::SetBeaconRange(float NewRange)
{
	BeaconRange = FMath::Max(0.f, NewRange);
}

bool AEffectArea::IsInBeaconRange() const
{
	UWorld* World = GetWorld();
	return World ? AEffectArea::IsLocationInBeaconRange(World, GetActorLocation()) : false;
}

bool AEffectArea::IsLocationInBeaconRange(UWorld* World, const FVector& Location)
{
	if (URTSBeaconSubsystem* BeaconSubsystem = World ? World->GetSubsystem<URTSBeaconSubsystem>() : nullptr)
	{
		return BeaconSubsystem->IsLocationInBeaconRange(Location);
	}
	return false;
}

void AEffectArea::SetActorVisibility(bool bVisible)
{
	if (Niagara_A) Niagara_A->SetVisibility(bVisible);
}

void AEffectArea::SetEnemyVisibility(AActor* DetectingActor, bool bVisible)
{
	if (bUseEffectAreaImpactProcessor) return;

	if (DetectingActor == nullptr || !DetectingActor->IsValidLowLevelFast())
	{
		return;
	}

	int32 DetectorTeamId = -1;
	if (AUnitBase* Unit = Cast<AUnitBase>(DetectingActor))
	{
		DetectorTeamId = Unit->TeamId;
	}

	if (DetectorTeamId == -1) return;
	
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	ACustomControllerBase* MyController = Cast<ACustomControllerBase>(PC);

	if (MyController && MyController->IsValidLowLevelFast())
	{
		if (MyController->AlliedTeamsMask & (1LL << DetectorTeamId))
		{
			bIsVisibleByFog = bVisible;
		}
	}
}

bool AEffectArea::ComputeInherentVisibility() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	if (!CustomPC) return true;

	int32 LocalTeamId = CustomPC->SelectableTeamId;
	bool bIsMyTeam = (TeamId == LocalTeamId || LocalTeamId == 0);

	bool VisibleByTeam = true;
	if (bInvisibleToEnemies && !bIsMyTeam && TeamId != 0)
	{
		VisibleByTeam = false;
	}

	bool VisibleByFog = true;
	if (bAffectedByFogOfWar && !bIsMyTeam && TeamId != 0)
	{
		VisibleByFog = bIsVisibleByFog;
	}

	// Invisibility status (relevant for enemies)
	if (bIsInvisible && !bIsMyTeam)
	{
		return false;
	}

	const bool bResult = VisibleByTeam && VisibleByFog && bIsOnViewport;

	return bResult;
}

bool AEffectArea::ComputeLocalVisibility() const
{
	return ComputeInherentVisibility();
}

void AEffectArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bUseEffectAreaImpactProcessor) return;

	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffectOne);
			UnitToHit->ApplyInvestmentEffect(AreaEffectTwo);
			UnitToHit->ApplyInvestmentEffect(AreaEffectThree);
			ImpactEvent(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffectOne);
			UnitToHit->ApplyInvestmentEffect(AreaEffectTwo);
			UnitToHit->ApplyInvestmentEffect(AreaEffectThree);
			ImpactEvent(UnitToHit);
		}
	}
}

void AEffectArea::HandleProjectileImpact_Implementation(AActor* Shooter, const FVector& ImpactLocation, TSubclassOf<class AProjectile> ProjectileClass, float DamageOverride, TSubclassOf<class UGameplayEffect> ProjectileEffect, TSubclassOf<class UGameplayEffect> ProjectileEffect2, TSubclassOf<class UGameplayEffect> ProjectileEffect3)
{
	float AppliedDamage = DamageOverride;
	if (AppliedDamage < 0.f && ProjectileClass) {
		if (AProjectile* ProjCDO = ProjectileClass->GetDefaultObject<AProjectile>())
			AppliedDamage = ProjCDO->Damage;
	}

	FMassEntityHandle Entity = MassBindingComponent ? MassBindingComponent->GetEntityHandle() : FMassEntityHandle();
	if (Entity.IsValid()) {
		FMassEntityManager& EM = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());
		if (FMassCombatStatsFragment* Stats = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity)) {
			Stats->Health -= AppliedDamage;
			if (HasAuthority() && Stats->Health <= 0.f)
			{
				EM.Defer().AddTag<FMassStateDeadTag>(Entity);
				if (FMassAIStateFragment* StateFrag = EM.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
				{
					StateFrag->StateTimer = 0.f;
				}
			}
		}
	}

	// Projectile effects via Shooter (like units)
	if (ProjectileClass && Shooter) {
		if (const AProjectile* CDO = ProjectileClass->GetDefaultObject<AProjectile>()) {
			if (APerformanceUnit* PerfShooter = Cast<APerformanceUnit>(Shooter)) {
				const FVector FromLoc = PerfShooter->GetActorLocation();
				const FRotator FaceRot = (ImpactLocation - FromLoc).Rotation();
				PerfShooter->FireEffectsAtLocation(CDO->ImpactVFX, CDO->ImpactSound, CDO->ScaleImpactVFX, CDO->ScaleImpactSound, ImpactLocation, 2.0f, FaceRot);
			}
		}
	}
}

void AEffectArea::SetDeathVisualState(bool bShouldHide)
{
	if (bShouldHide)
	{
		if (Niagara_A) Niagara_A->SetVisibility(false);
		if (ISMTemplate) ISMTemplate->SetHiddenInGame(true);

		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			if (Comp && Comp != GetRootComponent() && !Comp->IsA<UAreaDecalComponent>())
			{
				// Keep the RVT writer of the decal
				if (Comp->GetName().Contains(TEXT("RVTWriterMesh")))
				{
					continue;
				}
				Comp->SetHiddenInGame(true);
			}
		}
	}
	else
	{
		if (Niagara_A) Niagara_A->SetVisibility(true);
		if (ISMTemplate) ISMTemplate->SetHiddenInGame(false);

		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			if (Comp)
			{
				Comp->SetHiddenInGame(false);
			}
		}
	}
}

void AEffectArea::HandleDeath(bool bIsVisible)
{
	if (bLocalDeathEffectsExecuted) return;
	bLocalDeathEffectsExecuted = true;

	if (Niagara_A)
	{
		Niagara_A->Deactivate();
		Niagara_A->SetVisibility(false);
	}

	OnEffectAreaDead.Broadcast();

	if (bIsVisible && GetNetMode() != NM_DedicatedServer)
	{
		if (DeathVFX)
		{
			DeathNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DeathVFX, GetActorLocation());
		}

		if (DeathSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
		}
	}

	OnEffectAreaDestructionStarted();
	
	if (MassBindingComponent)
	{
		SetLifeSpan(MassBindingComponent->DespawnTime);
	}
	else
	{
		Destroy();
	}
}
