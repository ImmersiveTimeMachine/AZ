// Copyright Artur. AZ project.

using UnrealBuildTool;

/** AZ Natural Grip: editor tool that solves natural hand grasps on held weapons (see
 *  docs/design-briefs/natural-grip-cpp-plugin.md). Private/Core is the engine-agnostic solver core, also compiled by the
 *  standalone parity harness in Tools/ngtest. */
public class AZNaturalGrip : ModuleRules
{
	public AZNaturalGrip(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// the core's .cpp files keep file-local helpers in anonymous namespaces with common names
		bUseUnity = false;
		// the core reports bad data files with exceptions (caught at the module boundary)
		bEnableExceptions = true;

		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "Core"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AnimationBlueprintLibrary",
			"ApplicationCore",
			"AssetRegistry",
			"AssetTools",
			"EditorFramework",
			"GeometryCore",
			"InputCore",
			"Json",
			"MeshConversion",
			"MeshDescription",
			"Projects",
			"PropertyEditor",
			"SkeletalMeshDescription",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			"ToolMenus",
			"UnrealEd",
		});
	}
}
