// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "UnitNavigationPathTrait.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitNavigationPathTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};