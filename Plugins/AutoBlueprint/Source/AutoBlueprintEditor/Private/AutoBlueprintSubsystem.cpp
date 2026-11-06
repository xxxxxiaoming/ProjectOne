#include "AutoBlueprintSubsystem.h"

#include "Assets/WidgetBlueprintWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AutoBlueprintEditorContext.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Templates/UnrealTemplate.h"
#include "Scripting/EditorLuaRunner.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"
#include "Widgets/SmokeTestWidgetTree.h"

namespace
{
	void AddError(FAutoBlueprintBuildResult& Result, const TCHAR* Code, const FString& Message)
	{
		FAutoBlueprintDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.AssetPath = Result.TargetAssetPath;
		Diagnostic.Message = Message;
	}

	bool ValidateRequest(const FAutoBlueprintSmokeTestRequest& Request, FAutoBlueprintBuildResult& Result)
	{
		FText Reason;
		const FString& PackagePath = Request.PackagePath;
		if (!PackagePath.StartsWith(TEXT("/Game/UI/AutoBlueprintTests/"), ESearchCase::CaseSensitive)
			|| !FPackageName::IsValidLongPackageName(PackagePath, false, &Reason))
		{
			AddError(Result, TEXT("INVALID_PACKAGE_PATH"),
				TEXT("Use a new long package name below /Game/UI/AutoBlueprintTests/ without an extension. ") + Reason.ToString());
			return false;
		}

		const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
		if (!FPackageName::IsValidObjectPath(ObjectPath, &Reason))
		{
			AddError(Result, TEXT("INVALID_PACKAGE_PATH"), Reason.ToString());
			return false;
		}
		Result.TargetAssetPath = ObjectPath;

		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, Filename, FPackageName::GetAssetPackageExtension()))
		{
			AddError(Result, TEXT("INVALID_PACKAGE_PATH"), TEXT("The target package is not mounted in the project."));
			return false;
		}
		// A loaded package may contain unsaved work even when no .uasset exists yet.
		if (FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath)
			|| IFileManager::Get().FileExists(*Filename))
		{
			AddError(Result, TEXT("ASSET_ALREADY_EXISTS"), TEXT("The target package already exists in memory or on disk. Choose a new name."));
			return false;
		}
		if (Request.TitleText.IsEmpty() || Request.ButtonText.IsEmpty()
			|| Request.TitleText.Len() > 256 || Request.ButtonText.Len() > 256)
		{
			AddError(Result, TEXT("INVALID_TEXT"), TEXT("P0 title and button text must each contain 1 to 256 characters."));
			return false;
		}
		return true;
	}
}

FAutoBlueprintBuildResult UAutoBlueprintSubsystem::BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request)
{
	FAutoBlueprintBuildResult Result;
	FString ContextCode;
	FString ContextMessage;
	if (!AutoBlueprint::CheckEditorContext(ContextCode, ContextMessage))
	{
		AddError(Result, *ContextCode, ContextMessage);
		return Result;
	}
	if (bBuildInProgress)
	{
		AddError(Result, TEXT("EDITOR_BUSY"), TEXT("Another AutoBlueprint build is already in progress."));
		return Result;
	}
	TGuardValue<bool> BuildGuard(bBuildInProgress, true);
	if (!ValidateRequest(Request, Result))
	{
		return Result;
	}

	TStrongObjectPtr<UPackage> Package(CreatePackage(*Request.PackagePath));
	if (!Package.IsValid())
	{
		AddError(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Cannot create the new package."));
		return Result;
	}
	Result.CreatedPackageName = Package->GetName();

	// Use UE's blueprint creation/compiler path and native UMG classes only.
	TStrongObjectPtr<UWidgetBlueprint> Blueprint(Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(), Package.Get(), FName(*FPackageName::GetLongPackageAssetName(Request.PackagePath)),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(),
		FName(TEXT("AutoBlueprint")))));
	if (!Blueprint.IsValid())
	{
		AddError(Result, TEXT("ASSET_CREATE_FAILED"), TEXT("Unreal could not create the Widget Blueprint."));
		return Result;
	}
	Blueprint->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	Result.bAssetCreated = true;
	FAssetRegistryModule::AssetCreated(Blueprint.Get());
	Blueprint->MarkPackageDirty();

	if (!AutoBlueprint::BuildSmokeTestWidgetTree(*Blueprint.Get(), Request))
	{
		AddError(Result, TEXT("WIDGET_TREE_BUILD_FAILED"), TEXT("Cannot construct the fixed P0 widget tree. The new asset remains unsaved."));
		return Result;
	}
	AutoBlueprint::FAutoBlueprintWidgetWriteResult WriteResult =
		AutoBlueprint::CompileAndSaveWidgetBlueprint(*Blueprint.Get(), Request.bSave);
	Result.bCompiled = WriteResult.bCompiled;
	Result.bSaved = WriteResult.bSaved;
	Result.Diagnostics.Append(MoveTemp(WriteResult.Diagnostics));
	// If Request.bSave is false, then Result.bCompiled is true -> Result.bSuccess is true.
	// If Request.bSave is true, then Result.bCompiled + Result.bSaved both true -> Result.bSuccess is true.
	Result.bSuccess = Result.bCompiled && (!Request.bSave || Result.bSaved);
	return Result;
}

FAutoBlueprintBatchBuildResult UAutoBlueprintSubsystem::GenerateBatch(const FAutoBlueprintBatchRequest& Request)
{
	// TODO: implement batch scheduling and aggregation after general layout requests are available.
	// Do not loop over BuildSmokeTestWidget here: this API must remain free of asset side effects.
	FAutoBlueprintDiagnostic Diagnostic;
	Diagnostic.Code = TEXT("NOT_IMPLEMENTED");
	Diagnostic.Message = TEXT("GenerateBatch is reserved for future implementation. No assets were created or saved.");

	FAutoBlueprintBatchBuildResult Result;
	Result.Diagnostics.Add(Diagnostic);
	Result.Results.SetNum(Request.Requests.Num());
	for (FAutoBlueprintBuildResult& Item : Result.Results)
	{
		// No path validation or build ran; creation/compile/save fields retain their defaults.
		Item.Diagnostics.Add(Diagnostic);
	}
	return Result;
}

void UAutoBlueprintSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bAcceptScriptRequests = true;
}

void UAutoBlueprintSubsystem::Deinitialize()
{
	// Tasks are synchronous; no VM or callback is retained by the subsystem.
	bAcceptScriptRequests = false;
	Super::Deinitialize();
}

FAutoBlueprintLuaRunResult UAutoBlueprintSubsystem::RunEditorScript(const FAutoBlueprintLuaRunRequest& Request)
{
	FAutoBlueprintLuaRunResult Result;
	if (!AutoBlueprint::CheckEditorContext(Result.Code, Result.Message))
	{
		return Result;
	}
	if (!bAcceptScriptRequests)
	{
		Result.Code = TEXT("SUBSYSTEM_UNAVAILABLE");
		Result.Message = TEXT("AutoBlueprint is not accepting tool tasks during initialization or shutdown.");
		return Result;
	}
	if (bScriptInProgress || bBuildInProgress)
	{
		Result.Code = TEXT("EDITOR_BUSY");
		Result.Message = TEXT("Another AutoBlueprint script or build is in progress. Nested script execution is not supported.");
		return Result;
	}
	TGuardValue<bool> ScriptGuard(bScriptInProgress, true);
	FAutoBlueprintEditorLuaRunner Runner;
	return Runner.Run(Request);
}
