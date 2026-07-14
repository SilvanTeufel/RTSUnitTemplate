// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
using UnrealBuildTool;

public class RTSUnitTemplateTests : ModuleRules
{
    public RTSUnitTemplateTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "FunctionalTesting",
            "RTSUnitTemplate",
            "MassCore",
            "MassEntity",
            "MassCommon",
            "MassNavigation",
            "MassActors",
            "MassReplication"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
