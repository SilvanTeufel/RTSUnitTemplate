// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/IndicatorActor.h"

#include "Net/UnrealNetwork.h"

// Sets default values
AIndicatorActor::AIndicatorActor(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	DamageIndicatorComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("DamageIndicator"));
	RootComponent = DamageIndicatorComp;

	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
	}
}

void AIndicatorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AIndicatorActor, DamageIndicatorComp);
	DOREPLIFETIME(AIndicatorActor, LastDamage);
	DOREPLIFETIME(AIndicatorActor, DamageIndicatorCompLocation);
	DOREPLIFETIME(AIndicatorActor, LifeTime);
	DOREPLIFETIME(AIndicatorActor, MaxLifeTime);
	DOREPLIFETIME(AIndicatorActor, MinDrift);
	DOREPLIFETIME(AIndicatorActor, MaxDrift);
	DOREPLIFETIME(AIndicatorActor, ZDrift);
}

// Called when the game starts or when spawned

void AIndicatorActor::BeginPlay()
{
	Super::BeginPlay();
	//SpawnDamageIndicator(555.f);
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


void AIndicatorActor::SpawnDamageIndicator(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset)
{

	if (HasAuthority()) // Ensure this is running on the server
	{
		// Update replicated properties
		LastDamage = Damage;

		// Notify clients to update their widgets
		Multicast_UpdateWidget(Damage, HighColor, LowColor, ColorOffset);
	}
}

void AIndicatorActor::Multicast_UpdateWidget_Implementation(float Damage, FLinearColor HighColor, FLinearColor LowColor,
	float ColorOffset)
{
	if (DamageIndicatorComp)
	{
		DamageIndicatorComp->SetWorldLocation(GetActorLocation() + DamageIndicatorCompLocation);
		UDamageIndicator* DamageIndicator = Cast<UDamageIndicator>(DamageIndicatorComp->GetUserWidgetObject());
		if (DamageIndicator)
		{
			DamageIndicator->SetDamage(Damage);
			DamageIndicator->SetColour(HighColor, LowColor, ColorOffset);
		}
	}
}
