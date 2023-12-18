// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/UnitControllerBase.h"
#include "Net/UnrealNetwork.h"

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
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		Damage = ShootingUnit->Attributes->GetAttackDamage();
		TeamId = ShootingUnit->TeamId;
		MovementSpeed = ShootingUnit->Attributes->GetProjectileSpeed();
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
	DOREPLIFETIME(AProjectile, Mesh);
	DOREPLIFETIME(AProjectile, Material);
	DOREPLIFETIME(AProjectile, TargetLocation);
	DOREPLIFETIME(AProjectile, Damage);
	DOREPLIFETIME(AProjectile, LifeTime);
	DOREPLIFETIME(AProjectile, MaxLifeTime);
	DOREPLIFETIME(AProjectile, TeamId);
	DOREPLIFETIME(AProjectile, MovementSpeed);
	DOREPLIFETIME(AProjectile, DestructionDelayTime);
	
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
			//AddActorLocalOffset(Direction*MovementSpeed);
			AddActorWorldOffset(Direction * MovementSpeed);
			// Calculate the desired rotation towards the target
			//FRotator TargetRotation = Direction.Rotation();

			// Apply the rotation offset
			//TargetRotation += RotationOffset;

			// Rotate the mesh to face the target
			//Mesh->SetRelativeRotation(TargetRotation);
		}else
		{
			Destroy(true, false);
		}
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
			// Call the impact event
			ImpactEvent();

			// Delay the destruction
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AProjectile::DestroyProjectile, DestructionDelayTime, false);
		}else if(UnitToHit && UnitToHit->TeamId != TeamId)
		{
			AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);

			float NewDamage = Damage - UnitToHit->Attributes->GetArmor();
			
			if(ShootingUnit->IsDoingMagicDamage)
				NewDamage = Damage - UnitToHit->Attributes->GetMagicResistance();
			
			if(UnitToHit->Attributes->GetShield() <= 0)
			UnitToHit->SetHealth(UnitToHit->Attributes->GetHealth()-NewDamage);
			else
			UnitToHit->Attributes->SetAttributeShield(UnitToHit->Attributes->GetShield()-NewDamage);
				
			if(UnitToHit->GetUnitState() != UnitData::Run)
			{
				UnitToHit->UnitControlTimer = 0.f;
				UnitToHit->SetUnitState( UnitData::IsAttacked );
			}

			ShootingUnit->Experience++;
			// Call the impact event
			ImpactEvent();

			// Delay the destruction
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AProjectile::DestroyProjectile, DestructionDelayTime, false);
		}
			
	}
}

void AProjectile::DestroyProjectile()
{
	Destroy(true, false);
}