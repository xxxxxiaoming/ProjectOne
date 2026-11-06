#include "Modules/ModuleManager.h"

#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintLog.h"
#include "AutoBlueprintLuaTypes.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogAutoBlueprint);

class FAutoBlueprintEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("UMGEditor"));
		RunCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.Run"),
			TEXT("Run a trusted plugin tool: AutoBlueprint.Run <relative_script.lua> [arguments...]"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::RunScript));
		GenerateFromFileCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.GenerateFromFile"),
			TEXT("Invoke generate_from_file.lua. P1 reports NOT_IMPLEMENTED until P2/P3 layout support is available."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::GenerateFromFile));
		HelpCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.Help"),
			TEXT("Show tool commands and current capabilities."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::ShowHelp));
		GenerateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.P0.Generate"),
			TEXT("Generate the fixed P0 fixture: AutoBlueprint.P0.Generate /Game/UI/AutoBlueprintTests/WBP_P0_Review01"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::Generate));
		ErrorCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.P0.ErrorProbe"),
			TEXT("Run an intentional Lua error without asset writes; expect LUA_SCRIPT_FAILED and a traceback."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::ProbeError));
	}

	virtual void ShutdownModule() override
	{
		// Remove all delegates into this module. No task VM survives its command invocation.
		ErrorCommand.Reset();
		GenerateCommand.Reset();
		HelpCommand.Reset();
		GenerateFromFileCommand.Reset();
		RunCommand.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		// Reflected types and Lua C closures require a full editor restart when this module changes.
		return false;
	}

private:
	void RunScript(const TArray<FString>& Args)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.Run <relative_script.lua> [arguments...]"));
			return;
		}
		FAutoBlueprintLuaRunRequest Request;
		Request.ScriptPath = Args[0];
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			Request.Arguments.Add(Args[Index]);
		}
		ReportRun(UAutoBlueprintLibrary::RunEditorScript(Request));
	}

	void GenerateFromFile(const TArray<FString>& Args)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.GenerateFromFile requires one project-relative layout path."));
			return;
		}
		RunBundled(TEXT("generate_from_file.lua"), Args);
	}

	void Generate(const TArray<FString>& Args)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.P0.Generate requires one new package path below /Game/UI/AutoBlueprintTests/."));
			return;
		}
		RunBundled(TEXT("p0_generate.lua"), Args);
	}

	void ProbeError(const TArray<FString>& Args)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.P0.ErrorProbe takes no arguments."));
			return;
		}
		RunBundled(TEXT("p0_error.lua"), Args);
	}

	void ShowHelp(const TArray<FString>& Args)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.Help takes no arguments."));
			return;
		}
		UE_LOG(LogAutoBlueprint, Log, TEXT("AutoBlueprint.Run <relative_script.lua> [arguments...] - root: Plugins/AutoBlueprint/Scripts/Lua"));
		UE_LOG(LogAutoBlueprint, Log, TEXT("AutoBlueprint.Run examples/hello.lua - no asset writes; quote arguments containing spaces."));
		UE_LOG(LogAutoBlueprint, Log, TEXT("AutoBlueprint.P0.Generate <new_package_path> - fixed Widget Blueprint; AutoBlueprint.P0.ErrorProbe - traceback probe."));
		UE_LOG(LogAutoBlueprint, Log, TEXT("AutoBlueprint.GenerateFromFile <layout_path> - P1 entry only; JSON generation (P2/P3) and GenerateBatch are not implemented."));
	}

	void RunBundled(const FString& ScriptPath, const TArray<FString>& Arguments)
	{
		FAutoBlueprintLuaRunRequest Request;
		Request.ScriptPath = ScriptPath;
		Request.Arguments = Arguments;
		ReportRun(UAutoBlueprintLibrary::RunEditorScript(Request));
	}

	void ReportRun(const FAutoBlueprintLuaRunResult& Result) const
	{
		if (Result.bSuccess)
		{
			UE_LOG(LogAutoBlueprint, Log, TEXT("AutoBlueprint Lua task completed; its tool environment has been released: %s"), *Result.ScriptPath);
		}
		else
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("%s: %s"), *Result.Code, *Result.Message);
		}
	}

	TUniquePtr<FAutoConsoleCommand> RunCommand;
	TUniquePtr<FAutoConsoleCommand> GenerateFromFileCommand;
	TUniquePtr<FAutoConsoleCommand> HelpCommand;
	TUniquePtr<FAutoConsoleCommand> GenerateCommand;
	TUniquePtr<FAutoConsoleCommand> ErrorCommand;
};

IMPLEMENT_MODULE(FAutoBlueprintEditorModule, AutoBlueprintEditor)
