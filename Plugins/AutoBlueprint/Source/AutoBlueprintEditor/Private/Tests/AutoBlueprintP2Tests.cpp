#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AutoBlueprintLibrary.h"
#include "AutoBlueprintTypes.h"
#include "Dom/JsonObject.h"
#include "Layout/LayoutValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/WidgetTypeRegistry.h"

namespace
{
	FString P2Json(const FString& Package, const FString& Root = TEXT("{\"name\":\"Root\",\"type\":\"CanvasPanel\",\"is_variable\":false}"))
	{
		return TEXT("{\"schema_version\":1,\"asset\":{\"package_path\":\"") + Package
			+ TEXT("\"},\"design\":{\"width\":1080,\"height\":1920},\"root\":") + Root + TEXT("}");
	}
	bool HasP2Code(const FAutoBlueprintValidationResult& Result, const TCHAR* Code)
	{
		return Result.Diagnostics.ContainsByPredicate([Code](const FAutoBlueprintDiagnostic& D) { return D.Code == Code; });
	}
	FString P2Target()
	{
		return TEXT("/Game/UI/AutoBlueprintTests/WBP_P2_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP2ProtocolTest, "AutoBlueprint.P2.Protocol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAutoBlueprintP2ProtocolTest::RunTest(const FString& Parameters)
{
	const FString Package = P2Target();
	const FString Valid = P2Json(Package);
	AutoBlueprint::FLayoutSpec Layout;
	const auto Parsed = AutoBlueprint::ValidateLayoutJson(Valid, Layout);
	if (!TestTrue(TEXT("Valid layout parses"), Parsed.bValid))
	{
		for (const auto& D : Parsed.Diagnostics) { AddError(D.Code + TEXT(": ") + D.Message); }
		return false;
	}
	TestEqual(TEXT("Typed package"), Layout.PackagePath, Package);
	TestEqual(TEXT("Schema default opacity materialized"), Layout.Root.Properties.Fields[TEXT("render_opacity")]->Number, 1.0);
	TestTrue(TEXT("Public reflected facade"), UAutoBlueprintLibrary::ValidateLayout(Valid).bValid);
	TestNull(TEXT("Preflight creates no package"), FindPackage(nullptr, *Package));
	TestFalse(TEXT("Preflight writes no file"), FPackageName::DoesPackageExist(Package));
	struct FCase { FString Json; const TCHAR* Code; };
	const FCase Cases[] = {
		{ TEXT("{"), TEXT("INVALID_JSON") },
		{ Valid.Replace(TEXT("\"schema_version\":1"), TEXT("\"schema_version\":2")), TEXT("UNSUPPORTED_SCHEMA_VERSION") },
		{ Valid.Replace(TEXT("\"schema_version\":1"), TEXT("\"schema_version\":1,\"schema_version\":1")), TEXT("DUPLICATE_JSON_FIELD") },
		{ Valid.Replace(TEXT("\"is_variable\":false"), TEXT("\"is_variable\":\"false\"")), TEXT("INVALID_FIELD_TYPE") },
		{ Valid.Replace(TEXT("\"CanvasPanel\""), TEXT("\"ListView\"")), TEXT("UNSUPPORTED_WIDGET") },
		{ Valid.Replace(TEXT("\"is_variable\":false"), TEXT("\"is_variable\":false,\"typo\":1")), TEXT("UNKNOWN_FIELD") },
		{ Valid.Replace(TEXT("\"width\":1080"), TEXT("\"width\":0")), TEXT("INVALID_VALUE") },
		{ P2Json(TEXT("/Game/Outside/WBP_Test")), TEXT("INVALID_PACKAGE_PATH") },
		{ P2Json(Package, TEXT("{\"name\":\"Root\",\"type\":\"CanvasPanel\",\"is_variable\":false,\"slot\":{\"type\":\"Canvas\"}}")), TEXT("INVALID_SLOT") },
		{ P2Json(Package, TEXT("{\"name\":\"Root\",\"type\":\"CanvasPanel\",\"is_variable\":false,\"children\":[{\"name\":\"root\",\"type\":\"Image\",\"is_variable\":true,\"slot\":{\"type\":\"Canvas\"}}]}")), TEXT("DUPLICATE_WIDGET_NAME") },
		{ P2Json(Package, TEXT("{\"name\":\"Root\",\"type\":\"Image\",\"is_variable\":false,\"properties\":{\"brush\":{\"resource\":\"missing\"}}}")), TEXT("MISSING_RESOURCE") }
	};
	for (const FCase& Case : Cases)
	{
		const auto Result = UAutoBlueprintLibrary::ValidateLayout(Case.Json);
		TestFalse(Case.Code, Result.bValid);
		TestTrue(Case.Code, HasP2Code(Result, Case.Code));
		TestTrue(TEXT("Field location present"), !Result.Diagnostics.IsEmpty() && !Result.Diagnostics[0].JsonPath.IsEmpty());
	}
	const auto Recovery = AutoBlueprint::ValidateLayoutJson(Valid, Layout);
	TestTrue(TEXT("Failure does not pollute next validation"), Recovery.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP2ReadOnlyTest, "AutoBlueprint.P2.ReadOnlyConflicts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAutoBlueprintP2ReadOnlyTest::RunTest(const FString& Parameters)
{
	const FString Name = P2Target();
	// Only the test fixture creates a package; the API under test must leave it untouched.
	TStrongObjectPtr<UPackage> Package(CreatePackage(*Name));
	const bool bDirty = Package->IsDirty();
	auto Result = UAutoBlueprintLibrary::ValidateLayout(P2Json(Name));
	TestTrue(TEXT("Unsaved package rejected"), HasP2Code(Result, TEXT("ASSET_ALREADY_EXISTS")));
	TestTrue(TEXT("Package identity preserved"), FindPackage(nullptr, *Name) == Package.Get());
	TestEqual(TEXT("Dirty flag preserved"), Package->IsDirty(), bDirty);
	TestFalse(TEXT("No file saved"), FPackageName::DoesPackageExist(Name));
	for (const TCHAR* Path : { TEXT("../outside.json"), TEXT("/tmp/layout.json"), TEXT("Content/UI/layout.json"), TEXT("UIBlueprintLayouts/../outside.json") })
	{
		TestTrue(TEXT("File boundary"), HasP2Code(UAutoBlueprintLibrary::ValidateLayoutFile(Path), TEXT("INVALID_LAYOUT_PATH")));
	}
	TestTrue(TEXT("Missing file"), HasP2Code(UAutoBlueprintLibrary::ValidateLayoutFile(TEXT("UIBlueprintLayouts/DoesNotExist_P2.json")), TEXT("LAYOUT_READ_FAILED")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP2ExamplesTest, "AutoBlueprint.P2.Examples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAutoBlueprintP2ExamplesTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* File : { TEXT("minimal.layout.json"), TEXT("all_types.layout.json") })
	{
		FString Json;
		FAutoBlueprintValidationResult Read;
		TestTrue(TEXT("Read bundled sample"), AutoBlueprint::ReadLayoutFile(FString(TEXT("Plugins/AutoBlueprint/Examples/")) + File, Json, Read));
		// Avoid conflicts with assets a user may already have generated from the samples.
		Json = Json.Replace(TEXT("/Game/UI/AutoBlueprintTests/WBP_P2_Example"), *P2Target());
		const auto Result = UAutoBlueprintLibrary::ValidateLayout(Json);
		TestTrue(File, Result.bValid);
		for (const auto& D : Result.Diagnostics) { AddInfo(D.Code + TEXT(": ") + D.JsonPath + TEXT(": ") + D.Message); }
	}
	TestTrue(TEXT("Semantic slot error"), HasP2Code(UAutoBlueprintLibrary::ValidateLayoutFile(TEXT("Plugins/AutoBlueprint/Examples/invalid_slot.layout.json")), TEXT("INVALID_SLOT")));
	TestTrue(TEXT("Semantic duplicate name"), HasP2Code(UAutoBlueprintLibrary::ValidateLayoutFile(TEXT("Plugins/AutoBlueprint/Examples/duplicate_name.layout.json")), TEXT("DUPLICATE_WIDGET_NAME")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP2ResourceTest, "AutoBlueprint.P2.ResourcesAndDisk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAutoBlueprintP2ResourceTest::RunTest(const FString& Parameters)
{
	const FString Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString RelativeDir = TEXT("Content/UI/AutoBlueprintTests/P2_") + Token;
	const FString Directory = FPaths::Combine(FPaths::ProjectDir(), RelativeDir);
	IFileManager::Get().MakeDirectory(*Directory, true);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Directory, false, true); };
	const FString Source = RelativeDir + TEXT("/source.png");
	const FString SourceFull = FPaths::Combine(FPaths::ProjectDir(), Source);
	const FString Target = P2Target();
	const FString TextureTarget = TEXT("/Game/UI/AutoBlueprintTests/P2_") + Token + TEXT("/T_Image");
	const FString Resource = TEXT("\"resources\":{\"pic\":{\"source_file\":\"") + Source
		+ TEXT("\",\"package_path\":\"") + TextureTarget + TEXT("\"}},\"root\":");
	const FString Json = P2Json(Target).Replace(TEXT("\"root\":"), *Resource);
	TestTrue(TEXT("Write test LFS fixture"), FFileHelper::SaveStringToFile(TEXT("version https://git-lfs.github.com/spec/v1\noid sha256:test\nsize 1\n"), *SourceFull, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("LFS pointer rejected"), HasP2Code(UAutoBlueprintLibrary::ValidateLayout(Json), TEXT("LFS_SOURCE_NOT_MATERIALIZED")));
	TestTrue(TEXT("Write corrupt image fixture"), FFileHelper::SaveStringToFile(TEXT("broken image"), *SourceFull));
	TestTrue(TEXT("Corrupt image rejected"), HasP2Code(UAutoBlueprintLibrary::ValidateLayout(Json), TEXT("INVALID_IMAGE")));
	TestNull(TEXT("No target package created"), FindPackage(nullptr, *Target));
	TestNull(TEXT("No import package created"), FindPackage(nullptr, *TextureTarget));
	// Disk-only conflict: deliberately not a serialized asset; the preflight must reject
	// even an occupied target filename, without loading or rewriting it.
	FString Filename;
	TestTrue(TEXT("Mounted target"), FPackageName::TryConvertLongPackageNameToFilename(TextureTarget, Filename, FPackageName::GetAssetPackageExtension()));
	const FString Sentinel = TEXT("P2 test owned sentinel");
	TestTrue(TEXT("Write occupied file fixture"), FFileHelper::SaveStringToFile(Sentinel, *Filename));
	TestTrue(TEXT("Disk target conflict"), HasP2Code(UAutoBlueprintLibrary::ValidateLayout(P2Json(TextureTarget)), TEXT("ASSET_ALREADY_EXISTS")));
	FString After;
	FFileHelper::LoadFileToString(After, *Filename);
	TestEqual(TEXT("Occupied file unchanged"), After, Sentinel);
	const FString MissingResource = TEXT("\"resources\":{\"pic\":{\"object_path\":\"/Game/UI/P2_Missing_") + Token + TEXT(".P2_Missing\"}},\"root\":");
	TestTrue(TEXT("Missing existing texture"), HasP2Code(UAutoBlueprintLibrary::ValidateLayout(P2Json(Target).Replace(TEXT("\"root\":"), *MissingResource)), TEXT("MISSING_RESOURCE")));
	return true;
}

#endif
