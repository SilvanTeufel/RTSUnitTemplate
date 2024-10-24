// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/AIController/HealingUnitController.h"
#include "Perception/AiPerceptionComponent.h"
#include "Perception/AiSenseConfig_Sight.h"
#include "Actors/Waypoint.h"
#include "Controller/PlayerController/ControllerBase.h"

AHealingUnitController::AHealingUnitController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 
	DetectFriendlyUnits = true;
}

void AHealingUnitController::Tick(float DeltaSeconds)
{
	HealingUnitControlStateMachine(MyUnitBase, DeltaSeconds);
}

void AHealingUnitController::HealingUnitControlStateMachine(AUnitBase* Unit, float DeltaSeconds)
{
	AHealingUnit* UnitBase = Cast<AHealingUnit>(Unit);

	if(!UnitBase)
		return;

	CheckUnitDetectionTimer(DeltaSeconds);
	
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
			//DetectUnits(UnitBase, DeltaSeconds, false);
			//LoseUnitToChase(UnitBase);
	
			DetectAndLoseUnits();
		
			
			if(UnitBase->UsingUEPathfindingPatrol)
				HealPatrolUEPathfinding(UnitBase, DeltaSeconds);
			else
				HealPatrol(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::PatrolRandom:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("PatrolRandom"));

			if(UnitBase->SetNextUnitToChaseHeal())
			{
				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitData::Chase);
				return;
			}
			
			DetectAndLoseUnits();
			
			if(UnitBase->GetUnitState() != UnitData::Chase)
			{
				UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				SetUEPathfindingRandomLocation(UnitBase, DeltaSeconds);
			}

		}
		break;
	case UnitData::PatrolIdle:
		{
			if(UnitBase->SetNextUnitToChaseHeal())
			{
				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitData::Chase);
				return;
			}
			
			DetectAndLoseUnits();
			
			if(UnitBase->GetUnitState() != UnitData::Chase)
			{
				UnitBase->SetWalkSpeed(0);
				
				UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
				
				if(UnitBase->UnitControlTimer > UnitBase->NextWaypoint->RandomTime)
				{
					UnitBase->UnitControlTimer = 0.f;
					UnitBase->SetUEPathfinding = true;
					UnitBase->SetUnitState(UnitData::PatrolRandom);
				}
			}
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
	case UnitData::EvasionChase:
	case UnitData::Evasion:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			if(	UnitBase->CollisionUnit)
			{
				//UnitBase->EvadeDistance = GetCloseLocation(UnitBase->GetActorLocation(), 100.f);
				EvasionIdle(UnitBase, UnitBase->CollisionUnit->GetActorLocation());
				UnitBase->UnitControlTimer += DeltaSeconds;
			}
				
		}
		break;
	case UnitData::Idle:
		{
			//if(UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			UnitBase->SetWalkSpeed(0);

			if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead && !UnitBase->IsOnPlattform)
			{
				//UnitBase->UnitStatePlaceholder = UnitData::Idle;
				UnitBase->RunLocation = UnitBase->GetActorLocation();
				UnitBase->SetUnitState(UnitData::Evasion);
			}
			
			if(UnitBase->SetNextUnitToChaseHeal() && !UnitBase->IsOnPlattform)
			{
				UnitBase->SetUnitState(UnitData::Chase);
			}else if(!UnitBase->IsOnPlattform)
				SetUnitBackToPatrol(UnitBase, DeltaSeconds);
		}
		break;
	case UnitData::Casting:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			Casting(UnitBase, DeltaSeconds);
		}
		break;
	default: 
		{
			UnitBase->SetUnitState(UnitData::Idle);
		}
		break;
	}

	if (UnitBase->Attributes->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
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
    				UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
    				
    				RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
    				DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
    
    					AUnitBase* UnitToChase = UnitBase->UnitToChase;

						if(UnitToChase)
							UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
						
    					if (IsUnitToChaseInRange(UnitBase)) {

    						if(!bHealActorSpawned && UnitBase->UnitControlTimer >=  UnitBase->PauseDuration && UnitBase->UnitToChase->Attributes->GetHealth() < UnitBase->UnitToChase->Attributes->GetMaxHealth())
    						{
    							UnitBase->SpawnHealActor(UnitBase->UnitToChase);
    							UnitBase->ServerStartHealingEvent_Implementation();
    							UnitBase->SetUnitState(UnitData::Healing);
    							UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
    							UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
    							UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);

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

    						UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
    						UnitBase->SetUEPathfinding = true;
    						SetUEPathfinding(UnitBase, DeltaSeconds, UnitToChaseLocation);
    						//const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), UnitToChaseLocation);
    						//UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
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
	if (!UnitBase) return;
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);	
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	if (UnitBase->UnitControlTimer > AttackDuration +  UnitBase->PauseDuration) {
		UnitBase->SetUnitState( UnitData::Pause );
		bHealActorSpawned = false;
		UnitBase->UnitControlTimer = 0.f;
	}
	
}

void AHealingUnitController::HealPause(AHealingUnit* UnitBase, float DeltaSeconds)
{

		if (!UnitBase) return;
		//DetectUnits(UnitBase, DeltaSeconds, false);
	
		UnitBase->SetWalkSpeed(0);
		RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
		UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
	
		if(UnitBase->UnitToChase && (UnitBase->UnitToChase->GetUnitState() == UnitData::Dead || UnitBase->UnitToChase->Attributes->GetHealth() == UnitBase->UnitToChase->Attributes->GetMaxHealth())) {

			if(!UnitBase->SetNextUnitToChaseHeal())
			{
				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
				return;
			}
		}


		if (UnitBase->UnitControlTimer >  UnitBase->PauseDuration) {

			bHealActorSpawned = false;
			if (IsUnitToChaseInRange(UnitBase) && !bHealActorSpawned && UnitBase->SetNextUnitToChaseHeal()) {
				UnitBase->SpawnHealActor(UnitBase->UnitToChase);
				bHealActorSpawned = true;
				UnitBase->ServerStartHealingEvent_Implementation();
				
				UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
				UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
				UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
				
				UnitBase->SetUnitState(UnitData::Healing);
			}else if(!IsUnitToChaseInRange(UnitBase) && UnitBase->GetUnitState() != UnitData::Run)
			{
				UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				UnitBase->SetUnitState(UnitData::Chase);
			}
		
		}
}


void AHealingUnitController::HealRun(AHealingUnit* UnitBase, float DeltaSeconds)
{
	if(UnitBase->GetToggleUnitDetection() && UnitBase->SetNextUnitToChaseHeal())
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
		return;
	}

	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::Run;
		return;
	}
	

	DetectAndLoseUnits();
	
	if(UnitBase->GetUnitState() != UnitData::Chase)
	{
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
		const FVector UnitLocation = UnitBase->GetActorLocation();
		
		if(UnitBase->IsFlying)
		{
			UnitBase->RunLocation = FVector(UnitBase->RunLocation.X, UnitBase->RunLocation.Y, UnitBase->FlyHeight);
		}
	
		const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, UnitBase->RunLocation);
		UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());

		const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));
	
		if (Distance <= UnitBase->StopRunTolerance) {
			UnitBase->SetUnitState(UnitData::Idle);
		}
	
		UnitBase->UnitControlTimer = 0.f;
	}
}


void AHealingUnitController::HealRunUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds)
{

	if(UnitBase->GetToggleUnitDetection() && UnitBase->SetNextUnitToChaseHeal())
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
		return;
	}
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::Run;
		return;
	}

	DetectAndLoseUnits();
	
	if(UnitBase->GetUnitState() != UnitData::Chase)
	{
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

		const FVector UnitLocation = UnitBase->GetActorLocation();
		const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

		if (Distance <= UnitBase->StopRunTolerance) {
			UnitBase->SetUnitState(UnitData::Idle);
		}
	}
}

void AHealingUnitController::HealPatrol(AHealingUnit* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	if(IsUnitToChaseInRange(UnitBase))
	{
		UnitBase->SpawnHealActor(UnitBase->UnitToChase);
		bHealActorSpawned = true;
		UnitBase->ServerStartHealingEvent_Implementation();
		UnitBase->SetUnitState(UnitData::Healing);
	}else if(UnitBase->SetNextUnitToChaseHeal())
	{
	
		if(IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SpawnHealActor(UnitBase->UnitToChase);
			bHealActorSpawned = true;
			UnitBase->ServerStartHealingEvent_Implementation();
			UnitBase->SetUnitState(UnitData::Healing);
		}else if(!IsUnitToChaseInRange(UnitBase))
		{
			//UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			UnitBase->SetUEPathfinding = true;
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
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}else
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}


void AHealingUnitController::HealPatrolUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	if(IsUnitToChaseInRange(UnitBase))
	{
		UnitBase->SpawnHealActor(UnitBase->UnitToChase);
		bHealActorSpawned = true;
		UnitBase->ServerStartHealingEvent_Implementation();
		UnitBase->SetUnitState(UnitData::Healing);
	}else if(UnitBase->SetNextUnitToChaseHeal())
	{
		
		if(IsUnitToChaseInRange(UnitBase))
		{
			UnitBase->SpawnHealActor(UnitBase->UnitToChase);
			bHealActorSpawned = true;
			UnitBase->ServerStartHealingEvent_Implementation();
			UnitBase->SetUnitState(UnitData::Healing);
		}else if(!IsUnitToChaseInRange(UnitBase))
		{
			//UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			UnitBase->SetUEPathfinding = true;
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