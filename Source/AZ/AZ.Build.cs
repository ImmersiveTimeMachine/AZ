using UnrealBuildTool;

public class AZ : ModuleRules
{
	public AZ(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCore", "GameplayAbilities", "GameplayTags", "AIModule", "NavigationSystem", "CommonUI", "CommonInput", "PoseSearch", "MotionTrajectory", "Mover", "NetworkPrediction", "Chooser", "StructUtils", "AssetRegistry", "BlendStack", "AnimationWarpingRuntime", "AnimGraphRuntime", "MotionWarping" });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ApplicationCore", // Local input-device pairing and connection notifications.
			"DeveloperSettings", // Runtime platform settings used by CommonInput glyph selection.
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTasks",
			"Paper2D",
			"CinematicCamera",
			"EngineCameras",   // Perlin/WaveOscillator camera shake patterns (grab struggle shake)
			"Niagara",
			"GeometryCollectionEngine", // Component-scoped grenade destruction, excluding character physics.
			"FieldSystemEngine",
			"UMG"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UMGEditor", "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "BlendStackEditor", "PoseSearchEditor", "AnimationWarpingEditor", "AnimGraphRuntime", "ControlRig", "ControlRigDeveloper" });
		}

		SetupIrisSupport(Target);

		OptimizeCode = CodeOptimization.Never;
	}
}
