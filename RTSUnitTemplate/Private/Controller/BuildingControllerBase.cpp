// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/BuildingControllerBase.h"
#include "Characters/Unit/GASUnit.h"

void ABuildingControllerBase::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);
	BuildingControlStateMachine(DeltaSeconds);
}

void ABuildingControllerBase::BuildingControlStateMachine(float DeltaSeconds)
{
		AUnitBase* UnitBase = Cast<AUnitBase>(GetPawn());
		//UE_LOG(LogTemp, Warning, TEXT("Controller UnitBase->Attributes! %f"), UnitBase->Attributes->GetAttackDamage());
		if(!UnitBase) return;
	
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
			BuildingChase(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Attack:
		{
			Attack(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Pause:
		{
			Pause(UnitBase, DeltaSeconds);
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

			if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId != UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
			{
				UnitBase->UnitToChase = UnitBase->CollisionUnit;
				UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
				UnitBase->CollisionUnit = nullptr;
			}

			if(UnitBase->UnitsToChase.Num())
			{
				UnitBase->SetUnitState(UnitData::Chase);
			}else
				SetUnitBackToPatrol(UnitBase, DeltaSeconds);

				
				AutoExecuteAbilitys(UnitBase, DeltaSeconds);

		}
		break;
		default:
		{
			//UE_LOG(LogTemp, Warning, TEXT("default Idle"));
			UnitBase->SetUnitState(UnitData::Idle);
		}
		break;
		}

	if (UnitBase->Attributes->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
		KillUnitBase(UnitBase);
		UnitBase->UnitControlTimer = 0.f;
	}
}


void ABuildingControllerBase::CastingUnit(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase || !UnitBase->Attributes) return;
	
	UnitBase->SetWalkSpeed(0);
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer += DeltaSeconds;

	if (UnitBase->UnitControlTimer > UnitBase->CastTime)
	{
		if (UnitBase->ActivatedAbilityInstance)
		UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete();
		
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}

void ABuildingControllerBase::AutoExecuteAbilitys(AUnitBase* UnitBase, float DeltaSeconds)
{

	UnitBase->UnitControlTimer += DeltaSeconds;
	if(UnitBase->UnitControlTimer >= ExecutenDelayTime)
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

	if (!UnitBase->SetNextUnitToChase()) // If no unit is being chased, try to find one, otherwise set the pathfinding.
    {
        UnitBase->SetUEPathfinding = true;
        UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    } else
    {
       //UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
       //RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
       DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);

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

        // If we lose sight of the unit, reset chase.
        if (DistanceToUnitToChase > LoseSightRadius) 
        {
            LoseUnitToChase(UnitBase);
        }
    }
}