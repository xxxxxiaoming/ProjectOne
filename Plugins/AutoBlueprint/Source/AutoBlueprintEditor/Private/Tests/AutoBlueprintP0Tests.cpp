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
#include "WidgetBlueprint.h"

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
		TestFalse(TEXT("Invalid path must not create an asset"), Result.bAssetCreated);
		TestTrue(TEXT("Invalid path must not create a package"), Result.CreatedPackageName.IsEmpty());
		TestTrue(TEXT("Invalid path must not claim a validated target"), Result.TargetAssetPath.IsEmpty());
		TestFalse(TEXT("Invalid path must not compile"), Result.bCompiled);
		TestFalse(TEXT("Invalid path must not save"), Result.bSaved);
	}

	FAutoBlueprintSmokeTestRequest EmptyTextRequest;
	EmptyTextRequest.PackagePath = NewTestPackagePath();
	EmptyTextRequest.TitleText.Empty();
	const FAutoBlueprintBuildResult EmptyTextResult = UAutoBlueprintLibrary::BuildSmokeTestWidget(EmptyTextRequest);
	TestTrue(TEXT("Empty title must fail validation"), HasCode(EmptyTextResult, TEXT("INVALID_TEXT")));
	TestEqual(TEXT("A valid target path can be known before creation"), EmptyTextResult.TargetAssetPath,
		EmptyTextRequest.PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(EmptyTextRequest.PackagePath));
	TestFalse(TEXT("Empty title must not create an asset"), EmptyTextResult.bAssetCreated);
	TestTrue(TEXT("Empty title must not report a created package"), EmptyTextResult.CreatedPackageName.IsEmpty());
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
	if (!TestNotNull(TEXT("Test package must be created"), ExistingPackage.Get()))
	{
		return false;
	}
	// UObject is abstract in UE 5.3; use a concrete transient object for the conflict fixture.
	TStrongObjectPtr<UWidgetBlueprint> ExistingObject(NewObject<UWidgetBlueprint>(
		ExistingPackage.Get(), TEXT("ExistingObject"), RF_Transient));
	if (!TestNotNull(TEXT("Test Widget Blueprint must be created"), ExistingObject.Get()))
	{
		return false;
	}
	ExistingPackage->SetDirtyFlag(true);

	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
	TestFalse(TEXT("Loaded package conflict must fail"), Result.bSuccess);
	TestTrue(TEXT("Conflict must be reported"), HasCode(Result, TEXT("ASSET_ALREADY_EXISTS")));
	TestFalse(TEXT("No replacement asset may be created"), Result.bAssetCreated);
	TestTrue(TEXT("An existing package is not created by this request"), Result.CreatedPackageName.IsEmpty());
	TestFalse(TEXT("Conflict must not compile"), Result.bCompiled);
	TestFalse(TEXT("Conflict must not save"), Result.bSaved);
	TestTrue(TEXT("Original package identity must remain"), FindPackage(nullptr, *Request.PackagePath) == ExistingPackage.Get());
	TestTrue(TEXT("Original Widget Blueprint identity must remain"),
		FindObject<UWidgetBlueprint>(ExistingPackage.Get(), TEXT("ExistingObject")) == ExistingObject.Get());
	TestTrue(TEXT("Original dirty state must remain"), ExistingPackage->IsDirty());
	TestFalse(TEXT("No disk package may be written"), FPackageName::DoesPackageExist(Request.PackagePath));

	// Only clear the dirty flag of this test-owned package; no asset is saved or deleted.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP0SingleBuildResultTest,
	"AutoBlueprint.P0.SingleBuildResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP0SingleBuildResultTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditor(*this))
	{
		return false;
	}
	FAutoBlueprintSmokeTestRequest Request;
	Request.PackagePath = NewTestPackagePath();
	Request.bSave = false;

	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
	TestTrue(TEXT("An unsaved build can satisfy a request that does not ask for saving"), Result.bSuccess);
	TestTrue(TEXT("The Widget Blueprint must actually be created"), Result.bAssetCreated);
	TestTrue(TEXT("The generated Widget Blueprint must compile"), Result.bCompiled);
	TestFalse(TEXT("A compile without saving must not report a save"), Result.bSaved);
	TestEqual(TEXT("Report the one package actually created"), Result.CreatedPackageName, Request.PackagePath);
	const FString ExpectedObjectPath = Request.PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(Request.PackagePath);
	TestEqual(TEXT("Report the intended object path"), Result.TargetAssetPath, ExpectedObjectPath);
	TestFalse(TEXT("The build must not write a disk package"), FPackageName::DoesPackageExist(Request.PackagePath));

	TStrongObjectPtr<UPackage> Package(FindPackage(nullptr, *Request.PackagePath));
	if (!TestNotNull(TEXT("The generated package must exist in memory"), Package.Get()))
	{
		return false;
	}
	TStrongObjectPtr<UWidgetBlueprint> Blueprint(FindObject<UWidgetBlueprint>(
		Package.Get(), *FPackageName::GetLongPackageAssetName(Request.PackagePath)));
	TestNotNull(TEXT("The generated Widget Blueprint must exist"), Blueprint.Get());
	TestTrue(TEXT("The new package remains dirty until saved"), Package->IsDirty());

	// A later failed request must have its own outcome and must not claim the earlier package.
	const FAutoBlueprintBuildResult Conflict = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
	TestTrue(TEXT("The unsaved package blocks a second build"), HasCode(Conflict, TEXT("ASSET_ALREADY_EXISTS")));
	TestFalse(TEXT("The second build must fail"), Conflict.bSuccess);
	TestFalse(TEXT("The second build did not create an asset"), Conflict.bAssetCreated);
	TestTrue(TEXT("The second build did not create a package"), Conflict.CreatedPackageName.IsEmpty());
	TestFalse(TEXT("The second build did not compile"), Conflict.bCompiled);
	TestFalse(TEXT("The second build did not save"), Conflict.bSaved);
	TestTrue(TEXT("The earlier result remains successful"), Result.bSuccess);
	TestFalse(TEXT("Neither request may write a disk package"), FPackageName::DoesPackageExist(Request.PackagePath));
	TestTrue(TEXT("The conflict preserves the unsaved package"), Package->IsDirty());

	// Only this test's generated package is cleaned up; no user asset or file is removed.
	if (Blueprint.IsValid())
	{
		Blueprint->ClearFlags(RF_Standalone);
	}
	Package->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP0BatchNotImplementedTest,
	"AutoBlueprint.P0.BatchNotImplemented", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP0BatchNotImplementedTest::RunTest(const FString& Parameters)
{
	if (!RequireIdleEditor(*this))
	{
		return false;
	}
	FAutoBlueprintBatchRequest Request;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FAutoBlueprintSmokeTestRequest& Item = Request.Requests.AddDefaulted_GetRef();
		Item.PackagePath = NewTestPackagePath();
		Item.bSave = Index == 0;
	}
	const FAutoBlueprintBatchBuildResult Result = UAutoBlueprintLibrary::GenerateBatch(Request);
	TestTrue(TEXT("Batch API explicitly reports that it is not implemented"),
		Result.Diagnostics.ContainsByPredicate([](const FAutoBlueprintDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("NOT_IMPLEMENTED");
		}));
	if (!TestEqual(TEXT("Every request has a result slot"), Result.Results.Num(), Request.Requests.Num()))
	{
		return false;
	}
	for (int32 Index = 0; Index < Request.Requests.Num(); ++Index)
	{
		const FAutoBlueprintBuildResult& Item = Result.Results[Index];
		TestTrue(TEXT("Each unexecuted request has a diagnostic"), HasCode(Item, TEXT("NOT_IMPLEMENTED")));
		TestFalse(TEXT("The TODO API never claims success"), Item.bSuccess);
		TestFalse(TEXT("The TODO API never creates an asset"), Item.bAssetCreated);
		TestFalse(TEXT("The TODO API never compiles"), Item.bCompiled);
		TestFalse(TEXT("The TODO API never saves"), Item.bSaved);
		TestTrue(TEXT("The TODO API does not validate a target path"), Item.TargetAssetPath.IsEmpty());
		TestTrue(TEXT("The TODO API never reports a created package"), Item.CreatedPackageName.IsEmpty());
		TestNull(TEXT("The TODO API must not allocate a package"),
			FindPackage(nullptr, *Request.Requests[Index].PackagePath));
		TestFalse(TEXT("The TODO API must not write a package"),
			FPackageName::DoesPackageExist(Request.Requests[Index].PackagePath));
	}

	// Verify the public TODO API and nested USTRUCT/TArray result through the actual UnLua bridge.
	{
		UnLua::FLuaEnv Environment;
		Environment.SetName(TEXT("AutoBlueprint_BatchContractTest"));
		TestTrue(TEXT("Batch request and results must cross the UnLua reflection bridge"),
			Environment.DoString(TEXT(R"lua(
local request = UE.FAutoBlueprintBatchRequest()
request.Requests:Add(UE.FAutoBlueprintSmokeTestRequest())
request.Requests:Add(UE.FAutoBlueprintSmokeTestRequest())
local result = UE.UAutoBlueprintLibrary.GenerateBatch(request)
local diagnostics = result.Diagnostics:ToTable()
assert(#diagnostics == 1 and diagnostics[1].Code == "NOT_IMPLEMENTED")
local results = result.Results:ToTable()
assert(#results == 2)
for _, item in ipairs(results) do
    assert(item.bSuccess == false and item.bAssetCreated == false)
    assert(item.bCompiled == false and item.bSaved == false)
    assert(item.CreatedPackageName == "" and item.TargetAssetPath == "")
    local itemDiagnostics = item.Diagnostics:ToTable()
    assert(#itemDiagnostics == 1 and itemDiagnostics[1].Code == "NOT_IMPLEMENTED")
end
)lua"), TEXT("@AutoBlueprint/BatchContractTest")));
	}

	const FAutoBlueprintBatchBuildResult EmptyResult = UAutoBlueprintLibrary::GenerateBatch(FAutoBlueprintBatchRequest());
	TestTrue(TEXT("An empty batch has no item results"), EmptyResult.Results.IsEmpty());
	TestTrue(TEXT("An empty batch still reports NOT_IMPLEMENTED"),
		EmptyResult.Diagnostics.ContainsByPredicate([](const FAutoBlueprintDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("NOT_IMPLEMENTED");
		}));
	return true;
}

#endif
