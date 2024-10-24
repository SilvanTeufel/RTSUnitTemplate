// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/AIController/BuildingControllerBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "Characters/Unit/GASUnit.h"

void ABuildingControllerBase::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);
	BuildingControlStateMachine(MyUnitBase, DeltaSeconds);
}

void ABuildingControllerBase::BuildingControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds)
{
		//AUnitBase* UnitBase = Cast<AUnitBase>(GetPawn());
		//UE_LOG(LogTemp, Warning, TEXT("Controller UnitBase->Attributes! %f"), UnitBase->Attributes->GetAttackDamage());
		if(!UnitBase) return;

		CheckUnitDetectionTimer(DeltaSeconds);

		
		switch (UnitBase->UnitState)
		{
		case UnitData::None:
		{
			//UE_LOG(LogTemp, Warning, TEXT("None"));
		}
		break;
		case UnitData::Dead:
		{
			Dead(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Chase:
		{
				DetectAndLoseUnits();
			BuildingChase(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Attack:
		{
				AutoExecuteAbilitys(UnitBase, DeltaSeconds);
			AttackBuilding(UnitBase, DeltaSeconds);
				
		}
		break;
		case UnitData::Pause:
		{
				AutoExecuteAbilitys(UnitBase, DeltaSeconds);
			PauseBuilding(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Casting:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			CastingUnit(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Idle:
		{
			if(UnitBase->SetNextUnitToChase())
			{
					
				UnitBase->SetUnitState(UnitData::Chase);
				return;
			} 
				

			if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId != UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
			{
				UnitBase->UnitToChase = UnitBase->CollisionUnit;
				UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
				UnitBase->CollisionUnit = nullptr;
			}

			DetectAndLoseUnits();
				
			if(UnitBase->GetUnitState() == UnitData::Chase)
			{
				//UnitBase->SetUnitState(UnitData::Chase);
			}else
				SetUnitBackToPatrol(UnitBase, DeltaSeconds);

				
				AutoExecuteAbilitys(UnitBase, DeltaSeconds);

		}
		break;
		case UnitData::PatrolIdle:
		case UnitData::PatrolRandom:
		case UnitData::Patrol:
			{
				PatrolRandomBuilding(UnitBase, DeltaSeconds);
			}
			break;
		default:
		{
			//UE_LOG(LogTemp, Warning, TEXT("default Idle"));
			UnitBase->SetUnitState(UnitData::Idle);
		}
		break;
		}

	if (UnitBase->Attributes && UnitBase->Attributes->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
		KillUnitBase(UnitBase);
		UnitBase->UnitControlTimer = 0.f;
	}
}


void ABuildingControllerBase::CastingUnit(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase || !UnitBase->Attributes) return;
	
	UnitBase->SetWalkSpeed(0);
	UnitBase->UnitControlTimer += DeltaSeconds;

	if (UnitBase->UnitControlTimer > UnitBase->CastTime)
	{
		if (UnitBase->ActivatedAbilityInstance)
			UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete();
		
		UnitBase->SetWalkSpeed(0);
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}

void ABuildingControllerBase::AutoExecuteAbilitys(AUnitBase* UnitBase, float DeltaSeconds)
{

	UnitBase->UnitControlTimer += DeltaSeconds;
	if(UnitBase->UnitControlTimer >= ExecutenDelayTime || UnitBase->UnitControlTimer == 0.f)
	{
		UnitBase->UnitControlTimer = 0.f;
		
		if(AutoExeAbilitysArray[0])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilityOne, UnitBase->DefaultAbilities);
		if(AutoExeAbilitysArray[1])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilityTwo, UnitBase->DefaultAbilities);
		if(AutoExeAbilitysArray[2])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilityThree, UnitBase->DefaultAbilities);
		if(AutoExeAbilitysArray[3])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilityFour, UnitBase->DefaultAbilities);
		if(AutoExeAbilitysArray[4])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilityFive, UnitBase->DefaultAbilities);
		if(AutoExeAbilitysArray[5])
			UnitBase->ActivateAbilityByInputID(EGASAbilityInputID::AbilitySix, UnitBase->DefaultAbilities);
	}
}


void ABuildingControllerBase::BuildingChase(AUnitBase* UnitBase, float DeltaSeconds)
{
    if (!UnitBase) return;

	//DetectUnits(UnitBase, DeltaSeconds, false);
	//LoseUnitToChase(UnitBase);
	
	// If we lose sight of the unit, reset chase.
	if (!UnitBase->SetNextUnitToChase()) // If no unit is being chased, try to find one, otherwise set the pathfinding.
    {
        UnitBase->SetUEPathfinding = true;
        UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    } else
    {
       //UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
       //RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
       //DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);

        if (IsUnitToChaseInRange(UnitBase))
        {
            // Check if we can attack or should pause.
            if(UnitBase->UnitControlTimer >= UnitBase->PauseDuration)
            {
                UnitBase->ServerStartAttackEvent_Implementation();
                UnitBase->SetUnitState(UnitData::Attack);
                ActivateCombatAbilities(UnitBase);
            }
            else
            {
                UnitBase->SetUnitState(UnitData::Pause);
            }
        }
        else
        {
            // If we are flying, adjust the chase location.
            UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
            UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
       
        }
    }
}

void ABuildingControllerBase::PatrolRandomBuilding(AUnitBase* UnitBase, float DeltaSeconds)
{
	//DetectUnits(UnitBase, DeltaSeconds, true);
	//LoseUnitToChase(UnitBase);
	/*
	if(UnitBase->SetNextUnitToChase())
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
	}*/
}


void ABuildingControllerBase::AttackBuilding(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase) return;
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	if(UnitBase->SetNextUnitToChase())
	{
		
		if (UnitBase->UnitControlTimer > AttackDuration + UnitBase->PauseDuration) {
		
			if(!UnitBase->UseProjectile )
			{
				// Attack without Projectile
				if(IsUnitToChaseInRange(UnitBase))
				{
					//UE_LOG(LogTemp, Warning, TEXT("Unit is in Range, Attack without Projectile!"));
					float NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetArmor();
			
					if(UnitBase->IsDoingMagicDamage)
						NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetMagicResistance();
				
					if(UnitBase->UnitToChase->Attributes->GetShield() <= 0)
						UnitBase->UnitToChase->SetHealth_Implementation(UnitBase->UnitToChase->Attributes->GetHealth()-NewDamage);
					else
						UnitBase->UnitToChase->SetShield_Implementation(UnitBase->UnitToChase->Attributes->GetShield()-UnitBase->Attributes->GetAttackDamage());
						//UnitBase->UnitToChase->Attributes->SetAttributeShield(UnitBase->UnitToChase->Attributes->GetShield()-UnitBase->Attributes->GetAttackDamage());

					UnitBase->LevelData.Experience++;

					UnitBase->ServerMeeleImpactEvent();
					UnitBase->UnitToChase->ActivateAbilityByInputID(UnitBase->UnitToChase->DefensiveAbilityID, UnitBase->UnitToChase->DefensiveAbilities);
				
					if (!UnitBase->UnitToChase->UnitsToChase.Contains(UnitBase))
					{
						// If not, add UnitBase to the array
						UnitBase->UnitToChase->UnitsToChase.Emplace(UnitBase);
						UnitBase->UnitToChase->SetNextUnitToChase();
					}
				
					if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Attack &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Casting &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Rooted &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Pause)
					{
						UnitBase->UnitToChase->UnitControlTimer = 0.f;
						UnitBase->UnitToChase->SetUnitState( UnitData::IsAttacked );
					}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Casting)
					{
						UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceCastTime;
					}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Rooted)
					{
						UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceRootedTime;
					}
				}else
				{
					UnitBase->SetUnitState( UnitData::Chase );
				}
			}

			ProjectileSpawned = false;
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->SetUnitState( UnitData::Pause);
		}
	}else
	{
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}


void ABuildingControllerBase::PauseBuilding(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase) return;
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);
				
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
	
	if(!UnitBase->SetNextUnitToChase()) {

		//if(UnitBase->SetNextUnitToChase()) return;

		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState( UnitBase->UnitStatePlaceholder );
				
	} else if (UnitBase->UnitControlTimer > UnitBase->PauseDuration) {
		
		ProjectileSpawned = false;
		UnitBase->SetUnitState(UnitData::Chase);
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		
		if (IsUnitToChaseInRange(UnitBase)) {
				UnitBase->ServerStartAttackEvent_Implementation();
				UnitBase->SetUnitState(UnitData::Attack);

				UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
				UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
				if(UnitBase->Attributes->GetRange() >= 600.f) UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);

			
				CreateProjectile(UnitBase);
		}else
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
		
	}
}