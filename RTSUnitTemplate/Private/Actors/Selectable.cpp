// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Selectable.h"
#include "Characters/TalentedUnit.h"
#include "Components/CapsuleComponent.h"
#include "Characters/UnitBase.h"

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

// Called when the game starts or when spawned
void ASelectable::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASelectable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASelectable::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{

		AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
		
		 if(UnitBase)
		{
		 	if(UnitBase)
		 		switch (Type)
		 		{
		 	
					case SelectableData::Health:
		 				{
							UnitBase->SetHealth(UnitBase->Attributes->GetHealth() + Amount);
		 				}
		 			break;
		 			case SelectableData::Shield:
		 				{
		 					UnitBase->Attributes->SetShield(UnitBase->Attributes->GetShield() + Amount);
		 				}
		 				break;
		 			case SelectableData::WeaponTalentPoint:
		 			{
		 					ATalentedUnit* TalentedUnit = Cast<ATalentedUnit>(UnitBase);
		 					if(TalentedUnit)
		 					TalentedUnit->AddTalentPoint(Amount);
		 			}
		 			break;
		 			default:
		 				{
		 					
		 				}
		 			break;
		 		}

		 	if(Sound)
		 	UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);
		 	
			Destroy(true, false);
		}
			
	}
}

