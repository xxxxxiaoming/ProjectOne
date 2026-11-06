#include "Assets/WidgetBlueprintWriter.h"

#include "AutoBlueprintLog.h"
#include "AutoBlueprintTypes.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
	void AddWriterError(AutoBlueprint::FAutoBlueprintWidgetWriteResult& Result, const UWidgetBlueprint& Blueprint,
		const TCHAR* Code, const FString& Message)
	{
		FAutoBlueprintDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.AssetPath = Blueprint.GetPathName();
		Diagnostic.Message = Message;
	}
}

AutoBlueprint::FAutoBlueprintWidgetWriteResult AutoBlueprint::CompileAndSaveWidgetBlueprint(UWidgetBlueprint& Blueprint, bool bSave)
{
	FAutoBlueprintWidgetWriteResult Result;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
	FCompilerResultsLog CompilerLog;
	CompilerLog.bSilentMode = true;
	FKismetEditorUtilities::CompileBlueprint(&Blueprint, EBlueprintCompileOptions::None, &CompilerLog);

	for (const TSharedRef<FTokenizedMessage>& Message : CompilerLog.Messages)
	{
		FAutoBlueprintDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.AssetPath = Blueprint.GetPathName();
		Diagnostic.Message = Message->ToText().ToString();
		switch (Message->GetSeverity())
		{
		case EMessageSeverity::Error:
			Diagnostic.Code = TEXT("BLUEPRINT_COMPILE_ERROR");
			Diagnostic.Severity = EAutoBlueprintDiagnosticSeverity::Error;
			break;
		case EMessageSeverity::Warning:
		case EMessageSeverity::PerformanceWarning:
			Diagnostic.Code = TEXT("BLUEPRINT_COMPILE_WARNING");
			Diagnostic.Severity = EAutoBlueprintDiagnosticSeverity::Warning;
			break;
		default:
			Diagnostic.Code = TEXT("BLUEPRINT_COMPILE_INFO");
			Diagnostic.Severity = EAutoBlueprintDiagnosticSeverity::Info;
			break;
		}
	}

	Result.bCompiled = CompilerLog.NumErrors == 0 && Blueprint.GeneratedClass
		&& (Blueprint.Status == BS_UpToDate || Blueprint.Status == BS_UpToDateWithWarnings);
	if (!Result.bCompiled)
	{
		AddWriterError(Result, Blueprint, TEXT("BLUEPRINT_COMPILE_FAILED"), TEXT("Compilation failed. The new Widget Blueprint has not been saved."));
		return Result;
	}
	if (!bSave)
	{
		// A successful in-memory compile does not imply that the package was saved.
		return Result;
	}

	UPackage* Package = Blueprint.GetOutermost();
	const FString PackageName = Package->GetName();
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
	{
		AddWriterError(Result, Blueprint, TEXT("INVALID_PACKAGE_PATH"), TEXT("Cannot resolve the package to a project asset filename."));
		return Result;
	}

	// Recheck after compilation as editor callbacks may have created a disk asset.
	if (FPackageName::DoesPackageExist(PackageName) || IFileManager::Get().FileExists(*Filename))
	{
		AddWriterError(Result, Blueprint, TEXT("ASSET_ALREADY_EXISTS"), TEXT("A disk asset appeared before saving. CreateOnly refuses to replace it."));
		return Result;
	}
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
	{
		AddWriterError(Result, Blueprint, TEXT("PACKAGE_SAVE_FAILED"), TEXT("Cannot create the target asset directory: ") + FPaths::GetPath(Filename));
		return Result;
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GWarn;
	if (!UPackage::SavePackage(Package, &Blueprint, *Filename, SaveArgs))
	{
		AddWriterError(Result, Blueprint, TEXT("PACKAGE_SAVE_FAILED"), TEXT("Unreal failed to save the new package: ") + Filename);
		return Result;
	}

	Result.bSaved = true;
	// Preserve this evidence even if the calling Lua script throws after the save.
	UE_LOG(LogAutoBlueprint, Log, TEXT("Saved package: %s"), *PackageName);
	return Result;
}
