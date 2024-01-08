// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EffectArea.h"

#include "Net/UnrealNetwork.h"


// Sets default values
AEffectArea::AEffectArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
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

// Called when the game starts or when spawned
void AEffectArea::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEffectArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTime+=DeltaTime;
	
	if(LifeTime >= MaxLifeTime){
		Destroy(true, false);
	}
	
}

void AEffectArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEffectArea, AreaEffect);
	DOREPLIFETIME(AEffectArea, Mesh);
}



void AEffectArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffect);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffect);
		}
	}
}
