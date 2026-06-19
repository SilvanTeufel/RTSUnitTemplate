// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

using UnrealBuildTool;

public class RTSUnitTemplate : ModuleRules
{
    public RTSUnitTemplate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Public/RTSUnitTemplate.h";

        // Public: only what our public headers require
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "UMG",
            "MassCore",
            "MassEntity",
            "MassCommon",
            "MassReplication",
            "MassActors",
            "MassRepresentation",
            "MassNavigation",
            "MassMovement",
            "NavigationSystem",
            "XRBase",
            "GameplayAbilities", 
            "GameplayTasks", 
            "GameplayTags",
            "Niagara",
            "EnhancedInput"
        });

        // Private: implementation-only dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Mass stack (implementation-only)
            "MassSimulation", "MassSpawner", "MassSignals", "MassAIBehavior", "MassLOD",

            // ZoneGraph
            "ZoneGraph", "ZoneGraphAnnotations",

            // Navigation & AI
            "AIModule",

            // UI
            "Slate", "SlateCore", "MoviePlayer",

            // Input & FX
            "InputCore",

            // Misc
            "Json", "JsonUtilities", "RenderCore", "Landscape",
            "NNE"
        });
        
    }
}
