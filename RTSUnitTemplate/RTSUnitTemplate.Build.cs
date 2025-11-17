// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

using UnrealBuildTool;

public class RTSUnitTemplate : ModuleRules
{
    public RTSUnitTemplate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public: only what our public headers require
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "MassEntity",
            "MassCommon",
            "MassReplication",
            "MassActors"
        });

        // Private: implementation-only dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Mass stack (implementation-only)
            "MassSimulation", "MassSpawner", "MassMovement", "MassNavigation", "MassSignals",
            "MassRepresentation", "MassAIBehavior", "MassLOD",

            // ZoneGraph
            "ZoneGraph", "ZoneGraphAnnotations",

            // Gameplay Ability System & tags
            "GameplayAbilities", "GameplayTasks", "GameplayTags",

            // Navigation & AI
            "NavigationSystem", "AIModule",

            // UI
            "UMG", "Slate", "SlateCore", "MoviePlayer",

            // Input & FX
            "InputCore", "EnhancedInput", "Niagara",

            // Misc
            "StructUtils", "Json", "JsonUtilities", "RenderCore", "XRBase", "Landscape",
            "NNE"
        });
    }
}
