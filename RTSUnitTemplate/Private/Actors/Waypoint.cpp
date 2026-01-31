// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Waypoint.h"
#include "Net/UnrealNetwork.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

// Sets default values
AWaypoint::AWaypoint()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger Box"));
	BoxComponent->SetupAttachment(SceneRoot);
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AWaypoint::OnPlayerEnter);


	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(SceneRoot);
	
	bReplicates = true;
	SetReplicateMovement(true);
	SetActorHiddenInGame(true); // Start hidden to prevent flickering
	FollowCharacter = nullptr;
}

void AWaypoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWaypoint, Mesh);
	DOREPLIFETIME(AWaypoint, Niagara_A);
	DOREPLIFETIME(AWaypoint, TeamId);
	DOREPLIFETIME(AWaypoint, NextWaypoint);
	DOREPLIFETIME(AWaypoint, FollowCharacter);
}

// Called when the game starts or when spawned
void AWaypoint::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisibility();
}

void AWaypoint::OnRep_TeamId()
{
	UpdateVisibility();
}

bool AWaypoint::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	if (const AControllerBase* PC = Cast<AControllerBase>(RealViewer))
	{
		// A waypoint is only relevant to its own team (or if it's neutral)
		if (TeamId == 0 || TeamId == PC->SelectableTeamId)
		{
			return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
		}
		return false;
	}
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AWaypoint::UpdateVisibility()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return; // Dedicated server

	if (AControllerBase* MyPC = Cast<AControllerBase>(PC))
	{
		// Strict team check for visibility, including on Listen Server hosts
		if (TeamId != MyPC->SelectableTeamId && TeamId != 0)
		{
			SetActorHiddenInGame(true);
		}
		else
		{
			SetActorHiddenInGame(false);
		}
	}
}

void AWaypoint::AddAssignedUnit(AUnitBase* Unit)
{
	if (Unit)
	{
		AssignedUnits.Add(Unit);
	}
}

void AWaypoint::RemoveAssignedUnit(AUnitBase* Unit)
{
	if (Unit)
	{
		AssignedUnits.Remove(Unit);
	}
}

// Called every frame
void AWaypoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		if (FollowCharacter)
		{
			if (!GetWorldTimerManager().IsTimerActive(FollowTimerHandle))
			{
				GetWorldTimerManager().SetTimer(FollowTimerHandle, this, &AWaypoint::UpdatePositionToFollowCharacter, FollowInterval, true);
			}
		}
		else
		{
			if (GetWorldTimerManager().IsTimerActive(FollowTimerHandle))
			{
				GetWorldTimerManager().ClearTimer(FollowTimerHandle);
			}
		}
	}
}

void AWaypoint::UpdatePositionToFollowCharacter()
{
	if (FollowCharacter)
	{
		SetActorLocation(FollowCharacter->GetActorLocation());
	}
}

void AWaypoint::OnPlayerEnter(UPrimitiveComponent* OverlapComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	
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
