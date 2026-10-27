// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavFilter_LogisticsRoad.generated.h"

/**
 * Wegsuche fuer Strassen: nur das freie Navmesh. Gebaeudegrundrisse (UNavArea_Obstacle) und
 * geschlossene EnergyWalls (UNavArea_EnergyWall) sind gesperrt - fuer den Standardfilter sind
 * beide nur teuer, und eine Strasse lief durch sie hindurch, sobald der Umweg zu lang war.
 */
UCLASS()
class RTSUNITTEMPLATE_API UNavFilter_LogisticsRoad : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UNavFilter_LogisticsRoad(const FObjectInitializer& ObjectInitializer);
};
