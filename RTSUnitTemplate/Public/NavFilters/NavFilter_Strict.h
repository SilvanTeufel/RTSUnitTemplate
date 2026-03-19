// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavFilter_Strict.generated.h"

/**
 * UNavFilter_Strict - Completely blocks DirtyAreas (UNavArea_Obstacle).
 */
UCLASS()
class RTSUNITTEMPLATE_API UNavFilter_Strict : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UNavFilter_Strict(const FObjectInitializer& ObjectInitializer);
};
