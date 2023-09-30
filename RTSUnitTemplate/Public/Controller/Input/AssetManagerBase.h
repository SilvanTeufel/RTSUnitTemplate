// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "AssetManagerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UAssetManagerBase : public UAssetManager
{
	GENERATED_BODY()

public:

	UAssetManagerBase();

	// Returns the AssetManager singleton object.
	static UAssetManagerBase& Get();

protected:

	virtual void StartInitialLoading() override;
};