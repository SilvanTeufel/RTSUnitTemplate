// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/AIController/CameraUnitController.h"

#include "Controller/PlayerController/ControllerBase.h"

void ACameraUnitController::BeginPlay()
{
	Super::BeginPlay();

	if(!ControllerBase)
	{
		ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());
	}
}

void ACameraUnitController::OnPossess(APawn* PawN)
{
	Super::OnPossess(PawN);

	MyUnitBase = Cast<AUnitBase>(PawN);
}

void ACameraUnitController::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);
	CameraUnitControlStateMachine(MyUnitBase, DeltaSeconds);
}

void ACameraUnitController::CameraUnitRunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	const FVector UnitLocation = UnitBase->GetActorLocation();
	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
		return;
	}

	if(UnitBase->GetVelocity().X == 0.0f && UnitBase->GetVelocity().Y == 0.0f) UnitBase->SetUEPathfinding = true;
	
	if(!UnitBase->SetUEPathfinding) return;

	SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->RunLocation);
}


void ACameraUnitController::CameraUnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds)
{

		
		//UE_LOG(LogTemp, Warning, TEXT("Controller UnitBase->Attributes! %f"), UnitBase->Attributes->GetAttackDamage());
		if(!UnitBase) return;
	

		//DetectAndLoseUnits();
	
		switch (UnitBase->UnitState)
		{
		case UnitData::None:
		{
			//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
		}
		break;
		case UnitData::Dead:
		{
				if(Debug)UE_LOG(LogTemp, Warning, TEXT("Dead"));
				Dead(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Patrol:
		{

		}
		break;
		case UnitData::PatrolRandom:
			{

			}
			break;
		case UnitData::PatrolIdle:
			{

			}
			break;
		case UnitData::Run:
			{
				CameraUnitRunUEPathfinding(UnitBase, DeltaSeconds);
			}
		break;
		case UnitData::Chase:
			{

			}
		break;
		case UnitData::Attack:
		{

		}
		break;
		case UnitData::Pause:
		{

		}
		break;

		case UnitData::IsAttacked:
		{

		}
		break;
		case UnitData::EvasionChase:
		{

		}
		break;
		case UnitData::Evasion:
		{

		}
		break;
		case UnitData::Rooted:
		{

		}
		break;
		case UnitData::Casting:
		{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
				Casting(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Idle:
		{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
				UnitBase->SetWalkSpeed(0);
		}
		break;
		default:
			{
				//UE_LOG(LogTemp, Warning, TEXT("default Idle"));
				UnitBase->SetUnitState(UnitData::Idle);
			}
		break;
		}
}