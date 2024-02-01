// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Selectable.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASelectable::ASelectable()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is Selectable Capsule"));
	TriggerCapsule->InitCapsuleSize(30.f, 30.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	TriggerCapsule->SetupAttachment(RootComponent);
	TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASelectable::OnOverlapBegin);
}

void ASelectable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASelectable, PickupEffect);
	DOREPLIFETIME(ASelectable, PickupEffectTwo);
	DOREPLIFETIME(ASelectable, TeamId);
	DOREPLIFETIME(ASelectable, FollowTarget);

}

// Called when the game starts or when spawned
void ASelectable::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASelectable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(FollowTarget && Target)
	{
		const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), Target->GetActorLocation());
		AddActorWorldOffset(Direction * MovementSpeed);
		float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if(Distance <= PickUpDistance)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Target);
			
			if(!UnitBase) return;
			if(TeamId == UnitBase->TeamId) return;
			
			ImpactEvent();
			switch (Type)
			{
		 	
			case SelectableData::Health:
				{
					UnitBase->SetHealth(UnitBase->Attributes->GetHealth() + Amount);
				}
				break;
			case SelectableData::Shield:
				{
					UnitBase->Attributes->SetAttributeShield(UnitBase->Attributes->GetShield() + Amount);
				}
				break;
			case SelectableData::Effect:
				{
					UnitBase->ApplyInvestmentEffect(PickupEffect);
					UnitBase->ApplyInvestmentEffect(PickupEffectTwo);
				}
				break;
			default:
				{
		 					
				}
				break;
			}

			if(Sound)
				UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);
		 	
			DestroySelectableWithDelay();
		}
	}

}

void ASelectable::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
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

void ASelectable::DestroySelectableWithDelay()
{
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASelectable::DestroySelectable, DestructionDelayTime, false);
}

void ASelectable::DestroySelectable()
{
	Destroy(true, false);
}
