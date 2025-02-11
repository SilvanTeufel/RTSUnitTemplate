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
	Super::Tick(DeltaSeconds);
	UnitControlStateMachine(MyUnitBase, DeltaSeconds);
}

void ACameraUnitController::Casting(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase || !UnitBase->Attributes) return;
	
	UnitBase->SetWalkSpeed(0);
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase, DeltaSeconds);
	UnitBase->UnitControlTimer += DeltaSeconds;

	if (UnitBase->UnitControlTimer > UnitBase->CastTime)
	{
		
		if (UnitBase->ActivatedAbilityInstance)
		{
			UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete();
		}

		
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}

void ACameraUnitController::RotateToAttackUnit(AUnitBase* UnitBase, AUnitBase* UnitToChase, float DeltaSeconds)
{
	if (!UnitBase || !UnitToChase)
	{
		// Invalid pointers, cannot rotate
		return;
	}

	// Get the current location of UnitBase and UnitToChase
	FVector BaseLocation = UnitBase->GetActorLocation();
	FVector TargetLocation = UnitToChase->GetActorLocation();

	// Calculate the direction vector from UnitBase to UnitToChase
	FVector Direction = TargetLocation - BaseLocation;

	// If the direction vector is too small, no need to rotate
	if (Direction.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Normalize the direction vector
	Direction.Normalize();

	// Calculate the desired rotation to face the target
	FRotator DesiredRotation = Direction.Rotation();

	// Get the current rotation of UnitBase
	FRotator CurrentRotation = UnitBase->GetActorRotation();

	// Define the rotation speed (degrees per second)
	float RotationSpeed = 720.0f; // Adjust as needed for desired rotation speed

	// Interpolate between current rotation and desired rotation
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaSeconds, RotationSpeed);

	// Apply the new rotation to UnitBase
	UnitBase->SetActorRotation(NewRotation);
}



void ACameraUnitController::UnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds)
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