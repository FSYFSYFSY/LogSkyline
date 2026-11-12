// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LogSkyline : ModuleRules
{
	public LogSkyline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"LogSkyline",
			"LogSkyline/Variant_Platforming",
			"LogSkyline/Variant_Platforming/Animation",
			"LogSkyline/Variant_Combat",
			"LogSkyline/Variant_Combat/AI",
			"LogSkyline/Variant_Combat/Animation",
			"LogSkyline/Variant_Combat/Gameplay",
			"LogSkyline/Variant_Combat/Interfaces",
			"LogSkyline/Variant_Combat/UI",
			"LogSkyline/Variant_SideScrolling",
			"LogSkyline/Variant_SideScrolling/AI",
			"LogSkyline/Variant_SideScrolling/Gameplay",
			"LogSkyline/Variant_SideScrolling/Interfaces",
			"LogSkyline/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
