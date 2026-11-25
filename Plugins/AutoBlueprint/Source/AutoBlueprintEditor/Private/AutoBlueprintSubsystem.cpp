#include "AutoBlueprintSubsystem.h"

#include "Assets/LayoutBlueprintBuilder.h"
#include "AutoBlueprintEditorContext.h"
#include "Diagnostics/BuildReport.h"
#include "HAL/FileManager.h"
#include "Layout/LayoutValidation.h"
#include "Layout/SmokeTestLayout.h"
#include "Misc/PackageName.h"
#include "Scripting/EditorLuaRunner.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"

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
		RecordScriptOperation(TEXT("BuildSmokeTestWidget"), Result);
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		AddError(Result, TEXT("EDITOR_BUSY"), TEXT("An initialized, idle editor subsystem is required."));
		RecordScriptOperation(TEXT("BuildSmokeTestWidget"), Result);
		return Result;
	}

	if (!ValidateRequest(Request, Result))
	{
		RecordScriptOperation(TEXT("BuildSmokeTestWidget"), Result);
		return Result;
	}

	FAutoBlueprintBuildOptions Options;

	Options.bSave = Request.bSave;
	Options.bWriteReport = false;
	return BuildWidgetBlueprint(AutoBlueprint::MakeSmokeTestLayoutJson(Request), Options);
}

FAutoBlueprintBuildResult UAutoBlueprintSubsystem::BuildWidgetBlueprint(const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options)
{
	FAutoBlueprintBuildResult Result;
	FString ContextCode;
	FString ContextMessage;

	if (!AutoBlueprint::CheckEditorContext(ContextCode, ContextMessage))
	{
		AddError(Result, *ContextCode, ContextMessage);
		RecordScriptOperation(TEXT("BuildWidgetBlueprint"), Result);
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		AddError(Result, TEXT("EDITOR_BUSY"), TEXT("An initialized, idle editor subsystem is required."));
		RecordScriptOperation(TEXT("BuildWidgetBlueprint"), Result);
		return Result;
	}

	TGuardValue<bool> BuildGuard(bBuildInProgress, true);

	Result = AutoBlueprint::BuildLayoutBlueprint(LayoutJson, Options);
	AutoBlueprint::WriteBuildReport(Result, Options);
	RecordScriptOperation(TEXT("BuildWidgetBlueprint"), Result);
	return Result;
}

FAutoBlueprintBuildResult UAutoBlueprintSubsystem::BuildFromLayoutFile(const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options)
{
	FAutoBlueprintBuildResult Result;
	FString ContextCode;
	FString ContextMessage;

	if (!AutoBlueprint::CheckEditorContext(ContextCode, ContextMessage))
	{
		AddError(Result, *ContextCode, ContextMessage);
		RecordScriptOperation(TEXT("BuildFromLayoutFile"), Result);
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		AddError(Result, TEXT("EDITOR_BUSY"), TEXT("An initialized, idle editor subsystem is required."));
		RecordScriptOperation(TEXT("BuildFromLayoutFile"), Result);
		return Result;
	}

	TGuardValue<bool> BuildGuard(bBuildInProgress, true);
	FAutoBlueprintValidationResult ReadResult;
	FString Json;

	if (AutoBlueprint::ReadLayoutFile(ProjectRelativePath, Json, ReadResult))
	{
		Result = AutoBlueprint::BuildLayoutBlueprint(Json, Options);
	}
	else
	{
		Result.Diagnostics = MoveTemp(ReadResult.Diagnostics);
	}

	AutoBlueprint::WriteBuildReport(Result, Options);
	RecordScriptOperation(TEXT("BuildFromLayoutFile"), Result);
	return Result;
}

FAutoBlueprintGenerationResult UAutoBlueprintSubsystem::GenerateFromLayout(const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options)
{
	FAutoBlueprintGenerationResult Result;
	FString ContextCode;
	FString ContextMessage;

	if (!AutoBlueprint::CheckEditorContext(ContextCode, ContextMessage))
	{
		AddError(Result.Widget, *ContextCode, ContextMessage);
		RecordScriptOperation(TEXT("GenerateFromLayout"), Result);
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		AddError(Result.Widget, TEXT("EDITOR_BUSY"), TEXT("An initialized, idle editor subsystem is required."));
		RecordScriptOperation(TEXT("GenerateFromLayout"), Result);
		return Result;
	}

	TGuardValue<bool> BuildGuard(bBuildInProgress, true);

	Result = AutoBlueprint::GenerateLayoutBlueprint(LayoutJson, Options);
	AutoBlueprint::WriteGenerationReport(Result, Options);
	RecordScriptOperation(TEXT("GenerateFromLayout"), Result);
	return Result;
}

FAutoBlueprintGenerationResult UAutoBlueprintSubsystem::GenerateFromLayoutFile(const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options)
{
	FAutoBlueprintGenerationResult Result;
	FString ContextCode;
	FString ContextMessage;

	if (!AutoBlueprint::CheckEditorContext(ContextCode, ContextMessage))
	{
		AddError(Result.Widget, *ContextCode, ContextMessage);
		RecordScriptOperation(TEXT("GenerateFromLayoutFile"), Result);
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		AddError(Result.Widget, TEXT("EDITOR_BUSY"), TEXT("An initialized, idle editor subsystem is required."));
		RecordScriptOperation(TEXT("GenerateFromLayoutFile"), Result);
		return Result;
	}

	TGuardValue<bool> BuildGuard(bBuildInProgress, true);
	FAutoBlueprintValidationResult ReadResult;
	FString Json;

	if (AutoBlueprint::ReadLayoutFile(ProjectRelativePath, Json, ReadResult))
	{
		Result = AutoBlueprint::GenerateLayoutBlueprint(Json, Options);
	}
	else
	{
		Result.Diagnostics = MoveTemp(ReadResult.Diagnostics);
	}

	AutoBlueprint::WriteGenerationReport(Result, Options);
	RecordScriptOperation(TEXT("GenerateFromLayoutFile"), Result);
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
	ScriptOperations.Reset();
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

	ScriptOperations.Reset();
	Result = Runner.Run(Request);
	Result.Operations = MoveTemp(ScriptOperations);
	ScriptOperations.Reset();
	return Result;
}

FAutoBlueprintValidationResult UAutoBlueprintSubsystem::ValidateLayout(const FString& LayoutJson)
{
	FAutoBlueprintValidationResult Result;
	FAutoBlueprintDiagnostic Diagnostic;

	if (!AutoBlueprint::CheckEditorContext(Diagnostic.Code, Diagnostic.Message))
	{
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}

	if (!bAcceptScriptRequests || bBuildInProgress)
	{
		Diagnostic.Code = TEXT("EDITOR_BUSY");
		Diagnostic.Message = TEXT("Validation requires an initialized, idle editor subsystem.");
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}
	AutoBlueprint::FLayoutSpec Layout;
	return AutoBlueprint::ValidateLayoutJson(LayoutJson, Layout);
}

FAutoBlueprintValidationResult UAutoBlueprintSubsystem::ValidateLayoutFile(const FString& ProjectRelativePath)
{
	FAutoBlueprintValidationResult Result;
	FAutoBlueprintDiagnostic Diagnostic;

	if (!AutoBlueprint::CheckEditorContext(Diagnostic.Code, Diagnostic.Message))
	{
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}
	FString Json;

	if (!AutoBlueprint::ReadLayoutFile(ProjectRelativePath, Json, Result))
	{
		return Result;
	}
	return ValidateLayout(Json);
}

void UAutoBlueprintSubsystem::RecordScriptOperation(const TCHAR* Operation, const FAutoBlueprintBuildResult& Result)
{
	if (!IsInGameThread() || !bScriptInProgress)
	{
		return;
	}

	FAutoBlueprintGenerationResult Task;

	Task.bSuccess = Result.bSuccess;
	Task.Widget = Result;
	Task.ReportFilePath = Result.ReportFilePath;
	RecordScriptOperation(Operation, Task);
}

void UAutoBlueprintSubsystem::RecordScriptOperation(const TCHAR* Operation, const FAutoBlueprintGenerationResult& Result)
{
	if (!IsInGameThread() || !bScriptInProgress)
	{
		return;
	}

	FAutoBlueprintScriptOperation& Entry = ScriptOperations.AddDefaulted_GetRef();

	Entry.Operation = Operation;
	Entry.Result = Result;
}
