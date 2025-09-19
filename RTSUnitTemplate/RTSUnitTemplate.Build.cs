// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

using UnrealBuildTool;

public class RTSUnitTemplate : ModuleRules
{
	public RTSUnitTemplate(ReadOnlyTargetRules Target) : base(Target)
	{
		//PrivateDependencyModuleNames.AddRange(new string[] { "AITestSuite", "AITestSuite" });
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Core dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "NNE", "Json", "CoreUObject", "RenderCore", "Engine", "Niagara", "InputCore", "EnhancedInput", "GameplayTags", "XRBase", "Landscape"});
		
		// Mass dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "MassEntity", "MassSimulation", "MassSpawner", "MassMovement", "MassNavigation", "MassCommon", "MassActors", "MassSignals", "MassRepresentation", "MassReplication", "MassAIReplication", "MassAIBehavior", "NavigationSystem", "StructUtils", "MassLOD", "ZoneGraph",
			"ZoneGraphAnnotations", "MassGameplayDebug"  });
		
		// Gameplay Ability System
		PublicDependencyModuleNames.AddRange(new string[] { "NetCore", "GameplayAbilities", "GameplayTags", "GameplayTasks"});
		
		// Slate dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore",});
		
		// EOS dependencies
		// PublicDependencyModuleNames.AddRange(new string[] { "Networking", "Sockets", "OnlineSubsystemEOS", "OnlineSubsystem", "OnlineSubsystemUtils" });

		// Ai dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "AIModule" });

		// Hud dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "HeadMountedDisplay", "NavigationSystem" });

		// Widget dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "UMG", "MoviePlayer" });
		
		// Json dependencies
		PublicDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities" });
	
		
	}
}
