#include "Diagnostics/BuildReport.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void AutoBlueprint::WriteBuildReport(FAutoBlueprintBuildResult& Result, const FAutoBlueprintBuildOptions& Options)
{
	if (!Options.bWriteReport)
	{
		return;
	}

	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Diagnostics;
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AutoBlueprint/Reports"));
	const FString Filename = Directory / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));

	Report->SetNumberField(TEXT("report_version"), 1);
	Report->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
	Report->SetBoolField(TEXT("save_requested"), Options.bSave);
	Report->SetBoolField(TEXT("success"), Result.bSuccess);
	Report->SetBoolField(TEXT("asset_created"), Result.bAssetCreated);
	Report->SetBoolField(TEXT("compiled"), Result.bCompiled);
	Report->SetBoolField(TEXT("saved"), Result.bSaved);
	Report->SetStringField(TEXT("target_asset_path"), Result.TargetAssetPath);
	Report->SetStringField(TEXT("created_package_name"), Result.CreatedPackageName);

	for (const FAutoBlueprintDiagnostic& Diagnostic : Result.Diagnostics)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();

		Item->SetStringField(TEXT("severity"), Diagnostic.Severity == EAutoBlueprintDiagnosticSeverity::Error ? TEXT("error")
			: Diagnostic.Severity == EAutoBlueprintDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
		Item->SetStringField(TEXT("code"), Diagnostic.Code);
		Item->SetStringField(TEXT("json_path"), Diagnostic.JsonPath);
		Item->SetStringField(TEXT("widget_name"), Diagnostic.WidgetName);
		Item->SetStringField(TEXT("asset_path"), Diagnostic.AssetPath);
		Item->SetStringField(TEXT("message"), Diagnostic.Message);
		Diagnostics.Add(MakeShared<FJsonValueObject>(Item));
	}

	FString Json;

	Report->SetArrayField(TEXT("diagnostics"), Diagnostics);
	if (FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
		&& IFileManager::Get().MakeDirectory(*Directory, true)
		&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.ReportFilePath = Filename;
		return;
	}

	FAutoBlueprintDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();

	Diagnostic.Severity = EAutoBlueprintDiagnosticSeverity::Warning;
	Diagnostic.Code = TEXT("REPORT_WRITE_FAILED");
	Diagnostic.AssetPath = Result.TargetAssetPath;
	Diagnostic.Message = TEXT("Could not write the JSON report: ") + Filename + TEXT(". Asset outcomes are unchanged.");
}
