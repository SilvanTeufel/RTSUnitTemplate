// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "UnitAITrait.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitAITrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
protected:
	/** This is the function that actually adds the fragments */
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

};
