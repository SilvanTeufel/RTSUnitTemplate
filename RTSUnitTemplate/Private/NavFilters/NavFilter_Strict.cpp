// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "NavFilters/NavFilter_Strict.h"
#include "NavAreas/NavArea_Obstacle.h"

UNavFilter_Strict::UNavFilter_Strict(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Strict Mode: Orange zones are absolute barriers
	FNavigationFilterArea ObstacleArea;
	ObstacleArea.AreaClass = UNavArea_Obstacle::StaticClass();
	ObstacleArea.bIsExcluded = true; // ABSOLUTELY BLOCKED!
	
	Areas.Add(ObstacleArea);
}
