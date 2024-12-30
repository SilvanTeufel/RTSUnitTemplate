// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"

// Sets default values
AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);

	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
	}
}

void AProjectile::Init(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;

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

		if (ShootingUnit->IsVisibileEnemy || ShootingUnit->IsMyTeam)
		{

			SetVisibility(true);
		}
		else if (ShootingUnit->EnableFog)
		{

			SetVisibility(false);
		}else
		{
			SetVisibility(true);
		}
	}
}

void AProjectile::InitForAbility(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;

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

		if (ShootingUnit->IsVisibileEnemy || ShootingUnit->IsMyTeam)
		{

			SetVisibility(true);
		}
		else if (ShootingUnit->EnableFog)
		{

			SetVisibility(false);
		}else
		{
			SetVisibility(true);
		}
		
	}
}

void AProjectile::InitForLocationPosition(FVector Aim, AActor* ShootingActor)
{

	Shooter = ShootingActor;
	TargetLocation = Aim;

	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	UE_LOG(LogTemp, Warning, TEXT("Shooter location is: %s"), *ShooterLocation.ToString());
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		//Damage = ShootingUnit->Attributes->GetAttackDamage();
		UseAttributeDamage = false;
		TeamId = ShootingUnit->TeamId;

		if (ShootingUnit->IsVisibileEnemy || ShootingUnit->IsMyTeam)
		{

			SetVisibility(true);
		}
		else if (ShootingUnit->EnableFog)
		{

			SetVisibility(false);
		}else
		{
			SetVisibility(true);
		}
		
	}
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

void AProjectile::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, Target);
	DOREPLIFETIME(AProjectile, Shooter);
	DOREPLIFETIME(AProjectile, Mesh);
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
		if (Mesh) // Assuming your mesh component is named MeshComponent
		{
			Mesh->AddLocalRotation(NewRotation);
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
void AProjectile::Impact_Implementation(AActor* ImpactTarget)
{
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

void AProjectile::ImpactHeal_Implementation(AActor* ImpactTarget)
{
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
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			ImpactEvent();
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && !IsHealing)
		{
			ImpactEvent();
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			ImpactEvent();
			Impact(UnitToHit);
			SetIsAttacked(UnitToHit);
			DestroyWhenMaxPierced();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
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

