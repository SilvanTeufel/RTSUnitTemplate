// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Traits/UnitMassTagTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/UnitMassTag.h"

void UUnitMassTagTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FUnitMassTag>();
}
