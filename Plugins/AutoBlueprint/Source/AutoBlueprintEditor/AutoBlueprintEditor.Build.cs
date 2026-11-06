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
		// P1 uses console commands; no ToolMenus or JSON dependency is needed yet.
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry", "Kismet", "KismetCompiler", "Lua", "Projects",
			"Slate", "SlateCore", "UMG", "UMGEditor", "UnLua", "UnrealEd"
		});
	}
}
