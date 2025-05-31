// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "UnitMassTagTrait.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitMassTagTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};