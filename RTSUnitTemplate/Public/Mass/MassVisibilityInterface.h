// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MassVisibilityInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMassVisibilityInterface : public UInterface
{
	GENERATED_BODY()
};

class IMassVisibilityInterface
{
	GENERATED_BODY()

public:
	virtual void SetActorVisibility(bool bVisible) = 0;
	virtual void SetEnemyVisibility(AActor* DetectingActor, bool bVisible) = 0;
	virtual bool ComputeLocalVisibility() const = 0;
};
