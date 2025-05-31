// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Traits/UnitNavigationPathTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/UnitNavigationFragments.h"

void UUnitNavigationPathTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FUnitNavigationPathFragment>();
}
