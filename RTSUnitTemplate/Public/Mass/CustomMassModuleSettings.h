// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSettings.h"
#include "MassRepresentationProcessor.h"
#include "CustomMassModuleSettings.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UCustomMassModuleSettings : public UMassModuleSettings
{
	GENERATED_BODY()
	
	// You might list processor types here for easy access if needed
	// TArray<TSubclassOf<UMassProcessor>> DisplayProcessors;
};