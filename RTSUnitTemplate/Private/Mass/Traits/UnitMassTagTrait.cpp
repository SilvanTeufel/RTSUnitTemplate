// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Traits/UnitMassTagTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/UnitMassTag.h"

void UUnitMassTagTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FUnitMassTag>();
}
