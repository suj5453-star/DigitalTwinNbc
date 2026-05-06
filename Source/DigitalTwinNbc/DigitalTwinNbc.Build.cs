// Copyright NBC, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DigitalTwinNbc : ModuleRules
{
	public DigitalTwinNbc(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ChaosVehicles", "PhysicsCore", "Landscape", "RenderCore", "RHI", "ImageWrapper" });
	}
}
