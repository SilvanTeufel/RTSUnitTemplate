// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Traits/UnitNavigationPathTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/UnitNavigationFragments.h"

void UUnitNavigationPathTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FUnitNavigationPathFragment>();
}
