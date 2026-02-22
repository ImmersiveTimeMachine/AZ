using UnrealBuildTool;

public class AZ : ModuleRules
{
	public AZ(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCore", "GameplayAbilities", "CommonUI", "CommonInput"});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"GameplayAbilities", 
			"GameplayTags",
			"GameplayTasks",
			"Paper2D",
			"CinematicCamera", 
			"Niagara",
			"UMG"
		});
		
		OptimizeCode = CodeOptimization.Never;
	}
}
