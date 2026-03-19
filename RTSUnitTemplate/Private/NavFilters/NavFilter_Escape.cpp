// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "NavFilters/NavFilter_Escape.h"
#include "NavAreas/NavArea_Obstacle.h"

UNavFilter_Escape::UNavFilter_Escape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Escape Mode: Allow units trapped inside to leave
	FNavigationFilterArea ObstacleArea;
	ObstacleArea.AreaClass = UNavArea_Obstacle::StaticClass();
	ObstacleArea.bIsExcluded = false; // Allows escape
	ObstacleArea.TravelCostOverride = 5.0f; // Low cost to quickly find an exit
	ObstacleArea.EnteringCostOverride = 5.0f;
	
	Areas.Add(ObstacleArea);
}
