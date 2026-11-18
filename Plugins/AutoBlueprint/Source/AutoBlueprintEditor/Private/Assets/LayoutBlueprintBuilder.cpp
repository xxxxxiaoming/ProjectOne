#include "Assets/LayoutBlueprintBuilder.h"

#include "Assets/WidgetBlueprintWriter.h"
#include "Assets/CreateOnlyAssetWriter.h"
#include "Assets/ResolvedResources.h"
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

	FAutoBlueprintGenerationResult GenerateLayoutBlueprint(const FString& Json, const FAutoBlueprintBuildOptions& Options, bool bAllowResources)
	{
		FLayoutSpec Layout;
		FAutoBlueprintValidationResult Validation = ValidateLayoutJson(Json, Layout);
		FAutoBlueprintGenerationResult Task;
		FAutoBlueprintBuildResult& Result = Task.Widget;
		FResolvedResources Resources;

		Result.TargetAssetPath = Validation.PlannedAssetPath;
		Result.Diagnostics = MoveTemp(Validation.Diagnostics);
		if (!Validation.bValid || !ValidateLayoutBuildCapabilities(Layout, Result.Diagnostics))
		{
			return Task;
		}

		if (!bAllowResources && !Layout.Resources.IsEmpty())
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("RESOURCE_TASK_REQUIRED"),
				TEXT("Layouts with resources require GenerateFromLayout/GenerateFromLayoutFile, which report each resource separately."));
			return Task;
		}
		if (!Resources.Resolve(Layout, Task, Options.bSave))
		{
			return Task;
		}

		// Resource loading during validation can invoke editor callbacks. Recheck immediately before creation.
		const FString Filename = FPackageName::LongPackageNameToFilename(Layout.PackagePath, FPackageName::GetAssetPackageExtension());

		if (FindPackage(nullptr, *Layout.PackagePath) || FPackageName::DoesPackageExist(Layout.PackagePath)
			|| IFileManager::Get().FileExists(*Filename))
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_ALREADY_EXISTS"), TEXT("The target appeared during validation. CreateOnly refuses to replace it."));
			return Task;
		}

		TStrongObjectPtr<UPackage> Package(CreatePackage(*Layout.PackagePath));

		if (!Package.IsValid())
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Cannot create the new package."));
			return Task;
		}

		Result.CreatedPackageName = Package->GetName();

		TStrongObjectPtr<UWidgetBlueprint> Blueprint(Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(), Package.Get(), FName(*FPackageName::GetLongPackageAssetName(Layout.PackagePath)),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(),
			FName(TEXT("AutoBlueprint")))));

		if (!Blueprint.IsValid())
		{
			LayoutBlueprintBuild::AddFailure(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Unreal could not create the Widget Blueprint. The new package remains unsaved."));
			return Task;
		}

		Blueprint->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		Result.bAssetCreated = true;
		FAssetRegistryModule::AssetCreated(Blueprint.Get());
		Blueprint->MarkPackageDirty();

		if (!BuildLayoutWidgetTree(*Blueprint.Get(), Layout, Resources, Result.Diagnostics))
		{
			return Task;
		}

		FAutoBlueprintWidgetWriteResult WriteResult = CompileAndSaveWidgetBlueprint(*Blueprint.Get(), false, &Layout.DesignSize);

		Result.bCompiled = WriteResult.bCompiled;

		Result.Diagnostics.Append(MoveTemp(WriteResult.Diagnostics));
		if (!Result.bCompiled)
		{
			return Task;
		}
		if (Options.bSave)
		{
			// A Widget package must never be saved pointing at a new texture that failed to save.
			if (!Resources.SaveImports(Task))
			{
				LayoutBlueprintBuild::AddFailure(Result, TEXT("RESOURCE_SAVE_FAILED"), TEXT("A texture could not be saved. The compiled Widget remains unsaved; inspect resource results."));
				return Task;
			}

			FString Code;
			FString Message;

			Result.bSaved = SaveCreatedAsset(*Blueprint.Get(), Code, Message);
			if (!Result.bSaved)
			{
				LayoutBlueprintBuild::AddFailure(Result, *Code, Message);
				return Task;
			}
		}

		Result.bSuccess = true;
		Task.bSuccess = true;
		return Task;
	}

	FAutoBlueprintBuildResult BuildLayoutBlueprint(const FString& Json, const FAutoBlueprintBuildOptions& Options)
	{
		FAutoBlueprintGenerationResult Task = GenerateLayoutBlueprint(Json, Options, false);

		Task.Widget.Diagnostics.Append(MoveTemp(Task.Diagnostics));
		return MoveTemp(Task.Widget);
	}
}
