using UnrealBuildTool;

public class AutoBlueprintEditor : ModuleRules
{
	public AutoBlueprintEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public contracts expose only Core/Engine/editor subsystem types.
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "EditorSubsystem"
		});

		// Lua hosting, script discovery and asset writes remain editor implementation details.
		// JSON validation and image inspection are read-only P2 implementation details.
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json", "ImageWrapper", "AssetRegistry", "Kismet", "KismetCompiler", "Lua", "Projects",
			"Slate", "SlateCore", "UMG", "UMGEditor", "UnLua", "UnrealEd"
		});
	}
}
