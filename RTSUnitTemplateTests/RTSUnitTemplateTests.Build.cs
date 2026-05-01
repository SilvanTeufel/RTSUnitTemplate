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
            "MassEntity",
            "MassCommon",
            "MassNavigation",
            "MassActors"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
