// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/IndicatorActor.h"

// Sets default values
AIndicatorActor::AIndicatorActor(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	DamageIndicatorComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("DamageIndicator"));
	DamageIndicatorComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	
}

// Called when the game starts or when spawned

void AIndicatorActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnDamageIndicator(555.f);
}

// Called every frame
void AIndicatorActor::Tick(float DeltaTime)
{
	
	Super::Tick(DeltaTime);
	LifeTime = LifeTime + DeltaTime;
	if(LifeTime > MaxLifeTime)
	{
		Destroy(true);
	}else
	{
		float X = FMath::FRandRange(MinDrift, MaxDrift);
		float Y = FMath::FRandRange(MinDrift, MaxDrift);
		float Z = ZDrift;
		SetActorLocation(GetActorLocation()+ FVector(X, Y, Z));
	}
}


void AIndicatorActor::SpawnDamageIndicator(float Damage)
{

	if (DamageIndicatorComp) {

		DamageIndicatorComp->SetWorldLocation(GetActorLocation()+DamageIndicatorCompLocation);
		UDamageIndicator* DamageIndicator = Cast<UDamageIndicator>(DamageIndicatorComp->GetUserWidgetObject());
		if (DamageIndicator)
		{
			LastDamage = Damage;
			DamageIndicator->SetDamage(Damage);
		}

	}
}
