// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/BuildingControllerBase.h"


void ABuildingControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BuildingControlStateMachine(DeltaSeconds);
}

void ABuildingControllerBase::BuildingControlStateMachine(float DeltaSeconds)
{
}
