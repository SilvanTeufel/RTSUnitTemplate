// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Traits/MassActorLinkTrait.h"

// Source: MassActorLinkTrait.cpp

#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h" // For BuildContext
#include "MassRepresentationFragments.h"

void UMassActorLinkTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// Ensure the MassActors module is linked in Build.cs (should be already)
	BuildContext.AddFragment<FMassActorFragment>();
}
