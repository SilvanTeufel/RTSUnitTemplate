// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavFilter_Escape.generated.h"

/**
 * UNavFilter_Escape - Allows units trapped inside DirtyAreas (UNavArea_Obstacle) to exit.
 */
UCLASS()
class RTSUNITTEMPLATE_API UNavFilter_Escape : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UNavFilter_Escape(const FObjectInitializer& ObjectInitializer);
};
