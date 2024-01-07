// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/MissileRain.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
AMissileRain::AMissileRain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMissileRain::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMissileRain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTime+=DeltaTime;
	RainTime+=DeltaTime;
	
	if(LifeTime >= MaxLifeTime){
		Destroy(true, false);
	}else if(RainTime > SpawnMissileTime)
	{
		// Use SpawnCounter to Add Random Location Offsets

		SpawnMissile(TeamId, GetActorLocation(), DeltaTime);
		RainTime = 0.f;
		if(SpawnCounter >= MissileCount)
		{
			Destroy(true, false);
		}
	}
}

void AMissileRain::SpawnMissile(int TId, FVector SpawnLocation, float DeltaTime)
{
			FVector RandomOffset = FVector(FMath::FRandRange(-MissileRange, MissileRange), FMath::FRandRange(-MissileRange, MissileRange), 0.f);
			FVector SpawnLocationWithOffset = SpawnLocation + RandomOffset;

			FTransform Transform;
			Transform.SetLocation(SpawnLocationWithOffset);
			Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator
			Transform.SetScale3D(ScaleMissile);
		
			const auto MyMissile = Cast<AMissile>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, MissileBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
			if (MyMissile != nullptr)
			{
		
				MyMissile->Init(TId);
				MyMissile->Mesh->OnComponentBeginOverlap.AddDynamic(MyMissile, &AMissile::OnOverlapBegin);
				UGameplayStatics::FinishSpawningActor(MyMissile, Transform);
			}

			SpawnCounter++;
	
}