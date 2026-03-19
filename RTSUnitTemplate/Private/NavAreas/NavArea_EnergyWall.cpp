// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "NavAreas/NavArea_EnergyWall.h"

UNavArea_EnergyWall::UNavArea_EnergyWall(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultCost = 10000000.f; // Sehr hohe Base-Kosten, wird aber durch unseren Filter ohnehin exkludiert
    DrawColor = FColor::Orange; // Zur Unterscheidung im NavMesh-Debug
}
