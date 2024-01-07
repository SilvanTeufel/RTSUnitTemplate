// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Waypoint.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/ControllerBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AWaypoint::AWaypoint()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
	SetRootComponent(Root);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger Box"));
	BoxComponent->SetupAttachment(GetRootComponent());
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AWaypoint::OnPlayerEnter);


}

// Called when the game starts or when spawned
void AWaypoint::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AWaypoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWaypoint::OnPlayerEnter_Implementation(UPrimitiveComponent* OverlapComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (OtherActor != nullptr) {
		ActualCharacter = Cast<AUnitBase>(OtherActor);
		if (ActualCharacter != nullptr &&
			ActualCharacter->UnitState != UnitData::Attack &&
			ActualCharacter->UnitState != UnitData::Chase &&
			ActualCharacter->UnitState != UnitData::IsAttacked &&
			ActualCharacter->UnitState != UnitData::Pause &&
			ActualCharacter->UnitState != UnitData::Run
			) {
			if(PatrolCloseToWaypoint)
			{
				//SetupTimerFunction();
				RandomTime = FMath::FRandRange(PatrolCloseMinInterval, PatrolCloseMaxInterval);
				ActualCharacter->SetUEPathfinding = true;
				ActualCharacter->SetUnitState(UnitData::PatrolRandom);
			}
			else if(NextWaypoint)
			{
				ActualCharacter->SetWaypoint(NextWaypoint);
				ActualCharacter->SetUEPathfinding = true;
				ActualCharacter->RunLocationArray.Empty();
				ActualCharacter->RunLocationArrayIterator = 0;
				
				ActualCharacter->DijkstraStartPoint = ActualCharacter->GetActorLocation();
				ActualCharacter->DijkstraEndPoint = NextWaypoint->GetActorLocation();
				ActualCharacter->DijkstraSetPath = true;
				ActualCharacter->FollowPath = false;
			}
		}
	}
}
