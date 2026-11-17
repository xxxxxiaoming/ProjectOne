#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/WidgetBlueprintWriter.h"
#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "Widgets/Layout/Anchors.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SubclassOf.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace AutoBlueprintP3Tests
{
	bool RequireEditor(FAutomationTestBase& Test)
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

	FString NewPackage()
	{
		return TEXT("/Game/UI/AutoBlueprintTests/WBP_P3_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	TSharedPtr<FJsonObject> Fixture(const FString& PackagePath)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
		FString Json;
		TSharedPtr<FJsonObject> Object;

		if (!Plugin.IsValid() || !FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Examples/p3_minimal.layout.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
		{
			return nullptr;
		}

		Object->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), PackagePath);
		return Object;
	}

	FString Serialize(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;

		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	bool HasCode(const FAutoBlueprintBuildResult& Result, const TCHAR* Code)
	{
		return Result.Diagnostics.ContainsByPredicate([Code](const FAutoBlueprintDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == Code;
		});
	}

	void ReleaseUnsaved(const FAutoBlueprintBuildResult& Result)
	{
		// Only touch the unique package actually created by this test's request.
		if (!Result.bSaved && !Result.CreatedPackageName.IsEmpty())
		{
			if (UWidgetBlueprint* Blueprint = FindObject<UWidgetBlueprint>(nullptr, *Result.TargetAssetPath))
			{
				Blueprint->ClearFlags(RF_Standalone);
				Blueprint->GetOutermost()->SetDirtyFlag(false);
			}
		}
	}

	bool InspectTree(FAutomationTestBase& Test, UWidgetBlueprint* Blueprint)
	{
		if (!Test.TestNotNull(TEXT("Widget Blueprint exists"), Blueprint)
			|| !Test.TestNotNull(TEXT("Template tree exists"), static_cast<UWidgetTree*>(Blueprint->WidgetTree))
			|| !Test.TestNotNull(TEXT("Compiled class exists"), static_cast<UClass*>(Blueprint->GeneratedClass)))
		{
			return false;
		}

		UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
		UTextBlock* Title = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TXT_Title")));
		UButton* Button = Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("BTN_Confirm")));
		UTextBlock* Label = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TXT_Confirm")));

		if (!Test.TestNotNull(TEXT("Canvas root"), Root) || !Test.TestNotNull(TEXT("Title text"), Title)
			|| !Test.TestNotNull(TEXT("Button"), Button) || !Test.TestNotNull(TEXT("Button label"), Label))
		{
			return false;
		}

		Test.TestEqual(TEXT("Root name"), Root->GetFName(), FName(TEXT("RootCanvas")));
		Test.TestEqual(TEXT("Root child count"), Root->GetChildrenCount(), 2);
		Test.TestTrue(TEXT("Configured child order"), Root->GetChildAt(0) == Title && Root->GetChildAt(1) == Button);
		Test.TestTrue(TEXT("Button owns label"), Button->GetContent() == Label);
		Test.TestEqual(TEXT("Title is configuration text"), Title->GetText().ToString(), FString(TEXT("P3 JSON layout")));
		Test.TestEqual(TEXT("Font size"), Title->GetFont().Size, 32);
		Test.TestTrue(TEXT("Linear text color"), Title->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor(0.2f, 0.8f, 1.0f, 1.0f)));
		Test.TestEqual(TEXT("Title visibility"), Title->GetVisibility(), ESlateVisibility::HitTestInvisible);
		Test.TestTrue(TEXT("Explicit variable flags"), !Root->bIsVariable && Title->bIsVariable && Button->bIsVariable && !Label->bIsVariable);
		Test.TestNotNull(TEXT("Generated title variable"), FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("TXT_Title")));
		Test.TestNotNull(TEXT("Generated button variable"), FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("BTN_Confirm")));
		Test.TestNull(TEXT("Non-variable label has no generated member"), FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("TXT_Confirm")));
		Test.TestFalse(TEXT("Button focus configuration"), Button->GetIsFocusable());

		UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot);
		UButtonSlot* LabelSlot = Cast<UButtonSlot>(Label->Slot);

		if (!Test.TestNotNull(TEXT("Canvas child slot"), ButtonSlot) || !Test.TestNotNull(TEXT("Button child slot"), LabelSlot))
		{
			return false;
		}

		Test.TestTrue(TEXT("Stretched bottom anchors"), ButtonSlot->GetAnchors().Minimum == FVector2D(0, 1)
			&& ButtonSlot->GetAnchors().Maximum == FVector2D(1, 1));
		Test.TestTrue(TEXT("Canvas offsets"), ButtonSlot->GetOffsets() == FMargin(64, -80, 64, 64));
		Test.TestTrue(TEXT("Canvas alignment"), ButtonSlot->GetAlignment() == FVector2D(0, 1));
		Test.TestEqual(TEXT("Canvas z-order"), ButtonSlot->GetZOrder(), 2);
		Test.TestTrue(TEXT("Button padding"), LabelSlot->GetPadding() == FMargin(12, 8, 12, 8));
		Test.TestTrue(TEXT("Button alignment"), LabelSlot->GetHorizontalAlignment() == HAlign_Center
			&& LabelSlot->GetVerticalAlignment() == VAlign_Center);

		const UUserWidget* Defaults = Cast<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject());

		if (!Test.TestNotNull(TEXT("Native UserWidget defaults"), Defaults))
		{
			return false;
		}

		Test.TestTrue(TEXT("Designer custom size persists"), Defaults->DesignTimeSize == FVector2D(960, 640)
			&& Defaults->DesignSizeMode == EDesignPreviewSizeMode::Custom);
		Test.TestTrue(TEXT("Native runtime parent"), Blueprint->ParentClass == UUserWidget::StaticClass());
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP3TreeTest,
	"AutoBlueprint.P3.TreeAndVariables", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP3TreeTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP3Tests::RequireEditor(*this))
	{
		return false;
	}

	const FString PackagePath = AutoBlueprintP3Tests::NewPackage();
	const TSharedPtr<FJsonObject> Input = AutoBlueprintP3Tests::Fixture(PackagePath);
	FAutoBlueprintBuildOptions Options;

	if (!TestTrue(TEXT("Fixture readable"), Input.IsValid()))
	{
		return false;
	}

	Options.bSave = false;
	Options.bWriteReport = false;

	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildWidgetBlueprint(AutoBlueprintP3Tests::Serialize(Input), Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Result);
	};

	TestTrue(TEXT("Unsaved build succeeds and compiles"), Result.bSuccess && Result.bAssetCreated && Result.bCompiled);
	TestFalse(TEXT("No save requested"), Result.bSaved);
	TestTrue(TEXT("No report requested"), Result.ReportFilePath.IsEmpty());
	TestEqual(TEXT("Exactly one created package"), Result.CreatedPackageName, PackagePath);
	TestFalse(TEXT("No asset on disk"), FPackageName::DoesPackageExist(PackagePath));
	AutoBlueprintP3Tests::InspectTree(*this, FindObject<UWidgetBlueprint>(nullptr, *Result.TargetAssetPath));

	const FAutoBlueprintBuildResult Conflict = UAutoBlueprintLibrary::BuildWidgetBlueprint(AutoBlueprintP3Tests::Serialize(Input), Options);

	TestTrue(TEXT("Unsaved package blocks second build"), AutoBlueprintP3Tests::HasCode(Conflict, TEXT("ASSET_ALREADY_EXISTS")));
	TestFalse(TEXT("No replacement"), Conflict.bAssetCreated);
	TestTrue(TEXT("Conflict claims no created package"), Conflict.CreatedPackageName.IsEmpty());

	const FString SecondPackage = AutoBlueprintP3Tests::NewPackage();
	const FString SpecialText = TEXT("Unicode \u4e2d\u6587 \"quote\"\nsecond line");
	const TSharedRef<FJsonObject> SoloRoot = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();

	SoloRoot->SetStringField(TEXT("name"), TEXT("SoloText"));
	SoloRoot->SetStringField(TEXT("type"), TEXT("TextBlock"));
	SoloRoot->SetBoolField(TEXT("is_variable"), true);
	Properties->SetStringField(TEXT("text"), SpecialText);
	Properties->SetBoolField(TEXT("is_enabled"), false);
	Properties->SetNumberField(TEXT("render_opacity"), 0.5);
	Properties->SetStringField(TEXT("justification"), TEXT("Right"));
	Properties->SetBoolField(TEXT("auto_wrap_text"), true);
	SoloRoot->SetObjectField(TEXT("properties"), Properties);
	Input->SetObjectField(TEXT("root"), SoloRoot);
	Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), SecondPackage);

	const FAutoBlueprintBuildResult Second = UAutoBlueprintLibrary::BuildWidgetBlueprint(AutoBlueprintP3Tests::Serialize(Input), Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Second);
	};

	TestTrue(TEXT("Different root and shape build successfully"), Second.bSuccess);

	UWidgetBlueprint* SecondBlueprint = FindObject<UWidgetBlueprint>(nullptr, *Second.TargetAssetPath);
	UTextBlock* Solo = SecondBlueprint && SecondBlueprint->WidgetTree
		? Cast<UTextBlock>(SecondBlueprint->WidgetTree->RootWidget) : nullptr;

	if (TestNotNull(TEXT("Root is a TextBlock, not a fixed Canvas"), Solo))
	{
		TestEqual(TEXT("JSON text is preserved"), Solo->GetText().ToString(), SpecialText);
		TestFalse(TEXT("Enabled property"), Solo->GetIsEnabled());
		TestEqual(TEXT("Render opacity"), Solo->GetRenderOpacity(), 0.5f);
		TestEqual(TEXT("Normalized default font size"), Solo->GetFont().Size, 24);
		TestTrue(TEXT("Text wrapping"), Solo->GetAutoWrapText());
		TestNull(TEXT("A root has no parent slot"), static_cast<UPanelSlot*>(Solo->Slot));

		// UE 5.3 exposes SetJustification but no matching getter; inspect the serialized enum.
		const FByteProperty* JustificationProperty = FindFProperty<FByteProperty>(Solo->GetClass(), TEXT("Justification"));

		if (TestNotNull(TEXT("Text justification property exists"), JustificationProperty))
		{
			TestEqual(TEXT("Text alignment"), JustificationProperty->GetPropertyValue_InContainer(Solo),
				static_cast<uint8>(ETextJustify::Right));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP3PreflightTest,
	"AutoBlueprint.P3.PreflightNoWrites", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP3PreflightTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP3Tests::RequireEditor(*this))
	{
		return false;
	}

	FAutoBlueprintBuildOptions Options;

	Options.bWriteReport = false;
	for (int32 Case = 0; Case < 4; ++Case)
	{
		const FString PackagePath = AutoBlueprintP3Tests::NewPackage();
		const TSharedPtr<FJsonObject> Input = AutoBlueprintP3Tests::Fixture(PackagePath);

		if (!TestTrue(TEXT("Fixture readable"), Input.IsValid()))
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Root = Input->GetObjectField(TEXT("root"));
		const TSharedPtr<FJsonObject> Button = Root->GetArrayField(TEXT("children"))[1]->AsObject();
		FString ExpectedCode;

		if (Case == 0)
		{
			Root->SetStringField(TEXT("type"), TEXT("Overlay"));
			Root->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
			ExpectedCode = TEXT("BUILD_WIDGET_NOT_IMPLEMENTED");
		}
		else if (Case == 1)
		{
			Button->GetObjectField(TEXT("properties"))->SetObjectField(TEXT("style"), MakeShared<FJsonObject>());
			ExpectedCode = TEXT("BUILD_PROPERTY_NOT_IMPLEMENTED");
		}
		else if (Case == 2)
		{
			Button->SetStringField(TEXT("name"), TEXT("TXT_Title"));
			ExpectedCode = TEXT("DUPLICATE_WIDGET_NAME");
		}
		else
		{
			Button->GetObjectField(TEXT("properties"))->SetObjectField(TEXT("default"), MakeShared<FJsonObject>());
			ExpectedCode = TEXT("UNKNOWN_FIELD");
		}

		const FString Json = AutoBlueprintP3Tests::Serialize(Input);
		const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildWidgetBlueprint(Json, Options);

		TestFalse(TEXT("Rejected build"), Result.bSuccess);
		TestTrue(TEXT("Specific failure code"), AutoBlueprintP3Tests::HasCode(Result, *ExpectedCode));
		TestFalse(TEXT("Rejected input never creates an asset"), Result.bAssetCreated);
		TestTrue(TEXT("Rejected input never creates a package"), Result.CreatedPackageName.IsEmpty());
		TestNull(TEXT("No in-memory target"), FindPackage(nullptr, *PackagePath));
		TestFalse(TEXT("No disk target"), FPackageName::DoesPackageExist(PackagePath));
		if (Case < 2)
		{
			TestTrue(TEXT("P2 protocol validation remains independent of P3 capabilities"), UAutoBlueprintLibrary::ValidateLayout(Json).bValid);
		}
	}

	const FAutoBlueprintBuildResult Invalid = UAutoBlueprintLibrary::BuildWidgetBlueprint(TEXT("{"), Options);

	TestTrue(TEXT("JSON parsing cannot be bypassed"), AutoBlueprintP3Tests::HasCode(Invalid, TEXT("INVALID_JSON")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP3PersistenceTest,
	"AutoBlueprint.P3.SaveReloadAndConflict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP3PersistenceTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP3Tests::RequireEditor(*this))
	{
		return false;
	}

	const FString PackagePath = AutoBlueprintP3Tests::NewPackage();
	const TSharedPtr<FJsonObject> Input = AutoBlueprintP3Tests::Fixture(PackagePath);
	FAutoBlueprintBuildOptions Options;

	if (!TestTrue(TEXT("Fixture readable"), Input.IsValid()))
	{
		return false;
	}

	const FString Json = AutoBlueprintP3Tests::Serialize(Input);
	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildWidgetBlueprint(Json, Options);
	const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	TArray<uint8> Before;
	FString ReportJson;
	TSharedPtr<FJsonObject> Report;

	if (!TestTrue(TEXT("Build and requested save succeeded"), Result.bSuccess && Result.bSaved)
		|| !TestTrue(TEXT("Saved asset is readable"), FFileHelper::LoadFileToArray(Before, *Filename)))
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Result);
		return false;
	}

	TestTrue(TEXT("Report file is readable"), FFileHelper::LoadFileToString(ReportJson, *Result.ReportFilePath));
	if (TestTrue(TEXT("Report is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ReportJson), Report)))
	{
		TestTrue(TEXT("Report matches asset outcomes"), Report->GetBoolField(TEXT("success")) && Report->GetBoolField(TEXT("saved")));
		TestEqual(TEXT("Report names only this package"), Report->GetStringField(TEXT("created_package_name")), PackagePath);
	}

	// Release the entire test-owned package, then require a real load from disk.
	TArray<UPackage*> Packages;

	Packages.Add(FindPackage(nullptr, *PackagePath));
	if (!TestTrue(TEXT("Saved fixture package unloads"), UPackageTools::UnloadPackages(Packages)))
	{
		return false;
	}

	Packages.Reset();
	TestNull(TEXT("No cached package before reload"), FindPackage(nullptr, *PackagePath));

	TStrongObjectPtr<UWidgetBlueprint> Reloaded(LoadObject<UWidgetBlueprint>(nullptr, *Result.TargetAssetPath));

	if (!AutoBlueprintP3Tests::InspectTree(*this, Reloaded.Get()))
	{
		return false;
	}

	const bool bWasDirty = Reloaded->GetOutermost()->IsDirty();
	const FAutoBlueprintBuildResult Conflict = UAutoBlueprintLibrary::BuildWidgetBlueprint(Json, Options);
	TArray<uint8> After;

	TestTrue(TEXT("Disk conflict is reported"), AutoBlueprintP3Tests::HasCode(Conflict, TEXT("ASSET_ALREADY_EXISTS")));
	TestFalse(TEXT("Conflict never saves"), Conflict.bSaved);
	TestTrue(TEXT("Conflict preserves original object"), FindObject<UWidgetBlueprint>(nullptr, *Result.TargetAssetPath) == Reloaded.Get());
	TestEqual(TEXT("Conflict preserves dirty flag"), Reloaded->GetOutermost()->IsDirty(), bWasDirty);
	TestTrue(TEXT("Original file remains byte-for-byte unchanged by rejected build"), FFileHelper::LoadFileToArray(After, *Filename) && Before == After);

	UWorld* World = GEditor->GetEditorWorldContext().World();

	if (!TestNotNull(TEXT("Editor world exists for instantiation"), World))
	{
		return false;
	}

	// Materialize the class argument explicitly instead of chaining TObjectPtr and TSubclassOf conversions.
	const TSubclassOf<UUserWidget> WidgetClass(Reloaded->GeneratedClass.Get());
	TStrongObjectPtr<UUserWidget> Instance(CreateWidget<UUserWidget>(World, WidgetClass));

	TestNotNull(TEXT("Reloaded class can instantiate in editor world"), Instance.Get());
	if (Instance.IsValid())
	{
		TestNotNull(TEXT("Instance has generated title"), Instance->GetWidgetFromName(TEXT("TXT_Title")));
	}

	AddInfo(TEXT("Saved fixture retained for inspection: ") + Result.TargetAssetPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP3FileLuaTest,
	"AutoBlueprint.P3.FileLuaAndSaveFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP3FileLuaTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP3Tests::RequireEditor(*this))
	{
		return false;
	}

	const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PackagePath = AutoBlueprintP3Tests::NewPackage();
	const TSharedPtr<FJsonObject> Input = AutoBlueprintP3Tests::Fixture(PackagePath);
	const FString RelativePath = TEXT("UIBlueprintLayouts/P3_") + Id + TEXT(".json");
	const FString FullPath = FPaths::ProjectDir() / RelativePath;

	if (!TestTrue(TEXT("Fixture readable"), Input.IsValid()))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
	if (!TestTrue(TEXT("Write unique layout fixture"), FFileHelper::SaveStringToFile(AutoBlueprintP3Tests::Serialize(Input), *FullPath)))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*FullPath);
	};

	FAutoBlueprintLuaRunRequest Request;

	Request.ScriptPath = TEXT("tests/p3_build_no_save.lua");
	Request.Arguments.Add(RelativePath);

	const FAutoBlueprintLuaRunResult ScriptResult = UAutoBlueprintLibrary::RunEditorScript(Request);
	FAutoBlueprintBuildResult Created;

	Created.CreatedPackageName = PackagePath;
	Created.TargetAssetPath = PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Created);
	};

	TestTrue(TEXT("Actual UnLua build options/result reflection succeeds"), ScriptResult.bSuccess);
	TestFalse(TEXT("Lua no-save build never writes asset"), FPackageName::DoesPackageExist(PackagePath));

	// Make a filesystem obstruction below a unique test-owned directory: saving must fail honestly.
	const FString BlockerPackage = TEXT("/Game/UI/AutoBlueprintTests/P3_Block_") + Id;
	const FString Blocker = FPackageName::LongPackageNameToFilename(BlockerPackage);
	const FString FailedPackage = BlockerPackage + TEXT("/WBP_SaveFailure");

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Blocker), true);
	if (!TestTrue(TEXT("Create test-owned obstruction"), FFileHelper::SaveStringToFile(TEXT("P3 save obstruction"), *Blocker)))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*Blocker);
	};

	Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), FailedPackage);

	FAutoBlueprintBuildOptions Options;
	const FAutoBlueprintBuildResult Failed = UAutoBlueprintLibrary::BuildWidgetBlueprint(AutoBlueprintP3Tests::Serialize(Input), Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Failed);
	};

	TestTrue(TEXT("Saving fails after successful creation and compile"), !Failed.bSuccess && Failed.bAssetCreated && Failed.bCompiled && !Failed.bSaved);
	TestTrue(TEXT("Save failure diagnostic"), AutoBlueprintP3Tests::HasCode(Failed, TEXT("PACKAGE_SAVE_FAILED")));
	TestEqual(TEXT("Partial result names its unsaved package"), Failed.CreatedPackageName, FailedPackage);
	TestFalse(TEXT("Failed save did not write an asset"), FPackageName::DoesPackageExist(FailedPackage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP3CompileFailureTest,
	"AutoBlueprint.P3.CompileFailureDoesNotSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP3CompileFailureTest::RunTest(const FString& Parameters)
{
	if (!AutoBlueprintP3Tests::RequireEditor(*this))
	{
		return false;
	}

	const FString PackagePath = AutoBlueprintP3Tests::NewPackage();
	const TSharedPtr<FJsonObject> Input = AutoBlueprintP3Tests::Fixture(PackagePath);
	FAutoBlueprintBuildOptions Options;

	if (!TestTrue(TEXT("Fixture readable"), Input.IsValid()))
	{
		return false;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	Root->SetStringField(TEXT("name"), TEXT("Slot"));
	Root->SetStringField(TEXT("type"), TEXT("TextBlock"));
	Root->SetBoolField(TEXT("is_variable"), true);
	Input->SetObjectField(TEXT("root"), Root);

	// Structural marking may log the deliberate native-member collision during skeleton compilation.
	AddExpectedError(TEXT("Slot"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(PackagePath, EAutomationExpectedErrorFlags::Contains, 0);

	const FAutoBlueprintBuildResult Result = UAutoBlueprintLibrary::BuildWidgetBlueprint(AutoBlueprintP3Tests::Serialize(Input), Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP3Tests::ReleaseUnsaved(Result);
	};

	TestTrue(TEXT("Tree was created before compiler rejection"), Result.bAssetCreated);
	TestFalse(TEXT("Native member type collision cannot compile"), Result.bCompiled);
	TestFalse(TEXT("Compiler failure is not success"), Result.bSuccess);
	TestFalse(TEXT("Compiler failure is never saved"), Result.bSaved);
	TestTrue(TEXT("Compiler diagnostics reach the result"), AutoBlueprintP3Tests::HasCode(Result, TEXT("BLUEPRINT_COMPILE_FAILED")));
	TestFalse(TEXT("No failed Blueprint on disk"), FPackageName::DoesPackageExist(PackagePath));
	return true;
}

#endif
