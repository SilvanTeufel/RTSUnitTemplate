// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassActorLinkTrait.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassActorLinkTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
protected:
	// CORRECTED SIGNATURE - Matching the header you provided
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};