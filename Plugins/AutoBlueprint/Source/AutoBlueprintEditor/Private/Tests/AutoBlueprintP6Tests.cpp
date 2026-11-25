#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/CreateOnlyAssetWriter.h"
#include "Assets/ResolvedResources.h"
#include "Async/Async.h"
#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Layout/LayoutValidation.h"
#include "Layout/SmokeTestLayout.h"
#include "LuaEnv.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Scripting/EditorLuaRunner.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/UnrealTemplate.h"
#include "UnLuaModule.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WidgetBlueprint.h"

namespace AutoBlueprintP6Tests
{
	bool Idle(FAutomationTestBase& Test)
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

	bool HasCode(const TArray<FAutoBlueprintDiagnostic>& Diagnostics, const TCHAR* Code)
	{
		return Diagnostics.ContainsByPredicate([Code](const FAutoBlueprintDiagnostic& Item)
		{
			return Item.Code == Code;
		});
	}

	FString Serialize(const TSharedPtr<FJsonObject>& Input)
	{
		FString Json;

		FJsonSerializer::Serialize(Input.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	void ReleaseUnsaved(const FAutoBlueprintGenerationResult& Result)
	{
		TArray<FString> Paths;

		if (Result.Widget.bAssetCreated && !Result.Widget.bSaved)
		{
			Paths.Add(Result.Widget.TargetAssetPath);
		}

		for (const FAutoBlueprintResourceResult& Item : Result.Resources)
		{
			if (Item.bImported && !Item.bSaved)
			{
				Paths.Add(Item.TargetAssetPath);
			}
		}

		for (const FString& Path : Paths)
		{
			if (UObject* Object = FindObject<UObject>(nullptr, *Path))
			{
				Object->ClearFlags(RF_Standalone);
				Object->GetOutermost()->SetDirtyFlag(false);
			}
		}
	}

	struct FFixture
	{
		FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Base = TEXT("/Game/UI/AutoBlueprintTests/P6_") + Id;
		FString Source = TEXT("Content/UI/AutoBlueprintTests/P6_") + Id + TEXT(".png");
		FString File = TEXT("UIBlueprintLayouts/P6_") + Id + TEXT(".json");
		FString Blocker;
		TSharedPtr<FJsonObject> Input;

		bool Initialize(FAutomationTestBase& Test, bool bBlockSecondSave = false)
		{
			if (!Idle(Test))
			{
				return false;
			}

			FAutoBlueprintSmokeTestRequest Request;

			Request.PackagePath = Base + TEXT("/WBP_Task");

			const FString Json = AutoBlueprint::MakeSmokeTestLayoutJson(Request);
			TArray<uint8> Bytes;

			if (!Test.TestTrue(TEXT("Fixture JSON parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Input)))
			{
				return false;
			}

			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / FPaths::GetPath(Source)), true);
			if (!Test.TestTrue(TEXT("Fixture PNG written"), FBase64::Decode(TEXT("iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAYAAADED76LAAAAE0lEQVR4nGP4v0XjPz7MMDIUAAAyCraBzBAkmwAAAABJRU5ErkJggg=="), Bytes)
				&& FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectDir() / Source))))
			{
				return false;
			}

			const TSharedRef<FJsonObject> Resources = MakeShared<FJsonObject>();

			for (int32 Index = 0; Index < 2; ++Index)
			{
				const TSharedRef<FJsonObject> Resource = MakeShared<FJsonObject>();
				const FString Name = Index == 0 ? TEXT("a_first") : TEXT("b_second");
				const FString Directory = bBlockSecondSave && Index == 1 ? Base + TEXT("/Blocked") : Base;

				Resource->SetStringField(TEXT("source_file"), Source);
				Resource->SetStringField(TEXT("package_path"), Directory + TEXT("/T_") + Name);
				Resources->SetObjectField(Name, Resource);
			}

			Input->SetObjectField(TEXT("resources"), Resources);
			if (bBlockSecondSave)
			{
				Blocker = FPackageName::LongPackageNameToFilename(Base + TEXT("/Blocked"));
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Blocker), true);
				if (!Test.TestTrue(TEXT("Test-owned directory blocker written"), FFileHelper::SaveStringToFile(TEXT("P6 blocker"), *Blocker)))
				{
					return false;
				}
			}

			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / TEXT("UIBlueprintLayouts")), true);
			return Test.TestTrue(TEXT("Layout file written"), FFileHelper::SaveStringToFile(Serialize(Input), *(FPaths::ProjectDir() / File), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		}

		~FFixture()
		{
			IFileManager::Get().Delete(*(FPaths::ProjectDir() / Source), false, true);
			IFileManager::Get().Delete(*(FPaths::ProjectDir() / File), false, true);
			if (!Blocker.IsEmpty())
			{
				IFileManager::Get().Delete(*Blocker, false, true);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP6ScriptOutcomesTest,
	"AutoBlueprint.P6.ScriptOutcomesAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP6ScriptOutcomesTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP6Tests::Idle(*this))
	{
		return false;
	}

	TArray<lua_State*> OriginalStates;
	const bool bWasActive = IUnLuaModule::Get().IsActive();

	UnLua::FLuaEnv::GetAll().GenerateKeyArray(OriginalStates);

	for (int32 Case = 0; Case < 3; ++Case)
	{
		AutoBlueprintP6Tests::FFixture Fixture;

		if (!Fixture.Initialize(*this, Case != 0))
		{
			return false;
		}

		FAutoBlueprintLuaRunRequest Request;

		Request.ScriptPath = TEXT("tests/p6_outcomes.lua");
		Request.Arguments = { Fixture.File, Fixture.Base + TEXT("/WBP_NoSave"), Case == 2 ? TEXT("return") : TEXT("throw") };

		const FAutoBlueprintLuaRunResult Run = UAutoBlueprintLibrary::RunEditorScript(Request);

		ON_SCOPE_EXIT
		{
			for (const FAutoBlueprintScriptOperation& Entry : Run.Operations)
			{
				AutoBlueprintP6Tests::ReleaseUnsaved(Entry.Result);
			}
		};

		TestEqual(TEXT("Host status only describes script completion"), Run.bSuccess, Case == 2);
		if (Case != 2)
		{
			TestEqual(TEXT("Script diagnostic"), Run.Code, FString(TEXT("LUA_SCRIPT_FAILED")));
			TestTrue(TEXT("Error and traceback survive prior asset writes"), Run.Message.Contains(TEXT("AUTOBLUEPRINT_P6_AFTER_GENERATION"))
				&& Run.Message.Contains(TEXT("p6_outcomes.lua:")) && Run.Message.Contains(TEXT("stack traceback:")));
		}

		if (!TestEqual(TEXT("Five completed generation calls; no nested-script or validation entries"), Run.Operations.Num(), 5))
		{
			AddError(Run.Message);
			return false;
		}

		const TCHAR* Names[] = { TEXT("BuildFromLayoutFile"), TEXT("GenerateFromLayoutFile"), TEXT("BuildWidgetBlueprint"), TEXT("BuildWidgetBlueprint"), TEXT("GenerateFromLayout") };

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			TestEqual(TEXT("Journal completion order"), Run.Operations[Index].Operation, FString(Names[Index]));
		}

		TestTrue(TEXT("Legacy refusal retained"), AutoBlueprintP6Tests::HasCode(Run.Operations[0].Result.Widget.Diagnostics, TEXT("RESOURCE_TASK_REQUIRED")));

		const FAutoBlueprintGenerationResult& Task = Run.Operations[1].Result;
		const FAutoBlueprintBuildResult& Unsaved = Run.Operations[2].Result.Widget;

		if (!TestEqual(TEXT("Both texture outcomes retained"), Task.Resources.Num(), 2))
		{
			return false;
		}

		TestEqual(TEXT("Task success independent of Lua completion"), Task.bSuccess, Case == 0);
		TestEqual(TEXT("Widget saved only after all resources"), Task.Widget.bSaved, Case == 0);
		TestTrue(TEXT("First saved texture remains on disk"), Task.Resources[0].bSaved && FPackageName::DoesPackageExist(Task.Resources[0].CreatedPackageName));
		TestEqual(TEXT("Second save outcome retained"), Task.Resources[1].bSaved, Case == 0);
		TestEqual(TEXT("Widget disk outcome"), FPackageName::DoesPackageExist(Task.Widget.CreatedPackageName), Case == 0);
		TestTrue(TEXT("No-save smoke recorded exactly once"), Unsaved.bSuccess && Unsaved.bAssetCreated && Unsaved.bCompiled && !Unsaved.bSaved);
		TestFalse(TEXT("No-save never writes Widget"), FPackageName::DoesPackageExist(Unsaved.CreatedPackageName));
		TestFalse(TEXT("Invalid build remains failed"), Run.Operations[3].Result.bSuccess);
		TestFalse(TEXT("Invalid task remains failed"), Run.Operations[4].Result.bSuccess);

		if (Case != 0)
		{
			TestTrue(TEXT("Partial save diagnostic retained"), AutoBlueprintP6Tests::HasCode(Task.Resources[1].Diagnostics, TEXT("PACKAGE_SAVE_FAILED")));
			TestFalse(TEXT("Failed texture not on disk"), FPackageName::DoesPackageExist(Task.Resources[1].CreatedPackageName));
		}

		FString ReportJson;
		TSharedPtr<FJsonObject> Report;

		if (TestTrue(TEXT("Task report survives Lua failure"), FFileHelper::LoadFileToString(ReportJson, *Task.ReportFilePath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ReportJson), Report)))
		{
			TestEqual(TEXT("Report agrees with Widget result"), Report->GetObjectField(TEXT("widget"))->GetBoolField(TEXT("saved")), Task.Widget.bSaved);
			const TArray<TSharedPtr<FJsonValue>>& ReportResources = Report->GetArrayField(TEXT("resources"));

			if (TestEqual(TEXT("Report contains both resources"), ReportResources.Num(), 2))
			{
				TestTrue(TEXT("Report retains first successful save"), ReportResources[0]->AsObject()->GetBoolField(TEXT("saved")));
			}
		}

		Request.ScriptPath = TEXT("examples/hello.lua");
		Request.Arguments.Reset();

		const FAutoBlueprintLuaRunResult Next = UAutoBlueprintLibrary::RunEditorScript(Request);

		TestTrue(TEXT("Next task succeeds with no stale operations"), Next.bSuccess && Next.Operations.IsEmpty());
		TestEqual(TEXT("Previous returned snapshot is unchanged"), Run.Operations.Num(), 5);
		TestEqual(TEXT("No task VM retained"), UnLua::FLuaEnv::GetAll().Num(), OriginalStates.Num());
		TestEqual(TEXT("Game Lua activation unchanged"), IUnLuaModule::Get().IsActive(), bWasActive);

		for (lua_State* State : OriginalStates)
		{
			TestTrue(TEXT("Pre-existing VM survives"), UnLua::FLuaEnv::GetAll().Contains(State));
		}

		// Retrying with only a new Widget name must still protect already saved resources.
		Fixture.Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Fixture.Base + TEXT("/WBP_Retry"));

		FAutoBlueprintBuildOptions Options;

		Options.bWriteReport = false;

		const FAutoBlueprintGenerationResult Retry = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP6Tests::Serialize(Fixture.Input), Options);

		TestTrue(TEXT("Retry refuses resource conflict before new writes"), !Retry.bSuccess && !Retry.Widget.bAssetCreated && Retry.Resources.IsEmpty()
			&& AutoBlueprintP6Tests::HasCode(Retry.Widget.Diagnostics, TEXT("ASSET_ALREADY_EXISTS")));
		AddInfo(TEXT("P6 saved evidence retained: ") + Task.ReportFilePath);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP6AssetProtectionTest,
	"AutoBlueprint.P6.DirtyAssetsAndLateConflict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP6AssetProtectionTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP6Tests::Idle(*this))
	{
		return false;
	}

	for (int32 Case = 0; Case < 2; ++Case)
	{
		FAutoBlueprintSmokeTestRequest Request;

		Request.PackagePath = TEXT("/Game/UI/AutoBlueprintTests/WBP_P6_Protect_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Request.bSave = Case == 0;

		const FAutoBlueprintBuildResult Built = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);
		TStrongObjectPtr<UWidgetBlueprint> OwnedBlueprint(FindObject<UWidgetBlueprint>(nullptr, *Built.TargetAssetPath));
		UWidgetBlueprint* Blueprint = OwnedBlueprint.Get();

		if (!TestTrue(TEXT("Protection fixture built"), Built.bSuccess) || !TestNotNull(TEXT("Blueprint exists"), Blueprint))
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			// Only this test's own edit is cleared; saved packages are retained for inspection.
			Blueprint->GetOutermost()->SetDirtyFlag(false);
			if (!Built.bSaved)
			{
				Blueprint->ClearFlags(RF_Standalone);
			}
		};

		UTextBlock* Title = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TXT_Title")));

		if (!TestNotNull(TEXT("Editable title exists"), Title))
		{
			return false;
		}

		Title->SetText(FText::FromString(TEXT("P6 user edit must survive")));
		Blueprint->MarkPackageDirty();

		const FString Filename = FPackageName::LongPackageNameToFilename(Request.PackagePath, FPackageName::GetAssetPackageExtension());
		TArray<uint8> Before;
		TArray<uint8> After;

		if (Built.bSaved)
		{
			TestTrue(TEXT("Read existing file"), FFileHelper::LoadFileToArray(Before, *Filename));
		}

		const FAutoBlueprintBuildResult Conflict = UAutoBlueprintLibrary::BuildSmokeTestWidget(Request);

		TestTrue(TEXT("Conflict returns without modifying existing asset"), !Conflict.bAssetCreated && !Conflict.bSaved
			&& AutoBlueprintP6Tests::HasCode(Conflict.Diagnostics, TEXT("ASSET_ALREADY_EXISTS")));
		TestTrue(TEXT("Original object and dirty state preserved"), FindObject<UWidgetBlueprint>(nullptr, *Built.TargetAssetPath) == Blueprint && Blueprint->GetOutermost()->IsDirty());
		TestEqual(TEXT("User text preserved"), Title->GetText().ToString(), FString(TEXT("P6 user edit must survive")));

		if (Built.bSaved)
		{
			TestTrue(TEXT("Existing file not touched, not a serialization reproducibility check"), FFileHelper::LoadFileToArray(After, *Filename) && Before == After);
		}
		else
		{
			TestFalse(TEXT("Memory-only conflict writes no file"), IFileManager::Get().FileExists(*Filename));

			// Introduce a test-owned disk target after creation, directly before the writer.
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			if (!TestTrue(TEXT("Late-conflict sentinel written"), FFileHelper::SaveStringToFile(TEXT("P6 late target sentinel"), *Filename)))
			{
				return false;
			}

			ON_SCOPE_EXIT
			{
				IFileManager::Get().Delete(*Filename, false, true);
			};

			FString Code;
			FString Message;
			FString Sentinel;

			TestFalse(TEXT("Writer rechecks disk conflict"), AutoBlueprint::SaveCreatedAsset(*Blueprint, Code, Message));
			TestEqual(TEXT("Late conflict code"), Code, FString(TEXT("ASSET_ALREADY_EXISTS")));
			TestTrue(TEXT("Writer preserves late target bytes"), FFileHelper::LoadFileToString(Sentinel, *Filename) && Sentinel == TEXT("P6 late target sentinel"));
			TestTrue(TEXT("Late conflict leaves edited object dirty"), Blueprint->GetOutermost()->IsDirty());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP6ResourceLifetimeTest,
	"AutoBlueprint.P6.ResourceLifetimeAndSourceRecheck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP6ResourceLifetimeTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP6Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	AutoBlueprint::FLayoutSpec Layout;

	if (!TestTrue(TEXT("Real source passes preflight"), AutoBlueprint::ValidateLayoutJson(AutoBlueprintP6Tests::Serialize(Fixture.Input), Layout).bValid))
	{
		return false;
	}

	if (!TestTrue(TEXT("Replace test source after preflight"), FFileHelper::SaveStringToFile(TEXT("version https://git-lfs.github.com/spec/v1\noid sha256:test\nsize 1\n"),
		*(FPaths::ProjectDir() / Fixture.Source), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
	{
		return false;
	}

	{
		AutoBlueprint::FResolvedResources Resources;
		FAutoBlueprintGenerationResult Result;

		TestFalse(TEXT("Importer revalidates changed bytes"), Resources.Resolve(Layout, Result, false));
		if (!TestEqual(TEXT("Both resource plans retained"), Result.Resources.Num(), 2))
		{
			return false;
		}

		TestTrue(TEXT("First source fails with LFS diagnostic"), Result.Resources[0].bAttempted && !Result.Resources[0].bImported
			&& AutoBlueprintP6Tests::HasCode(Result.Resources[0].Diagnostics, TEXT("LFS_SOURCE_NOT_MATERIALIZED")));
		TestTrue(TEXT("Changed source diagnostic locates input"), Result.Resources[0].Diagnostics.ContainsByPredicate([](const FAutoBlueprintDiagnostic& Item)
		{
			return Item.Code == TEXT("LFS_SOURCE_NOT_MATERIALIZED") && Item.JsonPath == TEXT("$.resources.a_first.source_file");
		}));
		TestFalse(TEXT("Later source not attempted"), Result.Resources[1].bAttempted);

		for (const FAutoBlueprintResourceResult& Item : Result.Resources)
		{
			TestTrue(TEXT("No package claimed created"), Item.CreatedPackageName.IsEmpty());
			TestNull(TEXT("No texture package in memory"), FindPackage(nullptr, *FPackageName::ObjectPathToPackageName(Item.TargetAssetPath)));
		}
	}

	TWeakObjectPtr<UTexture2D> WeakTexture;
	TWeakObjectPtr<UFont> WeakFont;

	{
		AutoBlueprint::FResolvedResources Resources;
		AutoBlueprint::FLayoutSpec Cached;
		FAutoBlueprintGenerationResult Result;
		UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), FName(*(TEXT("P6_Texture_") + Fixture.Id)));
		UFont* Font = NewObject<UFont>(GetTransientPackage(), FName(*(TEXT("P6_Font_") + Fixture.Id)));
		AutoBlueprint::FLayoutResource Resource;
		TSharedPtr<AutoBlueprint::FLayoutValue> FontValue = MakeShared<AutoBlueprint::FLayoutValue>();

		WeakTexture = Texture;
		WeakFont = Font;
		Resource.Alias = TEXT("existing");
		Resource.ObjectPath = Texture->GetPathName();
		FontValue->Kind = AutoBlueprint::ELayoutValueKind::String;
		FontValue->String = Font->GetPathName();
		Cached.Resources.Add(Resource);
		Cached.Root.Properties.Fields.Add(TEXT("font"), FontValue);

		if (!TestTrue(TEXT("Transient existing resources resolved"), Resources.Resolve(Cached, Result, false)))
		{
			return false;
		}

		// Keep normal editor asset flags; only these transient, non-Standalone fixtures may be collected.
		Texture = nullptr;
		Font = nullptr;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		TestTrue(TEXT("Task cache strongly retains texture across GC"), WeakTexture.IsValid() && Resources.GetTexture(TEXT("existing")) == WeakTexture.Get());
		TestTrue(TEXT("Task cache strongly retains font across GC"), WeakFont.IsValid() && Resources.GetFont(FontValue->String) == WeakFont.Get());
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Released task cache does not leak texture"), WeakTexture.IsValid());
	TestFalse(TEXT("Released task cache does not leak font"), WeakFont.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP6ContextTest,
	"AutoBlueprint.P6.ContextAndInitializationRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP6ContextTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP6Tests::Idle(*this))
	{
		return false;
	}

	const int32 EnvironmentCount = UnLua::FLuaEnv::GetAll().Num();
	FAutoBlueprintLuaRunRequest Request;

	Request.ScriptPath = TEXT("examples/hello.lua");

	FAutoBlueprintEditorLuaRunner Runner;

	Runner.bSimulateUnavailableStateForTest = true;

	const FAutoBlueprintLuaRunResult Unavailable = Runner.Run(Request);

	TestEqual(TEXT("Unavailable state is diagnosed"), Unavailable.Code, FString(TEXT("LUA_ENV_INIT_FAILED")));
	TestTrue(TEXT("No operations before Lua initializes"), Unavailable.Operations.IsEmpty());
	TestEqual(TEXT("Initialization failure releases environment"), UnLua::FLuaEnv::GetAll().Num(), EnvironmentCount);

	Runner.bSimulateUnavailableStateForTest = false;
	TestTrue(TEXT("Same runner recovers after unavailable state"), Runner.Run(Request).bSuccess);
	TestEqual(TEXT("Successful recovery also releases environment"), UnLua::FLuaEnv::GetAll().Num(), EnvironmentCount);

	TFuture<FAutoBlueprintGenerationResult> Worker = Async(EAsyncExecution::Thread, []()
	{
		return UAutoBlueprintLibrary::GenerateFromLayout(TEXT("{}"), FAutoBlueprintBuildOptions());
	});

	const FAutoBlueprintGenerationResult WrongThread = Worker.Get();

	TestTrue(TEXT("Worker rejected before touching subsystem/assets"), AutoBlueprintP6Tests::HasCode(WrongThread.Diagnostics, TEXT("WRONG_THREAD"))
		&& !WrongThread.Widget.bAssetCreated && WrongThread.Resources.IsEmpty());

	{
		TGuardValue<bool> SavingGuard(GIsSavingPackage, true);

		const FAutoBlueprintLuaRunResult Busy = UAutoBlueprintLibrary::RunEditorScript(Request);
		const FAutoBlueprintGenerationResult Blocked = UAutoBlueprintLibrary::GenerateFromLayout(TEXT("{}"), FAutoBlueprintBuildOptions());

		TestEqual(TEXT("Saving context rejects tools"), Busy.Code, FString(TEXT("EDITOR_BUSY")));
		TestTrue(TEXT("Saving context rejects asset generation"), AutoBlueprintP6Tests::HasCode(Blocked.Diagnostics, TEXT("EDITOR_BUSY"))
			&& !Blocked.Widget.bAssetCreated && Blocked.Resources.IsEmpty());
	}

	TStrongObjectPtr<UAutoBlueprintSubsystem> Inactive(NewObject<UAutoBlueprintSubsystem>(GEditor));

	TestEqual(TEXT("Uninitialized subsystem rejects scripts"), Inactive->RunEditorScript(Request).Code, FString(TEXT("SUBSYSTEM_UNAVAILABLE")));
	TestTrue(TEXT("Public subsystem remains usable after rejected contexts"), UAutoBlueprintLibrary::RunEditorScript(Request).bSuccess);
	TestEqual(TEXT("All rejected contexts leave VM count unchanged"), UnLua::FLuaEnv::GetAll().Num(), EnvironmentCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP6ReflectionTest,
	"AutoBlueprint.P6.ReflectedOperationResults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP6ReflectionTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP6Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	const int32 OriginalCount = UnLua::FLuaEnv::GetAll().Num();
	const bool bWasActive = IUnLuaModule::Get().IsActive();
	const FString SmokePackage = Fixture.Base + TEXT("/WBP_ReflectionNoSave");

	ON_SCOPE_EXIT
	{
		FAutoBlueprintGenerationResult Owned;

		Owned.Widget.TargetAssetPath = SmokePackage + TEXT(".WBP_ReflectionNoSave");
		Owned.Widget.bAssetCreated = FindObject<UWidgetBlueprint>(nullptr, *Owned.Widget.TargetAssetPath) != nullptr;
		AutoBlueprintP6Tests::ReleaseUnsaved(Owned);
	};

	{
		// Independent caller VM stays alive while the tool creates and releases its own VM.
		UnLua::FLuaEnv Caller;
		const FString Script = FString::Printf(TEXT(R"lua(
local request = UE.FAutoBlueprintLuaRunRequest()
request.ScriptPath = "tests/p6_outcomes.lua"
request.Arguments:Add("%s")
request.Arguments:Add("%s")
request.Arguments:Add("throw")
local result = UE.UAutoBlueprintLibrary.RunEditorScript(request)
assert(not result.bSuccess and result.Code == "LUA_SCRIPT_FAILED")
local entries = result.Operations:ToTable()
assert(#entries == 5)
assert(entries[1].Operation == "BuildFromLayoutFile")
assert(not entries[1].Result.Widget.bAssetCreated)
assert(entries[2].Result.Widget.bSaved and entries[2].Result.bSuccess)
local resources = entries[2].Result.Resources:ToTable()
assert(#resources == 2 and resources[1].bSaved and resources[2].bSaved)
assert(entries[2].Result.ReportFilePath ~= "")
assert(entries[3].Result.Widget.bCompiled and not entries[3].Result.Widget.bSaved)
assert(not entries[4].Result.bSuccess and not entries[5].Result.bSuccess)
request.ScriptPath = "examples/hello.lua"
request.Arguments:Clear()
local nextResult = UE.UAutoBlueprintLibrary.RunEditorScript(request)
assert(nextResult.bSuccess and #nextResult.Operations:ToTable() == 0)
assert(#result.Operations:ToTable() == 5 and entries[2].Result.Widget.bSaved)
)lua"), *Fixture.File, *SmokePackage);

		TestTrue(TEXT("Real Lua reads nonempty nested operation/resource results after tool VM release"), Caller.DoString(*Script, TEXT("@AutoBlueprint/P6Reflection")));
		TestEqual(TEXT("Only the independent caller remains"), UnLua::FLuaEnv::GetAll().Num(), OriginalCount + 1);
	}

	TestEqual(TEXT("Caller also releases its VM"), UnLua::FLuaEnv::GetAll().Num(), OriginalCount);
	TestEqual(TEXT("No game activation change"), IUnLuaModule::Get().IsActive(), bWasActive);
	return true;
}

#endif
