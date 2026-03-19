// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavArea_EnergyWall.generated.h"

/**
 * Spezifische NavArea für undurchdringliche Energy Walls.
 */
UCLASS()
class RTSUNITTEMPLATE_API UNavArea_EnergyWall : public UNavArea_Obstacle
{
    GENERATED_BODY()

public:
    UNavArea_EnergyWall(const FObjectInitializer& ObjectInitializer);
};
