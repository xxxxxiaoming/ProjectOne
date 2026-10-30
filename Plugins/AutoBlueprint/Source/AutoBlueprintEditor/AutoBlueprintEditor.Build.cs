using UnrealBuildTool;

public class AutoBlueprintEditor : ModuleRules
{
	public AutoBlueprintEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "EditorSubsystem"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry", "Kismet", "KismetCompiler", "Lua", "Projects",
			"Slate", "SlateCore", "UMG", "UMGEditor", "UnLua", "UnrealEd"
		});
	}
}
