#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintLuaTypes.h"
#include "LuaEnv.h"
#include "Misc/Guid.h"
#include "Scripting/LuaScriptSource.h"
#include "UnLuaModule.h"

namespace
{
	// Distinct name from P0 helpers, including when UE combines test files in a Unity build.
	bool RequireIdleEditorForP1(FAutomationTestBase& Test)
	{
		FString Code;
		FString Message;
		if (!AutoBlueprint::CheckEditorContext(Code, Message))
		{
			Test.AddError(Code + TEXT(": ") + Message);
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP1InputValidationTest,
	"AutoBlueprint.P1.InputValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP1InputValidationTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditorForP1(*this))
	{
		return false;
	}
	const int32 OriginalEnvCount = UnLua::FLuaEnv::GetAll().Num();
	const TArray<FString> InvalidPaths =
	{
		TEXT(""), TEXT("../p0_error.lua"), TEXT("/p0_error.lua"), TEXT("C:/p0_error.lua"),
		TEXT("examples\\hello.lua"), TEXT("./p0_error.lua"), TEXT("examples//hello.lua"),
		TEXT("examples/../p0_error.lua"), TEXT("hello.txt"), TEXT("hello.LUA"), TEXT(".lua"),
		TEXT("examples/.lua"), TEXT("examples/hello.lua/extra")
	};
	for (const FString& Path : InvalidPaths)
	{
		FAutoBlueprintLuaRunRequest Request;
		Request.ScriptPath = Path;
		const FAutoBlueprintLuaRunResult Result = UAutoBlueprintLibrary::RunEditorScript(Request);
		TestFalse(TEXT("Invalid entry paths must fail"), Result.bSuccess);
		TestEqual(TEXT("Entry path diagnostic"), Result.Code, FString(TEXT("INVALID_SCRIPT_PATH")));
		TestTrue(TEXT("No invalid entry path may resolve"), Result.ScriptPath.IsEmpty());
		TestEqual(TEXT("Path rejection must not retain a VM"), UnLua::FLuaEnv::GetAll().Num(), OriginalEnvCount);
	}
	FAutoBlueprintLuaRunRequest Missing;
	Missing.ScriptPath = TEXT("missing_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".lua");
	TestEqual(TEXT("A missing file has its own diagnostic"),
		UAutoBlueprintLibrary::RunEditorScript(Missing).Code, FString(TEXT("LUA_SCRIPT_NOT_FOUND")));
	FAutoBlueprintLuaRunRequest TooMany;
	TooMany.ScriptPath = TEXT("examples/hello.lua");
	TooMany.Arguments.SetNum(33);
	TestEqual(TEXT("Argument count is bounded"),
		UAutoBlueprintLibrary::RunEditorScript(TooMany).Code, FString(TEXT("INVALID_ARGUMENTS")));
	FAutoBlueprintLuaRunRequest TooLong;
	TooLong.ScriptPath = TEXT("examples/hello.lua");
	TooLong.Arguments.Add(FString::ChrN(4097, TEXT('a')));
	TestEqual(TEXT("Argument length is bounded"),
		UAutoBlueprintLibrary::RunEditorScript(TooLong).Code, FString(TEXT("INVALID_ARGUMENTS")));

	FString RelativePath;
	TestTrue(TEXT("Dotted modules map to tool files"), AutoBlueprint::MakeLuaModulePath(TEXT("examples.greeter"), RelativePath));
	TestEqual(TEXT("Module path mapping"), RelativePath, FString(TEXT("examples/greeter.lua")));
	const TArray<FString> InvalidModules =
	{
		TEXT("../outside"), TEXT("examples..greeter"), TEXT(".examples"),
		TEXT("examples."), TEXT("examples/greeter"), TEXT("examples.greeter.lua")
	};
	for (const FString& Name : InvalidModules)
	{
		TestFalse(TEXT("Modules cannot traverse directories"), AutoBlueprint::MakeLuaModulePath(Name, RelativePath));
		TestTrue(TEXT("Rejected modules have no relative path"), RelativePath.IsEmpty());
	}
	TestEqual(TEXT("Rejected input must not retain a VM"), UnLua::FLuaEnv::GetAll().Num(), OriginalEnvCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP1HostLifecycleTest,
	"AutoBlueprint.P1.HostLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP1HostLifecycleTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditorForP1(*this))
	{
		return false;
	}
	IUnLuaModule& UnLuaModule = IUnLuaModule::Get();
	const bool bWasActive = UnLuaModule.IsActive();
	TArray<lua_State*> OriginalStates;
	UnLua::FLuaEnv::GetAll().GenerateKeyArray(OriginalStates);
	auto CheckCleanup = [this, &OriginalStates, &UnLuaModule, bWasActive]()
	{
		TestEqual(TEXT("Every task releases its VM"), UnLua::FLuaEnv::GetAll().Num(), OriginalStates.Num());
		TestEqual(TEXT("Game Lua activation is unchanged"), UnLuaModule.IsActive(), bWasActive);
		for (lua_State* State : OriginalStates)
		{
			TestTrue(TEXT("Pre-existing environments survive"), UnLua::FLuaEnv::GetAll().Contains(State));
		}
	};
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		FAutoBlueprintLuaRunRequest Request;
		Request.ScriptPath = TEXT("tests/host_contract.lua");
		Request.Arguments = { TEXT("\"; AutoBlueprintInjected = true; --"), TEXT("中文路径 with spaces") };
		const FAutoBlueprintLuaRunResult Result = UAutoBlueprintLibrary::RunEditorScript(Request);
		TestTrue(TEXT("Modules, arguments, caches and reflected reentry guard work"), Result.bSuccess);
		if (!Result.bSuccess)
		{
			AddError(Result.Code + TEXT(": ") + Result.Message);
		}
		CheckCleanup();

		Request.ScriptPath = TEXT("tests/module_failure.lua");
		Request.Arguments.Reset();
		const FAutoBlueprintLuaRunResult Failure = UAutoBlueprintLibrary::RunEditorScript(Request);
		TestEqual(TEXT("Uncaught module error fails the task"), Failure.Code, FString(TEXT("LUA_SCRIPT_FAILED")));
		TestTrue(TEXT("Original error survives"), Failure.Message.Contains(TEXT("AUTOBLUEPRINT_P1_MODULE_ERROR")));
		TestTrue(TEXT("Source location survives"), Failure.Message.Contains(TEXT("module_failure.lua:")));
		TestTrue(TEXT("Traceback survives"), Failure.Message.Contains(TEXT("stack traceback:")));
		CheckCleanup();

		Request.ScriptPath = TEXT("examples/hello.lua");
		TestTrue(TEXT("A later task succeeds after failure"), UAutoBlueprintLibrary::RunEditorScript(Request).bSuccess);
		CheckCleanup();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP1FileEntryBoundaryTest,
	"AutoBlueprint.P1.FileEntryBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP1FileEntryBoundaryTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditorForP1(*this))
	{
		return false;
	}
	FAutoBlueprintLuaRunRequest Request;
	Request.ScriptPath = TEXT("generate_from_file.lua");
	Request.Arguments.Add(TEXT("UIBlueprintLayouts/NotImplemented.layout.json"));
	const FAutoBlueprintLuaRunResult Result = UAutoBlueprintLibrary::RunEditorScript(Request);
	TestFalse(TEXT("P1 must not pretend to generate a layout"), Result.bSuccess);
	TestEqual(TEXT("Lua API failure reaches the host"), Result.Code, FString(TEXT("LUA_SCRIPT_FAILED")));
	TestTrue(TEXT("Missing capability is explicit"), Result.Message.Contains(TEXT("NOT_IMPLEMENTED")));
	TestTrue(TEXT("Entry script is identifiable"), Result.Message.Contains(TEXT("generate_from_file.lua:")));

	Request.Arguments.Reset();
	TestTrue(TEXT("File entry requires one path"),
		UAutoBlueprintLibrary::RunEditorScript(Request).Message.Contains(TEXT("INVALID_ARGUMENTS")));
	Request.ScriptPath = TEXT("examples/greeter.lua");
	TestTrue(TEXT("Helper module is not a runnable tool"),
		UAutoBlueprintLibrary::RunEditorScript(Request).Message.Contains(TEXT("Tool script must return a table containing run(arguments)")));
	return true;
}

#endif
