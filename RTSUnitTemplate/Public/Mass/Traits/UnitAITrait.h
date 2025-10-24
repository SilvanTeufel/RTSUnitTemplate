// Fill out your copyright notice in the Description page of Project Settings.

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
