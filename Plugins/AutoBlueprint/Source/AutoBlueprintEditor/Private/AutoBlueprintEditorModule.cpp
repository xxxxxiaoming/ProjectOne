#include "Modules/ModuleManager.h"

#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintLog.h"
#include "AutoBlueprintLuaTypes.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY(LogAutoBlueprint);

namespace
{
	// UE has already split on whitespace. Rejoin quoted tokens with one space;
	// the original whitespace cannot be recovered at this stage.
	bool ParseAutoBlueprintRunArguments(const TArray<FString>& Tokens, TArray<FString>& OutArguments, FString& OutError)
	{
		OutArguments.Reset();
		OutError.Reset();
		TArray<FString> ParsedArguments;
		FString Argument;
		bool bInQuotes = false;
		for (const FString& Token : Tokens)
		{
			int32 StartIndex = 0;
			if (bInQuotes)
			{
				Argument += TEXT(' ');
			}
			else
			{
				Argument.Reset();
				if (Token.StartsWith(TEXT("\"")))
				{
					bInQuotes = true;
					StartIndex = 1;
				}
			}

			for (int32 Index = StartIndex; Index < Token.Len(); ++Index)
			{
				const TCHAR Character = Token[Index];
				if (bInQuotes && Character == TEXT('\\') && Index + 1 < Token.Len()
					&& (Token[Index + 1] == TEXT('"') || Token[Index + 1] == TEXT('\\')))
				{
					Argument += Token[++Index];
				}
				else if (Character == TEXT('"'))
				{
					if (!bInQuotes || Index != Token.Len() - 1)
					{
						OutError = TEXT("Double quotes must wrap the entire argument; escape literal quotes inside with \\\".");
						return false;
					}
					bInQuotes = false;
				}
				else
				{
					Argument += Character;
				}
			}
			if (!bInQuotes)
			{
				ParsedArguments.Add(Argument);
			}
		}
		if (bInQuotes)
		{
			OutError = TEXT("Unclosed double quote in AutoBlueprint.Run arguments.");
			return false;
		}
		OutArguments = MoveTemp(ParsedArguments);
		return true;
	}
}

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
		TArray<FString> ParsedArgs;
		FString ParseError;
		if (!ParseAutoBlueprintRunArguments(Args, ParsedArgs, ParseError))
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: %s"), *ParseError);
			return;
		}
		if (ParsedArgs.IsEmpty())
		{
			UE_LOG(LogAutoBlueprint, Error, TEXT("INVALID_ARGUMENTS: AutoBlueprint.Run <relative_script.lua> [arguments...]"));
			return;
		}
		FAutoBlueprintLuaRunRequest Request;
		Request.ScriptPath = ParsedArgs[0];
		for (int32 Index = 1; Index < ParsedArgs.Num(); ++Index)
		{
			Request.Arguments.Add(ParsedArgs[Index]);
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
		UE_LOG(LogAutoBlueprint, Log, TEXT("Run arguments: double quotes wrap whole arguments; empty quotes preserve an empty argument. Whitespace inside quotes becomes one space; inside quotes, \\\" and \\\\ escape quotes and backslashes. Other commands retain their existing argument rules."));
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

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP1ConsoleArgumentsTest,
	"AutoBlueprint.P1.ConsoleArguments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP1ConsoleArgumentsTest::RunTest(const FString& Parameters)
{
	auto CheckArguments = [this](const TCHAR* Input, const TArray<FString>& Expected)
	{
		// Match ConsoleManager's real tokenization, including lost whitespace.
		TArray<FString> Tokens;
		FString(Input).ParseIntoArrayWS(Tokens);
		TArray<FString> Actual;
		FString Error;
		if (!TestTrue(Input, ParseAutoBlueprintRunArguments(Tokens, Actual, Error)))
		{
			AddError(Error);
			return;
		}
		TestTrue(TEXT("Successful parsing clears the diagnostic"), Error.IsEmpty());
		TestEqual(TEXT("Parsed argument count"), Actual.Num(), Expected.Num());
		for (int32 Index = 0; Index < FMath::Min(Actual.Num(), Expected.Num()); ++Index)
		{
			TestEqual(FString::Printf(TEXT("Argument %d for %s"), Index, Input), Actual[Index], Expected[Index]);
		}
	};

	CheckArguments(TEXT(""), {});
	CheckArguments(TEXT("examples/hello.lua hello_world 中文参数"),
		{ TEXT("examples/hello.lua"), TEXT("hello_world"), TEXT("中文参数") });
	CheckArguments(TEXT(R"("examples/hello.lua" "hello world" "中文 参数")"),
		{ TEXT("examples/hello.lua"), TEXT("hello world"), TEXT("中文 参数") });
	CheckArguments(TEXT("examples/hello.lua \"hello   \tworld\""),
		{ TEXT("examples/hello.lua"), TEXT("hello world") });
	CheckArguments(TEXT(R"(examples/hello.lua before "" "" after)"),
		{ TEXT("examples/hello.lua"), TEXT("before"), TEXT(""), TEXT(""), TEXT("after") });
	CheckArguments(TEXT(R"(examples/hello.lua "say \"hello world\"")"),
		{ TEXT("examples/hello.lua"), TEXT("say \"hello world\"") });
	CheckArguments(TEXT(R"(examples/hello.lua C:\Temp\file "C:\Temp\file" "C:\\Temp\\file" "tail\\")"),
		{ TEXT("examples/hello.lua"), TEXT(R"(C:\Temp\file)"), TEXT(R"(C:\Temp\file)"),
			TEXT(R"(C:\Temp\file)"), TEXT("tail\\") });
	CheckArguments(TEXT(R"(examples/hello.lua "literal\t")"),
		{ TEXT("examples/hello.lua"), TEXT(R"(literal\t)") });

	const TCHAR* InvalidInputs[] =
	{
		TEXT(R"(examples/hello.lua "hello world)"),
		TEXT(R"(examples/hello.lua hello"world)"),
		TEXT(R"(examples/hello.lua "hello"tail)"),
		TEXT(R"(examples/hello.lua """ )"),
		TEXT(R"(examples/hello.lua "say \"hello\")")
	};
	for (const TCHAR* Input : InvalidInputs)
	{
		TArray<FString> Tokens;
		FString(Input).ParseIntoArrayWS(Tokens);
		TArray<FString> Actual = { TEXT("stale") };
		FString Error;
		TestFalse(Input, ParseAutoBlueprintRunArguments(Tokens, Actual, Error));
		TestTrue(TEXT("Malformed quotes have a diagnostic"), !Error.IsEmpty());
		TestTrue(TEXT("Malformed quotes return no partial arguments"), Actual.IsEmpty());
	}
	return true;
}

#endif
