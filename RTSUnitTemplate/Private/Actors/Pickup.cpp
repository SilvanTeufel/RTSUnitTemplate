// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Pickup.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Net/UnrealNetwork.h"

// Sets default values
APickup::APickup()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 
	
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is Pickup Capsule"));
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	
	SetRootComponent(TriggerCapsule);
	TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnOverlapBegin);

}

void APickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APickup, PickupEffect);
	DOREPLIFETIME(APickup, PickupEffectTwo);
	DOREPLIFETIME(APickup, TeamId);
	DOREPLIFETIME(APickup, FollowTarget);
	DOREPLIFETIME(APickup, LifeTime);
	DOREPLIFETIME(APickup, MaxLifeTime);
	DOREPLIFETIME(APickup, DestructionDelayTime);
	DOREPLIFETIME(APickup, PickUpDistance);
	DOREPLIFETIME(APickup, Target);
	DOREPLIFETIME(APickup, MovementSpeed);
	DOREPLIFETIME(APickup, PickupAbility);
}

// Called when the game starts or when spawned
void APickup::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTime += DeltaTime;

	if(LifeTime > MaxLifeTime && MaxLifeTime > 0.f)
	{
		Destroy(true, false);
	}
	
	if(FollowTarget && Target)
	{
		const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), Target->GetActorLocation());
		AddActorWorldOffset(Direction * MovementSpeed);
		float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if(Distance <= PickUpDistance && !Executed)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Target);
			
			if(!UnitBase) return;
			if(TeamId == UnitBase->TeamId) return;
			
			ImpactEvent();
			switch (Type)
			{
		 	
			case PickUpData::Health:
				{
					UnitBase->SetHealth_Implementation(UnitBase->Attributes->GetHealth() + Amount);
				}
				break;
			case PickUpData::Shield:
				{
					UnitBase->SetShield_Implementation(UnitBase->Attributes->GetShield() + Amount);
					//UnitBase->Attributes->SetAttributeShield(UnitBase->Attributes->GetShield() + Amount);
				}
				break;
			case PickUpData::Effect:
				{
					UnitBase->ApplyInvestmentEffect(PickupEffect);
					UnitBase->ApplyInvestmentEffect(PickupEffectTwo);
				}
				break;
			case PickUpData::Ability:
				{
					
					if (PickupAbility && !UnitBase->SelectableAbilities.Contains(PickupAbility))
					{
						UnitBase->SelectableAbilities.AddUnique(PickupAbility);
						UnitBase->GetSelectedAbilitiesArray(PickupAbility);
					}
					
				}
				break;
			default:
				{
		 					
				}
				break;
			}

			if(Sound && UnitBase->IsOnViewport)
				UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);
		 	
			DestroySelectableWithDelay();
			Executed = true;
		}
	}

}

void APickup::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor && !FollowTarget)
	{

		AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
		
		if(!UnitBase) return;
		if(TeamId == UnitBase->TeamId) return;

		Target = UnitBase;
		FollowTarget = true;
			
	}
}

void APickup::DestroySelectableWithDelay()
{
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &APickup::DestroySelectable, DestructionDelayTime, false);
}

void APickup::DestroySelectable()
{
	Destroy(true, false);
}
