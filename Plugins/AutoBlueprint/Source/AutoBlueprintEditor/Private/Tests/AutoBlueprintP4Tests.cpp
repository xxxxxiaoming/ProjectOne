#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/CreateOnlyAssetWriter.h"
#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SubclassOf.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace AutoBlueprintP4Tests
{
	FString Serialize(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;

		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	bool HasCode(const TArray<FAutoBlueprintDiagnostic>& Diagnostics, const TCHAR* Code)
	{
		return Diagnostics.ContainsByPredicate([Code](const FAutoBlueprintDiagnostic& Item)
		{
			return Item.Code == Code;
		});
	}

	struct FFixture
	{
		FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Base = TEXT("/Game/UI/AutoBlueprintTests/P4_") + Id;
		FString SourceDirectory = TEXT("Content/UI/AutoBlueprintTests/P4_") + Id;
		FString LayoutFile = TEXT("UIBlueprintLayouts/P4_") + Id + TEXT(".json");
		TSharedPtr<FJsonObject> Input;
		TStrongObjectPtr<UFont> Font;

		bool Initialize(FAutomationTestBase& Test)
		{
			FString Code;
			FString Message;

			if (!AutoBlueprint::CheckEditorContext(Code, Message))
			{
				Test.AddError(Code + TEXT(": ") + Message);
				return false;
			}

			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
			FString Json;

			if (!Test.TestTrue(TEXT("P4 example readable"), Plugin.IsValid()
				&& FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Examples/p4_styles.layout.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Input) && Input.IsValid()))
			{
				return false;
			}

			Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Base + TEXT("/WBP_Styles"));
			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / SourceDirectory), true);

			IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
			TArray<uint8> Pixels;

			Pixels.Init(255, 8 * 8 * 4);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const TCHAR* Alias = Index == 0 ? TEXT("button") : TEXT("photo");
				const FString Source = SourceDirectory / (Index == 0 ? TEXT("button.png") : TEXT("photo.jpg"));
				const TSharedPtr<IImageWrapper> Image = Images.CreateImageWrapper(Index == 0 ? EImageFormat::PNG : EImageFormat::JPEG);

				if (!Test.TestTrue(TEXT("Test image encodes"), Image.IsValid() && Image->SetRaw(Pixels.GetData(), Pixels.Num(), 8, 8, ERGBFormat::BGRA, 8)))
				{
					return false;
				}

				const TArray64<uint8>& Compressed = Image->GetCompressed(90);
				TArray<uint8> Encoded;

				Encoded.Append(Compressed.GetData(), static_cast<int32>(Compressed.Num()));

				if (!Test.TestTrue(TEXT("Test source written"), FFileHelper::SaveArrayToFile(Encoded, *(FPaths::ProjectDir() / Source))))
				{
					return false;
				}

				const TSharedPtr<FJsonObject> Resource = Input->GetObjectField(TEXT("resources"))->GetObjectField(Alias);

				Resource->SetStringField(TEXT("source_file"), Source);
				Resource->SetStringField(TEXT("package_path"), Base + TEXT("/T_") + Alias);
			}

			// Duplicate a real runtime font into a unique test package: P4 never imports or edits fonts.
			UFont* NativeFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
			TStrongObjectPtr<UPackage> FontPackage(CreatePackage(*(Base + TEXT("/F_Existing"))));

			if (!Test.TestNotNull(TEXT("Engine Roboto font available"), NativeFont))
			{
				return false;
			}

			Font.Reset(DuplicateObject<UFont>(NativeFont, FontPackage.Get(), TEXT("F_Existing")));
			Font->SetFlags(RF_Public | RF_Standalone);
			Font->MarkPackageDirty();
			Input->GetObjectField(TEXT("root"))->GetArrayField(TEXT("children"))[0]->AsObject()
				->GetObjectField(TEXT("properties"))->SetStringField(TEXT("font"), Font->GetPathName());
			return true;
		}

		bool WriteLayout() const
		{
			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / TEXT("UIBlueprintLayouts")), true);
			return FFileHelper::SaveStringToFile(Serialize(Input), *(FPaths::ProjectDir() / LayoutFile), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		~FFixture()
		{
			// Only GUID-named raw files owned by this test; saved .uassets remain for inspection.
			IFileManager::Get().DeleteDirectory(*(FPaths::ProjectDir() / SourceDirectory), false, true);
			IFileManager::Get().Delete(*(FPaths::ProjectDir() / LayoutFile), false, true);
			if (Font.IsValid())
			{
				Font->ClearFlags(RF_Standalone);
				Font->GetOutermost()->SetDirtyFlag(false);
			}
		}
	};

	void ReleaseUnsaved(const FAutoBlueprintGenerationResult& Result)
	{
		TArray<FString> Paths;

		if (!Result.Widget.bSaved && Result.Widget.bAssetCreated)
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

	bool Inspect(FAutomationTestBase& Test, UWidgetBlueprint* Blueprint, const FString& FontPath)
	{
		if (!Test.TestNotNull(TEXT("Widget Blueprint exists"), Blueprint)
			|| !Test.TestNotNull(TEXT("Template tree exists"), static_cast<UWidgetTree*>(Blueprint->WidgetTree)))
		{
			return false;
		}

		UImage* Image = Cast<UImage>(Blueprint->WidgetTree->FindWidget(TEXT("IMG_Photo")));
		UButton* Button = Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("BTN_Confirm")));
		UTextBlock* Text = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TXT_Title")));
		const FStructProperty* BrushProperty = FindFProperty<FStructProperty>(UImage::StaticClass(), TEXT("Brush"));

		if (!Test.TestNotNull(TEXT("Image template"), Image) || !Test.TestNotNull(TEXT("Button template"), Button)
			|| !Test.TestNotNull(TEXT("Text template"), Text) || !Test.TestNotNull(TEXT("Serialized Image brush"), BrushProperty))
		{
			return false;
		}

		const FSlateBrush& Brush = *BrushProperty->ContainerPtrToValuePtr<FSlateBrush>(Image);
		UTexture2D* Photo = Cast<UTexture2D>(Brush.GetResourceObject());
		const FButtonStyle& Style = Button->GetStyle();
		UTexture2D* ButtonTexture = Cast<UTexture2D>(Style.Normal.GetResourceObject());

		Test.TestNotNull(TEXT("Image references imported JPEG texture"), Photo);
		Test.TestNotNull(TEXT("Button references imported PNG texture"), ButtonTexture);
		Test.TestTrue(TEXT("Image draw mode, dimensions and tint"), Brush.DrawAs == ESlateBrushDrawType::Image
			&& Brush.ImageSize.X == 8.0f && Brush.ImageSize.Y == 8.0f
			&& Brush.TintColor.GetSpecifiedColor().Equals(FLinearColor(1.0f, 0.5f, 0.25f, 0.8f)));
		Test.TestEqual(TEXT("Image render opacity"), Image->GetRenderOpacity(), 0.75f);
		Test.TestTrue(TEXT("Four states reuse the same texture object"), Style.Hovered.GetResourceObject() == ButtonTexture
			&& Style.Pressed.GetResourceObject() == ButtonTexture && Style.Disabled.GetResourceObject() == ButtonTexture);
		Test.TestTrue(TEXT("Box draw and normalized nine-slice margin"), Style.Normal.DrawAs == ESlateBrushDrawType::Box
			&& Style.Normal.Margin == FMargin(0.25f));
		Test.TestTrue(TEXT("Hovered tint"), Style.Hovered.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.5f, 1, 0.5f, 1)));
		Test.TestTrue(TEXT("Pressed tint"), Style.Pressed.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.5f, 0.5f, 1, 1)));
		Test.TestTrue(TEXT("Disabled tint and alpha"), Style.Disabled.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.25f, 0.25f, 0.25f, 0.5f)));
		Test.TestTrue(TEXT("Configured UFont reference"), Text->GetFont().FontObject && Text->GetFont().FontObject->GetPathName() == FontPath);
		Test.TestEqual(TEXT("Font size persists"), Text->GetFont().Size, 32.0f);
		Test.TestTrue(TEXT("Native runtime parent only"), Blueprint->ParentClass == UUserWidget::StaticClass());
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP4ImportTest,
	"AutoBlueprint.P4.ImportStylesAndReuse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP4ImportTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP4Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	FAutoBlueprintBuildOptions Options;

	Options.bSave = false;
	Options.bWriteReport = false;

	const FString Json = AutoBlueprintP4Tests::Serialize(Fixture.Input);
	const FAutoBlueprintValidationResult Validation = UAutoBlueprintLibrary::ValidateLayout(Json);

	TestTrue(TEXT("Read-only validation accepts actual PNG/JPEG and font"), Validation.bValid);
	TestNull(TEXT("Validation did not create texture package"), FindPackage(nullptr, *(Fixture.Base + TEXT("/T_button"))));

	const FAutoBlueprintBuildResult Legacy = UAutoBlueprintLibrary::BuildWidgetBlueprint(Json, Options);

	TestTrue(TEXT("Single-package API directs resources to task API"), AutoBlueprintP4Tests::HasCode(Legacy.Diagnostics, TEXT("RESOURCE_TASK_REQUIRED")));
	TestFalse(TEXT("Legacy refusal never creates Widget"), Legacy.bAssetCreated);

	const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(Json, Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP4Tests::ReleaseUnsaved(Result);
	};

	if (!TestTrue(TEXT("Unsaved task succeeded"), Result.bSuccess) || !TestEqual(TEXT("Two independent imports"), Result.Resources.Num(), 2))
	{
		return false;
	}

	AutoBlueprintP4Tests::Inspect(*this, FindObject<UWidgetBlueprint>(nullptr, *Result.Widget.TargetAssetPath), Fixture.Font->GetPathName());
	for (const FAutoBlueprintResourceResult& Item : Result.Resources)
	{
		TestTrue(TEXT("Import without save reports truthfully"), Item.bAttempted && Item.bImported && !Item.bReused && !Item.bSaved && Item.bSuccess);
		TestFalse(TEXT("Unsaved import has no disk asset"), FPackageName::DoesPackageExist(Item.CreatedPackageName));

		UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *Item.TargetAssetPath);

		if (!TestNotNull(TEXT("Imported texture"), Texture))
		{
			return false;
		}

		TestFalse(TEXT("Imported texture compilation completes before returning the task"), Texture->IsCompiling());
		TestTrue(TEXT("New texture has UI import settings"), Texture->LODGroup == TEXTUREGROUP_UI
			&& Texture->CompressionSettings == TC_EditorIcon && Texture->MipGenSettings == TMGS_NoMipmaps && Texture->SRGB);

		// Set up a different, fully built shared texture before testing read-only reuse.
		// Direct property writes without PreEditChange can invalidate an in-flight build.
		Texture->PreEditChange(nullptr);
		Texture->LODGroup = TEXTUREGROUP_World;
		Texture->CompressionSettings = TC_Grayscale;
		Texture->MipGenSettings = TMGS_FromTextureGroup;
		Texture->Filter = TF_Nearest;
		Texture->SRGB = false;
		Texture->PostEditChange();

		UTexture* TexturesToFinish[] = { Texture };

		FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
		TestFalse(TEXT("Shared fixture compilation completes before reuse"), Texture->IsCompiling());
		Texture->GetOutermost()->SetDirtyFlag(false);

		const TSharedRef<FJsonObject> Reuse = MakeShared<FJsonObject>();

		Reuse->SetStringField(TEXT("object_path"), Item.TargetAssetPath);
		Fixture.Input->GetObjectField(TEXT("resources"))->SetObjectField(Item.Alias, Reuse);
	}

	Fixture.Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Fixture.Base + TEXT("/WBP_Reuse"));

	const FAutoBlueprintGenerationResult Reused = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

	AutoBlueprintP4Tests::ReleaseUnsaved(Reused);
	TestTrue(TEXT("Reuse task succeeded"), Reused.bSuccess);
	for (const FAutoBlueprintResourceResult& Item : Reused.Resources)
	{
		UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *Item.TargetAssetPath);

		TestTrue(TEXT("Reuse never claims creation or saving"), Item.bReused && !Item.bImported && !Item.bSaved && Item.CreatedPackageName.IsEmpty());
		TestTrue(TEXT("Shared settings and dirty state unchanged"), Texture && Texture->LODGroup == TEXTUREGROUP_World
			&& Texture->CompressionSettings == TC_Grayscale && Texture->MipGenSettings == TMGS_FromTextureGroup
			&& Texture->Filter == TF_Nearest && !Texture->SRGB && !Texture->GetOutermost()->IsDirty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP4PersistenceTest,
	"AutoBlueprint.P4.SaveReloadAndLua", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP4PersistenceTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP4Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	FString Code;
	FString Message;
	const FString FontPath = Fixture.Font->GetPathName();

	if (!TestTrue(TEXT("Test fixture font saved before reuse"), AutoBlueprint::SaveCreatedAsset(*Fixture.Font.Get(), Code, Message))
		|| !TestTrue(TEXT("Layout file written"), Fixture.WriteLayout()))
	{
		return false;
	}

	FAutoBlueprintLuaRunRequest Request;

	Request.ScriptPath = TEXT("tests/p4_generate.lua");
	Request.Arguments.Add(Fixture.LayoutFile);

	const FAutoBlueprintLuaRunResult Script = UAutoBlueprintLibrary::RunEditorScript(Request);

	if (!TestTrue(TEXT("Real UnLua reflects nested task/resource results and saves all three new packages"), Script.bSuccess))
	{
		AddError(Script.Message);
		return false;
	}

	const FString WidgetPath = Fixture.Base + TEXT("/WBP_Styles.WBP_Styles");
	TArray<UPackage*> Packages;

	for (const TCHAR* Name : { TEXT("WBP_Styles"), TEXT("T_button"), TEXT("T_photo"), TEXT("F_Existing") })
	{
		const FString Path = Fixture.Base / Name;

		TestTrue(TEXT("Fixture package exists on disk"), FPackageName::DoesPackageExist(Path));
		Packages.Add(FindPackage(nullptr, *Path));
	}

	Fixture.Font.Reset();
	if (!TestTrue(TEXT("Saved packages unload together"), UPackageTools::UnloadPackages(Packages)))
	{
		return false;
	}

	Packages.Reset();
	TestNull(TEXT("Widget not cached before reload"), FindPackage(nullptr, *(Fixture.Base + TEXT("/WBP_Styles"))));

	TStrongObjectPtr<UWidgetBlueprint> Reloaded(LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath));

	if (!AutoBlueprintP4Tests::Inspect(*this, Reloaded.Get(), FontPath))
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	const TSubclassOf<UUserWidget> WidgetClass(Reloaded->GeneratedClass.Get());

	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	TStrongObjectPtr<UUserWidget> Instance(CreateWidget<UUserWidget>(World, WidgetClass));

	TestNotNull(TEXT("Reloaded class instantiates"), Instance.Get());
	// Reuse on a saving task must still leave existing texture files untouched.
	const FString TextureFile = FPackageName::LongPackageNameToFilename(Fixture.Base + TEXT("/T_button"), FPackageName::GetAssetPackageExtension());
	TArray<uint8> Before;
	TArray<uint8> After;

	TestTrue(TEXT("Saved shared texture readable"), FFileHelper::LoadFileToArray(Before, *TextureFile));
	Fixture.Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Fixture.Base + TEXT("/WBP_SavedReuse"));
	for (const TCHAR* Alias : { TEXT("button"), TEXT("photo") })
	{
		const TSharedRef<FJsonObject> Resource = MakeShared<FJsonObject>();
		const FString Name = FString(TEXT("T_")) + Alias;

		Resource->SetStringField(TEXT("object_path"), Fixture.Base / Name + TEXT(".") + Name);
		Fixture.Input->GetObjectField(TEXT("resources"))->SetObjectField(Alias, Resource);
	}

	FAutoBlueprintBuildOptions Options;
	const FAutoBlueprintGenerationResult Reused = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

	AutoBlueprintP4Tests::ReleaseUnsaved(Reused);
	TestTrue(TEXT("Saving task reuses textures and saves only its Widget"), Reused.bSuccess && Reused.Widget.bSaved);
	for (const FAutoBlueprintResourceResult& Item : Reused.Resources)
	{
		TestTrue(TEXT("Existing texture never reports imported or saved"), Item.bReused && Item.bSuccess && !Item.bImported && !Item.bSaved);
	}

	TestTrue(TEXT("Shared texture file unchanged by successful saving task"), FFileHelper::LoadFileToArray(After, *TextureFile) && Before == After);
	AddInfo(TEXT("Saved P4 fixture retained for visual inspection: ") + WidgetPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP4PreflightTest,
	"AutoBlueprint.P4.PreflightNoImports", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP4PreflightTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 7; ++Case)
	{
		AutoBlueprintP4Tests::FFixture Fixture;

		if (!Fixture.Initialize(*this))
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Resources = Fixture.Input->GetObjectField(TEXT("resources"));
		const TSharedPtr<FJsonObject> Photo = Resources->GetObjectField(TEXT("photo"));
		const FString PhotoSource = FPaths::ProjectDir() / Photo->GetStringField(TEXT("source_file"));
		FString Expected;

		if (Case == 0)
		{
			Photo->SetStringField(TEXT("source_file"), Fixture.SourceDirectory / TEXT("missing.png"));
			Expected = TEXT("MISSING_RESOURCE");
		}
		else if (Case == 1)
		{
			FFileHelper::SaveStringToFile(TEXT("version https://git-lfs.github.com/spec/v1\noid sha256:fixture\nsize 1\n"),
				*PhotoSource, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Expected = TEXT("LFS_SOURCE_NOT_MATERIALIZED");
		}
		else if (Case == 2)
		{
			FFileHelper::SaveStringToFile(TEXT("not an image"), *PhotoSource, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Expected = TEXT("INVALID_IMAGE");
		}
		else if (Case == 3)
		{
			Photo->Values.Empty();
			Photo->SetStringField(TEXT("object_path"), Fixture.Font->GetPathName());
			Expected = TEXT("INVALID_RESOURCE_TYPE");
		}
		else if (Case == 4)
		{
			Photo->SetStringField(TEXT("package_path"), Fixture.Font->GetOutermost()->GetName());
			Expected = TEXT("ASSET_ALREADY_EXISTS");
		}
		else if (Case == 5)
		{
			Fixture.Input->GetObjectField(TEXT("root"))->GetArrayField(TEXT("children"))[2]->AsObject()
				->GetObjectField(TEXT("properties"))->GetObjectField(TEXT("style"))->SetObjectField(TEXT("focused"), MakeShared<FJsonObject>());
			Expected = TEXT("UNSUPPORTED_FOCUS_BRUSH");
		}
		else
		{
			const TSharedPtr<FJsonObject> Root = Fixture.Input->GetObjectField(TEXT("root"));

			Root->SetStringField(TEXT("type"), TEXT("Overlay"));
			Root->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
			Expected = TEXT("BUILD_WIDGET_NOT_IMPLEMENTED");
		}

		FAutoBlueprintBuildOptions Options;

		Options.bWriteReport = false;

		const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

		TestFalse(TEXT("Invalid task fails"), Result.bSuccess);
		TestTrue(TEXT("Specific preflight diagnostic"), AutoBlueprintP4Tests::HasCode(Result.Widget.Diagnostics, *Expected));
		TestTrue(TEXT("Failure precedes all import attempts"), Result.Resources.IsEmpty());
		TestNull(TEXT("No first texture package"), FindPackage(nullptr, *(Fixture.Base + TEXT("/T_button"))));
		TestNull(TEXT("No Widget package"), FindPackage(nullptr, *(Fixture.Base + TEXT("/WBP_Styles"))));
		TestFalse(TEXT("No first texture on disk"), FPackageName::DoesPackageExist(Fixture.Base + TEXT("/T_button")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP4CompileFailureTest,
	"AutoBlueprint.P4.CompileFailureSavesNothing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP4CompileFailureTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP4Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Root = Fixture.Input->GetObjectField(TEXT("root"));

	Root->SetStringField(TEXT("name"), TEXT("Slot"));
	Root->SetBoolField(TEXT("is_variable"), true);
	AddExpectedError(TEXT("Slot"), EAutomationExpectedErrorFlags::Contains, 0);

	FAutoBlueprintBuildOptions Options;
	const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

	AutoBlueprintP4Tests::ReleaseUnsaved(Result);
	TestFalse(TEXT("Compile failure fails task"), Result.bSuccess);
	TestTrue(TEXT("Compiler diagnostic present"), AutoBlueprintP4Tests::HasCode(Result.Widget.Diagnostics, TEXT("BLUEPRINT_COMPILE_FAILED")));
	TestTrue(TEXT("Widget created but not compiled or saved"), Result.Widget.bAssetCreated && !Result.Widget.bCompiled && !Result.Widget.bSaved);
	TestEqual(TEXT("Both images imported before compile"), Result.Resources.Num(), 2);
	for (const FAutoBlueprintResourceResult& Item : Result.Resources)
	{
		TestTrue(TEXT("Texture remains imported and unsaved"), Item.bImported && !Item.bSaved && !Item.bSuccess);
		TestFalse(TEXT("Compile failure does not leave a texture file"), FPackageName::DoesPackageExist(Item.CreatedPackageName));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP4PartialSaveTest,
	"AutoBlueprint.P4.PartialSaveReports", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP4PartialSaveTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 2; ++Case)
	{
		AutoBlueprintP4Tests::FFixture Fixture;

		if (!Fixture.Initialize(*this))
		{
			return false;
		}

		FString Code;
		FString Message;

		if (!TestTrue(TEXT("Existing font fixture saved"), AutoBlueprint::SaveCreatedAsset(*Fixture.Font.Get(), Code, Message)))
		{
			return false;
		}

		const FString BlockedDirectory = Fixture.Base + TEXT("/Blocked");
		const FString BlockerFilename = FPackageName::LongPackageNameToFilename(BlockedDirectory);
		const TSharedPtr<FJsonObject> Target = Case == 0
			? Fixture.Input->GetObjectField(TEXT("resources"))->GetObjectField(TEXT("photo")) : Fixture.Input->GetObjectField(TEXT("asset"));

		Target->SetStringField(TEXT("package_path"), BlockedDirectory / (Case == 0 ? TEXT("T_photo") : TEXT("WBP_Styles")));
		if (!TestTrue(TEXT("Test-owned file blocks target directory"), FFileHelper::SaveStringToFile(TEXT("P4 fixture blocker"), *BlockerFilename)))
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			IFileManager::Get().Delete(*BlockerFilename, false, true);
		};

		FAutoBlueprintBuildOptions Options;
		const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

		AutoBlueprintP4Tests::ReleaseUnsaved(Result);
		if (!TestEqual(TEXT("Both resources reported"), Result.Resources.Num(), 2))
		{
			return false;
		}

		TestFalse(TEXT("Partial save fails overall task"), Result.bSuccess);
		TestTrue(TEXT("First texture really saved"), Result.Resources[0].bSaved && Result.Resources[0].bSuccess
			&& FPackageName::DoesPackageExist(Result.Resources[0].CreatedPackageName));
		TestEqual(TEXT("Second texture saved only in Widget-failure case"), Result.Resources[1].bSaved, Case == 1);
		TestTrue(TEXT("Widget compiled but never saved"), Result.Widget.bCompiled && !Result.Widget.bSaved && !Result.Widget.bSuccess);
		TestTrue(TEXT("Failure attributed to the correct package"), Case == 0
			? AutoBlueprintP4Tests::HasCode(Result.Resources[1].Diagnostics, TEXT("PACKAGE_SAVE_FAILED"))
			: AutoBlueprintP4Tests::HasCode(Result.Widget.Diagnostics, TEXT("PACKAGE_SAVE_FAILED")));

		FString Json;
		TSharedPtr<FJsonObject> Report;

		if (TestTrue(TEXT("Report readable and valid JSON"), FFileHelper::LoadFileToString(Json, *Result.ReportFilePath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Report)))
		{
			TestEqual(TEXT("Independent-result report version"), Report->GetNumberField(TEXT("report_version")), 2.0);
			TestFalse(TEXT("Report does not invent an aggregate saved flag"), Report->HasField(TEXT("saved")));
			TestFalse(TEXT("Report shows Widget unsaved"), Report->GetObjectField(TEXT("widget"))->GetBoolField(TEXT("saved")));
			TestTrue(TEXT("Report preserves first successful save"), Report->GetArrayField(TEXT("resources"))[0]->AsObject()->GetBoolField(TEXT("saved")));
		}

		// A fresh Widget name still cannot overwrite the already saved first texture.
		const FString SavedTextureFile = FPackageName::LongPackageNameToFilename(Result.Resources[0].CreatedPackageName, FPackageName::GetAssetPackageExtension());
		TArray<uint8> Before;
		TArray<uint8> After;

		TestTrue(TEXT("Read saved texture bytes"), FFileHelper::LoadFileToArray(Before, *SavedTextureFile));
		Fixture.Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Fixture.Base + TEXT("/WBP_Conflict"));

		const FAutoBlueprintGenerationResult Conflict = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP4Tests::Serialize(Fixture.Input), Options);

		TestTrue(TEXT("Texture CreateOnly conflict"), AutoBlueprintP4Tests::HasCode(Conflict.Widget.Diagnostics, TEXT("ASSET_ALREADY_EXISTS")));
		TestTrue(TEXT("Conflict does not rewrite saved texture"), FFileHelper::LoadFileToArray(After, *SavedTextureFile) && Before == After);
		TestFalse(TEXT("Conflict creates no new Widget"), Conflict.Widget.bAssetCreated);
	}

	return true;
}

#endif
