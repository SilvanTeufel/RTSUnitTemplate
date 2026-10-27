// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "NavFilters/NavFilter_LogisticsRoad.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavAreas/NavArea_EnergyWall.h"

UNavFilter_LogisticsRoad::UNavFilter_LogisticsRoad(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Jede Flaechenklasse hat eine eigene Kennung - die abgeleitete EnergyWall-Flaeche muss eigens
	// gesperrt werden, die Obstacle-Zeile allein erfasst sie nicht.
	FNavigationFilterArea Gebaeude;
	Gebaeude.AreaClass = UNavArea_Obstacle::StaticClass();
	Gebaeude.bIsExcluded = true;
	Areas.Add(Gebaeude);

	FNavigationFilterArea Wand;
	Wand.AreaClass = UNavArea_EnergyWall::StaticClass();
	Wand.bIsExcluded = true;
	Areas.Add(Wand);
}
