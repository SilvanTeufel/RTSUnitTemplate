// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/UnitBase.h"

// Sets default values
AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);
	//Mesh->SetupAttachment(RootComponent);
}

void AProjectile::Init(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;

	AUnitBase* ShootingUnit = Cast<AUnitBase>(ShootingActor);
	if(ShootingUnit)
	{
		Damage = ShootingUnit->AttackDamage;
		TeamId = ShootingUnit->TeamId;
		MovementSpeed = ShootingUnit->ProjectileSpeed;
	}
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LifeTime += DeltaTime;

	if(LifeTime > MaxLifeTime)
	{
		Destroy(true, false);
	}else if(Target)
	{
		AUnitBase* TargetToAttack = Cast<AUnitBase>(Target);
		
		if(TargetToAttack && TargetToAttack->GetUnitState() != UnitData::Dead)
		{
			const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), TargetToAttack->GetActorLocation());
			AddActorLocalOffset(Direction*MovementSpeed);
		}else
		{
			Destroy(true, false);
		}
	}
	
}

void AProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			Destroy(true, false);
		}else if(UnitToHit && UnitToHit->TeamId != TeamId)
		{
			if(UnitToHit->GetShield() <= 0)
			UnitToHit->SetHealth(UnitToHit->GetHealth()-Damage);
			else
			UnitToHit->SetShield(UnitToHit->GetShield()-Damage);
				
			if(UnitToHit->GetUnitState() != UnitData::Run)
			UnitToHit->SetUnitState( UnitData::IsAttacked );
			
			Destroy(true, false);
		}
			
	}
}

