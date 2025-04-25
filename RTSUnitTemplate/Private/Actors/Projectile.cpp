// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"

// Sets default values
AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 

	// Create a new scene component as the root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot); // Set it as the root component

	// Create all components and attach them to the SceneRoot
	Mesh_A = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh_A->SetupAttachment(SceneRoot); // Attach to the new root
	
	Mesh_B = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh_B"));
	Mesh_B->SetupAttachment(SceneRoot);
	
	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(SceneRoot);
	
	Niagara_B = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_B"));
	Niagara_B->SetupAttachment(SceneRoot);
	
	// Collision settings for Mesh
	Mesh_A->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh_A->SetCollisionProfileName(TEXT("Trigger"));
	Mesh_A->SetGenerateOverlapEvents(true);

	// Optionally, initialize Niagara properties here
	SceneRoot->SetVisibility(false, true);

	bReplicates = true;
	/*
	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicates(true);
		SetReplicateMovement(true);
	}*/
}

void AProjectile::Init(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;
	SetOwner(Shooter);
	
	if(TargetLocation.IsZero())
		TargetLocation = TargetActor->GetActorLocation();

	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		Damage = ShootingUnit->Attributes->GetAttackDamage();
		TeamId = ShootingUnit->TeamId;
		MovementSpeed = ShootingUnit->Attributes->GetProjectileSpeed();
	}
}

void AProjectile::InitForAbility(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;
	SetOwner(Shooter);
	
	if(TargetLocation.IsZero())
		TargetLocation = TargetActor->GetActorLocation();

	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		//Damage = ShootingUnit->Attributes->GetAttackDamage();
		UseAttributeDamage = false;
		TeamId = ShootingUnit->TeamId;
	}
}

void AProjectile::InitForLocationPosition(FVector Aim, AActor* ShootingActor)
{

	Shooter = ShootingActor;
	TargetLocation = Aim;
	SetOwner(Shooter);
	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		//Damage = ShootingUnit->Attributes->GetAttackDamage();
		UseAttributeDamage = false;
		TeamId = ShootingUnit->TeamId;
	}
}


void AProjectile::SetProjectileVisibility_Implementation()
{

	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* TargetUnit = Cast<AUnitBase>(Target);

	bool bShootingVisible = ShootingUnit ? ((ShootingUnit->IsVisibleEnemy || ShootingUnit->IsMyTeam) ? true : (!ShootingUnit->EnableFog)) : false;
	bool bTargetVisible   = TargetUnit ? ((TargetUnit->IsVisibleEnemy  || TargetUnit->IsMyTeam)  ? true : (!TargetUnit->EnableFog))   : false;
	bool bFinalVisibility = bShootingVisible || bTargetVisible;
	
	SceneRoot->SetVisibility(bFinalVisibility, true);
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetReplicateMovement(true);
}

void AProjectile::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, Target);
	DOREPLIFETIME(AProjectile, Shooter);

	DOREPLIFETIME(AProjectile, ImpactSound);
	DOREPLIFETIME(AProjectile, ImpactVFX);
	
	DOREPLIFETIME(AProjectile, Material);
	DOREPLIFETIME(AProjectile, Damage);
	DOREPLIFETIME(AProjectile, LifeTime);
	DOREPLIFETIME(AProjectile, MaxLifeTime);
	DOREPLIFETIME(AProjectile, TeamId);
	DOREPLIFETIME(AProjectile, MovementSpeed);
	DOREPLIFETIME(AProjectile, DestructionDelayTime);
	DOREPLIFETIME(AProjectile, RotateMesh);
	DOREPLIFETIME(AProjectile, RotationSpeed);
	DOREPLIFETIME(AProjectile, TargetLocation);
	DOREPLIFETIME(AProjectile, ShooterLocation);
	DOREPLIFETIME(AProjectile, FollowTarget);
	DOREPLIFETIME(AProjectile, MaxPiercedTargets);
	DOREPLIFETIME(AProjectile, PiercedTargets);
	DOREPLIFETIME(AProjectile, IsHealing);
	DOREPLIFETIME(AProjectile, IsBouncingBack);
	DOREPLIFETIME(AProjectile, IsBouncingNext);
	DOREPLIFETIME(AProjectile, BouncedBack);
	DOREPLIFETIME(AProjectile, ProjectileEffect); // Added for Build
	DOREPLIFETIME(AProjectile, UseAttributeDamage);

	DOREPLIFETIME(AProjectile, ScaleImpactVFX);
	DOREPLIFETIME(AProjectile, ScaleImpactSound);

	DOREPLIFETIME(AProjectile, Niagara_A);
	DOREPLIFETIME(AProjectile, Niagara_B);
}

// Called every frame
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckViewport();
	LifeTime += DeltaTime;
	
	if(RotateMesh)
	{
		// Calculate rotation amount based on DeltaTime and RotationSpeed
	
		FRotator NewRotation = FRotator(RotationSpeed.X * DeltaTime, RotationSpeed.Y * DeltaTime, RotationSpeed.Z * DeltaTime);

		// Apply rotation to the mesh
		if (Mesh_A) // Assuming your mesh component is named MeshComponent
		{
			Mesh_A->AddLocalRotation(NewRotation);
		}
	}
	if(LifeTime > MaxLifeTime)
	{
		Destroy(true, false);
	}else if(LifeTime > MaxLifeTime && FollowTarget)
	{
		Impact(Target);
		Destroy(true, false);
	}else if(Target)
	{
		FlyToUnitTarget();
	}else
	{
		FlyToLocationTarget();
	}
	
	
}

void AProjectile::CheckViewport()
{
	if (IsInViewport(GetActorLocation(), VisibilityOffset))
	{
		IsOnViewport = true;
	}
	else
	{
		IsOnViewport = false;
	}
}

bool AProjectile::IsInViewport(FVector WorldPosition, float Offset)
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldPosition, ScreenPosition);

		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		return ScreenPosition.X >= -Offset && ScreenPosition.X <= ViewportSizeX + Offset && ScreenPosition.Y >= -Offset && ScreenPosition.Y <= ViewportSizeY + Offset;
	}

	return false;
}

void AProjectile::FlyToUnitTarget()
{
	AUnitBase* TargetToAttack = Cast<AUnitBase>(Target);
		
	if(TargetToAttack && TargetToAttack->GetUnitState() != UnitData::Dead && FollowTarget) 
	{
		if(FollowTarget)
		{
			const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), TargetToAttack->GetActorLocation());
			AddActorWorldOffset(Direction * MovementSpeed);
		}
	}else if(FollowTarget)
	{
		Destroy(true, false);
	}else
	{
		const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(ShooterLocation, TargetLocation);
		AddActorWorldOffset(Direction * MovementSpeed);
	}

	// Calculate the distance between the projectile and the target
	float DistanceToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if(DistanceToTarget <= MovementSpeed && FollowTarget)
	{
		Impact(Target);
		Destroy(true, false);
	}
}

void AProjectile::FlyToLocationTarget()
{
	const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(ShooterLocation, TargetLocation);
	AddActorWorldOffset(Direction * MovementSpeed);
}

void AProjectile::Impact(AActor* ImpactTarget)
{
	if (!HasAuthority()) return;
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* UnitToHit = Cast<AUnitBase>(ImpactTarget);

	if(UnitToHit && UnitToHit->TeamId != TeamId && ShootingUnit)
	{
		float NewDamage = Damage;
		if (UseAttributeDamage)
		{
			NewDamage = ShootingUnit->Attributes->GetAttackDamage() - UnitToHit->Attributes->GetArmor();
			
			if(ShootingUnit->IsDoingMagicDamage)
				NewDamage = ShootingUnit->Attributes->GetAttackDamage() - UnitToHit->Attributes->GetMagicResistance();
		}else
		{
			NewDamage = Damage - UnitToHit->Attributes->GetArmor();
			
			if(ShootingUnit->IsDoingMagicDamage)
				NewDamage = Damage - UnitToHit->Attributes->GetMagicResistance();
		}
		
		if(UnitToHit->Attributes->GetShield() <= 0)
			UnitToHit->SetHealth_Implementation(UnitToHit->Attributes->GetHealth()-NewDamage);
		else
			UnitToHit->SetShield_Implementation(UnitToHit->Attributes->GetShield()-NewDamage);
			//UnitToHit->Attributes->SetAttributeShield(UnitToHit->Attributes->GetShield()-NewDamage);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId)
		{
			UnitToHit->ApplyInvestmentEffect(ProjectileEffect);
		}
		
		ShootingUnit->IncreaseExperience();
		UnitToHit->ActivateAbilityByInputID(UnitToHit->DefensiveAbilityID, UnitToHit->DefensiveAbilities);

		SetNextBouncing(ShootingUnit, UnitToHit);
		SetBackBouncing(ShootingUnit);
	}			
}


void AProjectile::ImpactHeal(AActor* ImpactTarget)
{
	if (!HasAuthority()) return;
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* UnitToHit = Cast<AUnitBase>(ImpactTarget);
	//UE_LOG(LogTemp, Warning, TEXT("Projectile ShootingUnit->Attributes->GetAttackDamage()! %f"), ShootingUnit->Attributes->GetAttackDamage());
	if(UnitToHit && UnitToHit->TeamId == TeamId && ShootingUnit)
	{
		float NewDamage = Damage;
		
		if (UseAttributeDamage)
			NewDamage = ShootingUnit->Attributes->GetAttackDamage();
		
		if(UnitToHit->Attributes->GetShield() <= 0)
			UnitToHit->SetHealth_Implementation(UnitToHit->Attributes->GetHealth()+NewDamage);
		else
			UnitToHit->SetShield_Implementation(UnitToHit->Attributes->GetShield()+NewDamage);
			//UnitToHit->Attributes->SetAttributeShield(UnitToHit->Attributes->GetShield()+NewDamage);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId == TeamId)
		{
			UnitToHit->ApplyInvestmentEffect(ProjectileEffect);
		}
		
		ShootingUnit->IncreaseExperience();
		UnitToHit->ActivateAbilityByInputID(UnitToHit->DefensiveAbilityID, UnitToHit->DefensiveAbilities);
		SetNextBouncing(ShootingUnit, UnitToHit);
		SetBackBouncing(ShootingUnit);
	}			
}


void AProjectile::OnOverlapBegin_Implementation(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	// Prüfen, ob wir NICHT im Game Thread sind
	if ( !IsInGameThread() )
	{
		// Sicherere Erfassung mit Weak Pointers für UObjects
		TWeakObjectPtr<AProjectile> WeakThis(this);
		TWeakObjectPtr<UPrimitiveComponent> WeakOverlappedComp(OverlappedComp);
		TWeakObjectPtr<AActor> WeakOtherActor(OtherActor);
		TWeakObjectPtr<UPrimitiveComponent> WeakOtherComp(OtherComp);
		// HitResult und primitive Typen per Wert kopieren ist sicher
		FHitResult CapturedSweepResult = SweepResult;

		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, WeakOverlappedComp, WeakOtherActor, WeakOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult]() // Erfasste Variablen
		{
		   // Starke Pointer im Game Thread holen
		   AProjectile* StrongThis = WeakThis.Get();
		   UPrimitiveComponent* StrongOverlappedComp = WeakOverlappedComp.Get();
		   AActor* StrongOtherActor = WeakOtherActor.Get();
		   UPrimitiveComponent* StrongOtherComp = WeakOtherComp.Get();

		   // Prüfen, ob 'this' und ggf. andere kritische Objekte noch gültig sind
		   if (StrongThis /* && StrongOverlappedComp && StrongOtherActor */) // Prüfe nach Bedarf
		   {
			  // Funktion erneut aufrufen, jetzt garantiert im Game Thread
			  // Verwende die starken Pointer oder kopierten Werte
			  StrongThis->OnOverlapBegin(StrongOverlappedComp, StrongOtherActor, StrongOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult);
			  // Oder ggf. direkt die _Implementation aufrufen:
			  // StrongThis->OnOverlapBegin_Implementation(StrongOverlappedComp, StrongOtherActor, StrongOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult);
		   }
		});
		// Ursprüngliche Ausführung beenden
		return;
	}

	
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			ImpactEvent();
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && !IsHealing)
		{
			ImpactEvent();
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			ImpactEvent();
			Impact(UnitToHit);
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			SetIsAttacked(UnitToHit);
			DestroyWhenMaxPierced();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyWhenMaxPierced();
		}
			
	}
}

void AProjectile::DestroyWhenMaxPierced()
{
	PiercedTargets += 1;
	if(PiercedTargets >= MaxPiercedTargets)
	{
		DestroyProjectileWithDelay();
	}
}

void AProjectile::DestroyProjectileWithDelay()
{
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AProjectile::DestroyProjectile, DestructionDelayTime, false);
}

void AProjectile::DestroyProjectile()
{
	Destroy(true, false);
}


void AProjectile::SetIsAttacked(AUnitBase* UnitToHit)
{
	if(UnitToHit->GetUnitState() != UnitData::Run &&
		UnitToHit->GetUnitState() != UnitData::Attack &&
		UnitToHit->GetUnitState() != UnitData::Casting &&
		UnitToHit->GetUnitState() != UnitData::Rooted &&
		UnitToHit->GetUnitState() != UnitData::Pause)
	{
		UnitToHit->UnitControlTimer = 0.f;
		UnitToHit->SetUnitState( UnitData::IsAttacked );
	}else if(UnitToHit->GetUnitState() == UnitData::Casting)
	{
		UnitToHit->UnitControlTimer -= UnitToHit->ReduceCastTime;
	}else if(UnitToHit->GetUnitState() == UnitData::Rooted)
	{
		UnitToHit->UnitControlTimer -= UnitToHit->ReduceRootedTime;
	}
}

void AProjectile::SetBackBouncing(AUnitBase* ShootingUnit)
{
	if(IsBouncingBack && IsBouncingNext && PiercedTargets == (MaxPiercedTargets-1))
	{
		Target = ShootingUnit;
		TargetLocation = ShootingUnit->GetActorLocation();
		BouncedBack = true;
	}else if(IsBouncingBack && PiercedTargets < MaxPiercedTargets)
	{
		Target = ShootingUnit;
		TargetLocation = ShootingUnit->GetActorLocation();
		BouncedBack = true;
	}
}

void AProjectile::SetNextBouncing(AUnitBase* ShootingUnit, AUnitBase* UnitToHit)
{
	if(IsBouncingNext)
	{
		AUnitBase* NewTarget = GetNextUnitInRange(ShootingUnit, UnitToHit);

		if(!NewTarget)
		{
			DestroyProjectileWithDelay();
			return;
		}
		
		Target = NewTarget;
		TargetLocation = NewTarget->GetActorLocation();
	}
}

AUnitBase* AProjectile::GetNextUnitInRange(AUnitBase* ShootingUnit, AUnitBase* UnitToHit)
{
	float Range = 9999999.f;
	AUnitBase* RUnit = nullptr; 
	for (AUnitBase* Unit : ShootingUnit->UnitsToChase)
	{
		if (Unit && Unit != UnitToHit)
		{
			float Distance = FVector::Dist(Unit->GetActorLocation(), ShootingUnit->GetActorLocation());
			if (Distance <= Range)
			{
				Range = Distance;
				RUnit = Unit;
			}else
			{
				
			}
		}
	}
	
	return RUnit;
}

void AProjectile::SetVisibility(bool Visible)
{
	GetMesh()->SetVisibility(Visible);
}

