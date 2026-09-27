// Copyright Artur. AZ project.

using UnrealBuildTool;

/** Editor-only (UncookedOnly) companion of the AZ module: AnimGraph editor nodes for AZ runtime anim nodes. */
public class AZEditor : ModuleRules
{
	public AZEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraph",
			"AnimGraphRuntime",
			"AZ",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"BlueprintGraph",
			"Slate",
			"SlateCore",
			"UnrealEd",
		});
	}
}
