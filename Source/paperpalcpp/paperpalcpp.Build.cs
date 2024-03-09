// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class paperpalcpp : ModuleRules
{
	public paperpalcpp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
