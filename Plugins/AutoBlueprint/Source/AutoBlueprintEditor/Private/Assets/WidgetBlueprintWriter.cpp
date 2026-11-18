#include "Assets/WidgetBlueprintWriter.h"

#include "Assets/CreateOnlyAssetWriter.h"
#include "Blueprint/UserWidget.h"
#include "AutoBlueprintTypes.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/Package.h"
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

AutoBlueprint::FAutoBlueprintWidgetWriteResult AutoBlueprint::CompileAndSaveWidgetBlueprint(UWidgetBlueprint& Blueprint, bool bSave, const FVector2D* DesignSize)
{
	FAutoBlueprintWidgetWriteResult Result;
	FCompilerResultsLog CompilerLog;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
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
	if (DesignSize)
	{
		// Designer size belongs to the generated UserWidget CDO, not the Blueprint or a preview instance.
		UUserWidget* Defaults = Cast<UUserWidget>(Blueprint.GeneratedClass->GetDefaultObject());

		if (!Defaults)
		{
			Result.bCompiled = false;
			AddWriterError(Result, Blueprint, TEXT("BLUEPRINT_COMPILE_FAILED"), TEXT("The generated class has no native UserWidget defaults. The asset is not saved."));
			return Result;
		}

		Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
		Defaults->DesignTimeSize = *DesignSize;
		Blueprint.MarkPackageDirty();
	}
	if (!bSave)
	{
		// A successful in-memory compile does not imply that the package was saved.
		return Result;
	}

	FString Code;
	FString Message;

	Result.bSaved = SaveCreatedAsset(Blueprint, Code, Message);
	if (!Result.bSaved)
	{
		AddWriterError(Result, Blueprint, *Code, Message);
	}

	return Result;
}
