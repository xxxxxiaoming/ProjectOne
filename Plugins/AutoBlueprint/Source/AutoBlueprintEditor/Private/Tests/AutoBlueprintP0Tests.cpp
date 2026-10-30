#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintTypes.h"
#include "LuaEnv.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Scripting/EditorLuaRunner.h"
#include "UnLuaModule.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	bool HasCode(const FAutoBlueprintBuildResult& Result, const TCHAR* Code)
	{
		return Result.Diagnostics.ContainsByPredicate([Code](const FAutoBlueprintDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == Code;
		});
	}

	FString NewTestPackagePath()
	{
		return TEXT("/Game/UI/AutoBlueprintTests/WBP_Automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	bool RequireIdleEditor(FAutomationTestBase& Test)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP0InputValidationTest,
	"AutoBlueprint.P0.InputValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP0InputValidationTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditor(*this))
	{
		return false;
	}
	const TArray<FString> InvalidPaths =
	{
		TEXT(""),
		TEXT("/Game/UI/Other/WBP_P0"),
		TEXT("/Game/UI/AutoBlueprintTestsOther/WBP_P0"),
		TEXT("/Game/UI/AutoBlueprintTests/../WBP_P0"),
		TEXT("/Game/UI/AutoBlueprintTests/WBP_P0.uasset"),
		TEXT("/Game/UI/AutoBlueprintTests/WBP_P0.WBP_P0"),
		TEXT("/Engine/UI/WBP_P0")
	};
	for (const FString& Path : InvalidPaths)
	{
		FAutoBlueprintSmokeTestRequest Request;
		Request.PackagePath = Path;
		const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
		TestFalse(TEXT("Invalid path must fail"), Result.bSuccess);
		TestTrue(TEXT("Invalid path must have a stable diagnostic"), HasCode(Result, TEXT("INVALID_PACKAGE_PATH")));
		TestTrue(TEXT("Invalid path must not create an asset"), Result.CreatedAssets.IsEmpty());
		TestTrue(TEXT("Invalid path must not create a package"), Result.UnsavedPackages.IsEmpty());
		TestFalse(TEXT("Invalid path must not save"), Result.bSaved);
	}

	FAutoBlueprintSmokeTestRequest EmptyTextRequest;
	EmptyTextRequest.PackagePath = NewTestPackagePath();
	EmptyTextRequest.TitleText.Empty();
	const FAutoBlueprintBuildResult EmptyTextResult = UAutoBlueprintLibrary::BuildSmokeTestWidget(EmptyTextRequest);
	TestTrue(TEXT("Empty title must fail validation"), HasCode(EmptyTextResult, TEXT("INVALID_TEXT")));
	TestNull(TEXT("Validation must not allocate a package"), FindPackage(nullptr, *EmptyTextRequest.PackagePath));
	TestFalse(TEXT("Validation must not write a package"), FPackageName::DoesPackageExist(EmptyTextRequest.PackagePath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP0LoadedPackageConflictTest,
	"AutoBlueprint.P0.LoadedPackageConflict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP0LoadedPackageConflictTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditor(*this))
	{
		return false;
	}
	FAutoBlueprintSmokeTestRequest Request;
	Request.PackagePath = NewTestPackagePath();
	TStrongObjectPtr<UPackage> ExistingPackage(CreatePackage(*Request.PackagePath));
	TStrongObjectPtr<UObject> ExistingObject(NewObject<UObject>(ExistingPackage.Get(), TEXT("ExistingObject"), RF_Transient));
	ExistingPackage->SetDirtyFlag(true);

	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
	TestFalse(TEXT("Loaded package conflict must fail"), Result.bSuccess);
	TestTrue(TEXT("Conflict must be reported"), HasCode(Result, TEXT("ASSET_ALREADY_EXISTS")));
	TestTrue(TEXT("No replacement asset may be created"), Result.CreatedAssets.IsEmpty());
	TestTrue(TEXT("Original package identity must remain"), FindPackage(nullptr, *Request.PackagePath) == ExistingPackage.Get());
	TestTrue(TEXT("Original object identity must remain"),
		FindObject<UObject>(ExistingPackage.Get(), TEXT("ExistingObject")) == ExistingObject.Get());
	TestTrue(TEXT("Original dirty state must remain"), ExistingPackage->IsDirty());
	TestFalse(TEXT("No disk package may be written"), FPackageName::DoesPackageExist(Request.PackagePath));

	// Only the empty package owned by this test is cleaned; no asset is deleted.
	ExistingPackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP0LuaCleanupTest,
	"AutoBlueprint.P0.LuaErrorAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP0LuaCleanupTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditor(*this))
	{
		return false;
	}
	IUnLuaModule& UnLuaModule = IUnLuaModule::Get();
	const bool bWasActive = UnLuaModule.IsActive();
	TArray<lua_State*> OriginalStates;
	UnLua::FLuaEnv::GetAll().GenerateKeyArray(OriginalStates);
	FAutoBlueprintEditorLuaRunner Runner;

	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		const FAutoBlueprintLuaRunResult Result = Runner.Run(TEXT("p0_error.lua"), FString());
		TestFalse(TEXT("Lua exception must fail the task"), Result.bSuccess);
		TestEqual(TEXT("Lua failure code"), Result.Code, FString(TEXT("LUA_SCRIPT_FAILED")));
		TestTrue(TEXT("Original error must be preserved"), Result.Message.Contains(TEXT("AUTOBLUEPRINT_P0_EXPECTED_ERROR")));
		TestTrue(TEXT("Traceback must contain the script location"), Result.Message.Contains(TEXT("p0_error.lua:")));
		TestTrue(TEXT("Traceback must be included"), Result.Message.Contains(TEXT("stack traceback:")));
		TestEqual(TEXT("VM count must recover after each task"), UnLua::FLuaEnv::GetAll().Num(), OriginalStates.Num());
	}

	// This uses the real reflected USTRUCT/UFUNCTION/TArray bridge, without creating an asset.
	const FAutoBlueprintLuaRunResult RejectedBuild = Runner.Run(TEXT("p0_generate.lua"), TEXT("/Engine/UI/WBP_Forbidden"));
	TestFalse(TEXT("Failed C++ build must propagate through Lua"), RejectedBuild.bSuccess);
	TestTrue(TEXT("Structured C++ diagnostic must reach the host"), RejectedBuild.Message.Contains(TEXT("INVALID_PACKAGE_PATH")));
	TestEqual(TEXT("No task VM may remain"), UnLua::FLuaEnv::GetAll().Num(), OriginalStates.Num());
	TestEqual(TEXT("Game Lua activation must not change"), UnLuaModule.IsActive(), bWasActive);
	for (lua_State* OriginalState : OriginalStates)
	{
		TestTrue(TEXT("Pre-existing Lua environments must remain"), UnLua::FLuaEnv::GetAll().Contains(OriginalState));
	}
	return true;
}

#endif
