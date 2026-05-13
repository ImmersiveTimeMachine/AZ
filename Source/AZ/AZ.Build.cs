using UnrealBuildTool;

public class AZ : ModuleRules
{
	public AZ(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCore", "GameplayAbilities", "GameplayTags", "AIModule", "CommonUI", "CommonInput", "PoseSearch", "MotionTrajectory", "Mover", "NetworkPrediction", "Chooser", "StructUtils", "AssetRegistry", "BlendStack", "AnimationWarpingRuntime", "AnimGraphRuntime" });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTasks",
			"Paper2D",
			"CinematicCamera",
			"Niagara",
			"UMG"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UMGEditor", "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "BlendStackEditor", "PoseSearchEditor", "AnimationWarpingEditor", "AnimGraphRuntime" });
		}
		
		OptimizeCode = CodeOptimization.Never;
	}
}
