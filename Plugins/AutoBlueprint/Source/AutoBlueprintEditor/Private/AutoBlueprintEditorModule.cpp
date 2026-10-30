#include "Modules/ModuleManager.h"

#include "AutoBlueprintLog.h"
#include "HAL/IConsoleManager.h"
#include "Scripting/EditorLuaRunner.h"

DEFINE_LOG_CATEGORY(LogAutoBlueprint);

class FAutoBlueprintEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Register the specialized Widget Blueprint compiler before exposing a generation entry.
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("UMGEditor"));
		GenerateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.P0.Generate"),
			TEXT("Generate the P0 fixture without Play. Usage: AutoBlueprint.P0.Generate /Game/UI/AutoBlueprintTests/WBP_P0_Review01"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::Generate));
		ErrorCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("AutoBlueprint.P0.ErrorProbe"),
			TEXT("Run an intentional Lua error without writing assets; expect LUA_SCRIPT_FAILED with a stack trace."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAutoBlueprintEditorModule::ProbeError));
	}

	virtual void ShutdownModule() override
	{
		ErrorCommand.Reset();
		GenerateCommand.Reset();
	}

private:
	void Generate(const TArray<FString>& Args)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.P0.Generate requires one new package path below /Game/UI/AutoBlueprintTests/."));
			return;
		}
		ReportRun(Runner.Run(TEXT("p0_generate.lua"), Args[0]));
	}

	void ProbeError(const TArray<FString>& Args)
	{
		if (!Args.IsEmpty())
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.P0.ErrorProbe takes no arguments."));
			return;
		}
		ReportRun(Runner.Run(TEXT("p0_error.lua"), FString()));
	}

	void ReportRun(const FAutoBlueprintLuaRunResult& Result) const
	{
		if (Result.bSuccess)
		{
			UE_LOG(LogAutoBlueprint, Log, TEXT("P0 Lua task completed; its tool environment has been released."));
		}
		else
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("%s: %s"), *Result.Code, *Result.Message);
		}
	}

	FAutoBlueprintEditorLuaRunner Runner;
	TUniquePtr<FAutoConsoleCommand> GenerateCommand;
	TUniquePtr<FAutoConsoleCommand> ErrorCommand;
};

IMPLEMENT_MODULE(FAutoBlueprintEditorModule, AutoBlueprintEditor)
