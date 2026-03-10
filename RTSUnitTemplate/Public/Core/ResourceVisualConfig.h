// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/WorkerData.h"
#include "ResourceVisualConfig.generated.h"

UCLASS(BlueprintType)
class RTSUNITTEMPLATE_API UResourceVisualConfig : public UDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
    TMap<EResourceType, UStaticMesh*> DefaultResourceMeshes;
};
