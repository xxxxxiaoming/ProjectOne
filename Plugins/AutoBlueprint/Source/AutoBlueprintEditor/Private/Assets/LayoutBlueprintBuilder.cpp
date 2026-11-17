#include "Assets/LayoutBlueprintBuilder.h"

#include "Assets/WidgetBlueprintWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Layout/LayoutValidation.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"
#include "Widgets/WidgetTreeBuilder.h"

namespace AutoBlueprint
{
	namespace LayoutBlueprintBuild
	{
		void AddFailure(FAutoBlueprintBuildResult& Result, const TCHAR* Code, const FString& Message)
		{
			FAutoBlueprintDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();

			Diagnostic.Code = Code;
			Diagnostic.AssetPath = Result.TargetAssetPath;
			Diagnostic.Message = Message;
		}
	}

	FAutoBlueprintBuildResult BuildLayoutBlueprint(const FString& Json, const FAutoBlueprintBuildOptions& Options)
	{
		FLayoutSpec Layout;
		FAutoBlueprintValidationResult Validation = ValidateLayoutJson(Json, Layout);
		FAutoBlueprintBuildResult Result;

		Result.TargetAssetPath = Validation.PlannedAssetPath;
		Result.Diagnostics = MoveTemp(Validation.Diagnostics);
		if (!Validation.bValid || !ValidateLayoutBuildCapabilities(Layout, Result.Diagnostics))
		{
			return Result;
		}

		// Resource loading during validation can invoke editor callbacks. Recheck immediately before creation.
		const FString Filename = FPackageName::LongPackageNameToFilename(Layout.PackagePath, FPackageName::GetAssetPackageExtension());

		if (FindPackage(nullptr, *Layout.PackagePath) || FPackageName::DoesPackageExist(Layout.PackagePath)
			|| IFileManager::Get().FileExists(*Filename))
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_ALREADY_EXISTS"), TEXT("The target appeared during validation. CreateOnly refuses to replace it."));
			return Result;
		}

		TStrongObjectPtr<UPackage> Package(CreatePackage(*Layout.PackagePath));

		if (!Package.IsValid())
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Cannot create the new package."));
			return Result;
		}

		Result.CreatedPackageName = Package->GetName();

		TStrongObjectPtr<UWidgetBlueprint> Blueprint(Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(), Package.Get(), FName(*FPackageName::GetLongPackageAssetName(Layout.PackagePath)),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(),
			FName(TEXT("AutoBlueprint")))));

		if (!Blueprint.IsValid())
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Unreal could not create the Widget Blueprint. The new package remains unsaved."));
			return Result;
		}

		Blueprint->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		Result.bAssetCreated = true;
		FAssetRegistryModule::AssetCreated(Blueprint.Get());
		Blueprint->MarkPackageDirty();

		if (!BuildLayoutWidgetTree(*Blueprint.Get(), Layout, Result.Diagnostics))
		{
			return Result;
		}

		FAutoBlueprintWidgetWriteResult WriteResult = CompileAndSaveWidgetBlueprint(*Blueprint.Get(), Options.bSave, &Layout.DesignSize);

		Result.bCompiled = WriteResult.bCompiled;
		Result.bSaved = WriteResult.bSaved;
		Result.Diagnostics.Append(MoveTemp(WriteResult.Diagnostics));
		Result.bSuccess = Result.bCompiled && (!Options.bSave || Result.bSaved);
		return Result;
	}
}
