// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/HealingUnitController.h"
#include "Perception/AiPerceptionComponent.h"
#include "Perception/AiSenseConfig_Sight.h"
#include "Actors/Waypoint.h"
#include "Controller/ControllerBase.h"

AHealingUnitController::AHealingUnitController()
{
	PrimaryActorTick.bCanEverTick = true;
	DetectFriendlyUnits = true;
}

void AHealingUnitController::Tick(float DeltaSeconds)
{
	HealingUnitControlStateMachine(DeltaSeconds);
}

void AHealingUnitController::HealingUnitControlStateMachine(float DeltaSeconds)
{
	AHealingUnit* UnitBase = Cast<AHealingUnit>(GetPawn());

	if(!UnitBase)
		return;


	switch (UnitBase->GetUnitState())
	{
	case UnitData::None:
		{
			//if(UnitBase->TeamId)UE_LOG(LogTemp, Warning, TEXT("None"));
		}
		break;
	case UnitData::Dead:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Dead"));
			Dead(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Patrol:
		{
			//UE_LOG(LogTemp, Warning, TEXT("Patrol"));
			if(UnitBase->UsingUEPathfindingPatrol)
				HealPatrolUEPathfinding(UnitBase, DeltaSeconds);
			else
				HealPatrol(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Run:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Run"));
		
			if(UnitBase->UEPathfindingUsed)
				HealRunUEPathfinding(UnitBase, DeltaSeconds);
			else
				HealRun(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Chase:
		{
			//UE_LOG(LogTemp, Warning, TEXT("Chase"));
			ChaseHealTarget(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Healing:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Healing"));
			Healing(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Pause:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("HealPause"));
			HealPause(UnitBase, DeltaSeconds);
		}
		break; 

	case UnitData::IsAttacked:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("IsAttacked"));
			IsAttacked(UnitBase, DeltaSeconds);
		}
		break;

	case UnitData::Idle:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			UnitBase->SetWalkSpeed(0);

			if(UnitBase->SetNextUnitToChaseHeal())
			{
				UnitBase->SetUnitState(UnitData::Chase);
			}
		}
		break;
	default:
		{
			UnitBase->SetUnitState(UnitData::Idle);
		}
		break;
	}

	if (UnitBase->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
		KillUnitBase(UnitBase);
		UnitBase->UnitControlTimer = 0.f;
	}
	
}

void AHealingUnitController::ChaseHealTarget(AHealingUnit* UnitBase, float DeltaSeconds)
{
					if(!UnitBase->SetNextUnitToChaseHeal())
					{
						UnitBase->SetUEPathfinding = true;
						UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
					
					}else if (UnitBase->UnitToChase) {
    				UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
    				
    				RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
    				DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
    
    					AUnitBase* UnitToChase = UnitBase->UnitToChase;

    					if (IsUnitToChaseInRange(UnitBase)) {

    						if(!bHealActorSpawned && UnitBase->UnitControlTimer >= PauseDuration && UnitBase->UnitToChase->GetHealth() < UnitBase->UnitToChase->GetMaxHealth())
    						{
    							UnitBase->SpawnHealActor(UnitBase->UnitToChase);
    							UnitBase->SetUnitState(UnitData::Healing);
    							bHealActorSpawned = true;
    						}else
    						{
    							UnitBase->SetUnitState(UnitData::Pause);
    						}	
    					}
    					else {
    						FVector UnitToChaseLocation = UnitToChase->GetActorLocation();
    						
    						if(UnitToChase->IsFlying && !UnitBase->IsFlying)
    						{
    							if (DistanceToUnitToChase > UnitBase->StopRunToleranceForFlying)
    							{
    								UnitBase->SetUnitState(UnitData::Idle);
    							}
    						}
    						
    						if(UnitBase->IsFlying)
    						{
    							UnitToChaseLocation =  FVector(UnitToChaseLocation.X, UnitToChaseLocation.Y, UnitBase->FlyHeight);
    						}
    						const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), UnitToChaseLocation);
    						
    						UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
    					}
    
    					if (DistanceToUnitToChase > LoseSightRadius) {
    						UnitBase->UnitsToChase.Remove(UnitBase->UnitToChase );
    						UnitBase->UnitToChase = nullptr;
    						
    						if(!UnitBase->TeamId && UnitBase->FollowPath)
    						{
    							UnitBase->RunLocationArray.Empty();
    							UnitBase->RunLocationArrayIterator = 0;
    							UnitBase->DijkstraStartPoint = UnitBase->GetActorLocation();
    							UnitBase->DijkstraEndPoint = UnitBase->NextWaypoint->GetActorLocation();
    							UnitBase->DijkstraSetPath = true;
    							UnitBase->FollowPath = false;
    							UnitBase->SetUEPathfinding = true;
    							UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    						}else
    						{
    							UnitBase->UnitsToChase.Remove(UnitBase->UnitToChase );
    							UnitBase->UnitToChase = nullptr;
    							UnitBase->SetUEPathfinding = true;
    							UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    						}
    						
    					}
    				}else
    				{
    					UnitBase->SetUEPathfinding = true;
    					UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    				}
}

void AHealingUnitController::Healing(AHealingUnit* UnitBase, float DeltaSeconds)
{
	
	UnitBase->SetWalkSpeed(0);	
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	if (UnitBase->UnitControlTimer > AttackDuration + PauseDuration) {
		UnitBase->SetUnitState( UnitData::Pause );
		bHealActorSpawned = false;
		UnitBase->UnitControlTimer = 0.f;
	}
	
}

void AHealingUnitController::HealPause(AHealingUnit* UnitBase, float DeltaSeconds)
{

		UnitBase->SetWalkSpeed(0);
		RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
		UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
	
		if(UnitBase->UnitToChase && (UnitBase->UnitToChase->GetUnitState() == UnitData::Dead || UnitBase->UnitToChase->GetHealth() == UnitBase->UnitToChase->GetMaxHealth())) {

			if(!UnitBase->SetNextUnitToChaseHeal())
			{
				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
				return;
			}
		}


		if (UnitBase->UnitControlTimer > PauseDuration) {

			bHealActorSpawned = false;
			if (IsUnitToChaseInRange(UnitBase) && !bHealActorSpawned && UnitBase->SetNextUnitToChaseHeal()) {
				UnitBase->SpawnHealActor(UnitBase->UnitToChase);
				bHealActorSpawned = true;
				UnitBase->SetUnitState(UnitData::Healing);
			}else if(!IsUnitToChaseInRange(UnitBase) && UnitBase->GetUnitState() != UnitData::Run)
			{
				UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				UnitBase->SetUnitState(UnitData::Chase);
			}
		
		}
}


void AHealingUnitController::HealRun(AHealingUnit* UnitBase, float DeltaSeconds)
{
	if(UnitBase->ToggleUnitDetection && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChaseHeal())
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
				
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
		
	if(UnitBase->IsFlying)
	{
		UnitBase->RunLocation = FVector(UnitBase->RunLocation.X, UnitBase->RunLocation.Y, UnitBase->FlyHeight);
	}
	
	const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, UnitBase->RunLocation);
	UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);

	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));
	
	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
	
	UnitBase->UnitControlTimer = 0.f;
}


void AHealingUnitController::HealRunUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds)
{

	if(UnitBase->ToggleUnitDetection && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChaseHeal())
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
	
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);

	const FVector UnitLocation = UnitBase->GetActorLocation();
	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AHealingUnitController::HealPatrol(AHealingUnit* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
	if(UnitBase->SetNextUnitToChaseHeal())
	{
		if(IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SpawnHealActor(UnitBase->UnitToChase);
			bHealActorSpawned = true;
			UnitBase->SetUnitState(UnitData::Healing);
		}else if(!IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			UnitBase->SetUnitState(UnitData::Chase);
		}

	} else if (UnitBase->NextWaypoint != nullptr)
	{

		FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();

		if(UnitBase->IsFlying)
		{
			WaypointLocation =  FVector(WaypointLocation.X, WaypointLocation.Y, UnitBase->FlyHeight);
		}

		if(UnitBase->FollowPath)
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), UnitBase->RunLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
		}else
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
		}
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}


void AHealingUnitController::HealPatrolUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
	if(UnitBase->SetNextUnitToChaseHeal())
	{
		if(IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SpawnHealActor(UnitBase->UnitToChase);
			bHealActorSpawned = true;
			UnitBase->SetUnitState(UnitData::Healing);
		}else if(!IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			UnitBase->SetUnitState(UnitData::Chase);
		}

	} else if (UnitBase->NextWaypoint != nullptr)
	{
		SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->NextWaypoint->GetActorLocation());
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}