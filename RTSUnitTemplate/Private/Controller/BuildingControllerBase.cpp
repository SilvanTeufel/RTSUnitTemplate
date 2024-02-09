// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/BuildingControllerBase.h"


void ABuildingControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
			UE_LOG(LogTemp, Warning, TEXT("None"));
		}
		break;
		case UnitData::Dead:
		{
			Dead(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Chase:
		{
			Chase(UnitBase, DeltaSeconds);
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
