#include "Diagnostics/BuildReport.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace AutoBlueprint
{
	namespace OutcomeReport
	{
		TArray<TSharedPtr<FJsonValue>> DiagnosticsJson(const TArray<FAutoBlueprintDiagnostic>& Values)
		{
			TArray<TSharedPtr<FJsonValue>> Diagnostics;

			for (const FAutoBlueprintDiagnostic& Diagnostic : Values)
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

			return Diagnostics;
		}

		TSharedRef<FJsonObject> WidgetJson(const FAutoBlueprintBuildResult& Result)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();

			Object->SetBoolField(TEXT("success"), Result.bSuccess);
			Object->SetBoolField(TEXT("asset_created"), Result.bAssetCreated);
			Object->SetBoolField(TEXT("compiled"), Result.bCompiled);
			Object->SetBoolField(TEXT("saved"), Result.bSaved);
			Object->SetStringField(TEXT("target_asset_path"), Result.TargetAssetPath);
			Object->SetStringField(TEXT("created_package_name"), Result.CreatedPackageName);
			Object->SetArrayField(TEXT("diagnostics"), DiagnosticsJson(Result.Diagnostics));
			return Object;
		}

		void Save(const TSharedRef<FJsonObject>& Report, const FAutoBlueprintBuildOptions& Options,
			FString& ReportFilePath, TArray<FAutoBlueprintDiagnostic>& Diagnostics)
		{
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AutoBlueprint/Reports"));
			const FString Filename = Directory / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
			FString Json;

			Report->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
			Report->SetBoolField(TEXT("save_requested"), Options.bSave);
			if (FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
				&& IFileManager::Get().MakeDirectory(*Directory, true)
				&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				ReportFilePath = Filename;
				return;
			}

			FAutoBlueprintDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();

			Diagnostic.Severity = EAutoBlueprintDiagnosticSeverity::Warning;
			Diagnostic.Code = TEXT("REPORT_WRITE_FAILED");
			Diagnostic.Message = TEXT("Could not write the JSON report: ") + Filename + TEXT(". Asset outcomes are unchanged.");
		}
	}

	void WriteBuildReport(FAutoBlueprintBuildResult& Result, const FAutoBlueprintBuildOptions& Options)
	{
		if (!Options.bWriteReport)
		{
			return;
		}

		const TSharedRef<FJsonObject> Report = OutcomeReport::WidgetJson(Result);

		Report->SetNumberField(TEXT("report_version"), 1);
		OutcomeReport::Save(Report, Options, Result.ReportFilePath, Result.Diagnostics);
	}

	void WriteGenerationReport(FAutoBlueprintGenerationResult& Result, const FAutoBlueprintBuildOptions& Options)
	{
		if (!Options.bWriteReport)
		{
			return;
		}

		const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Resources;

		Report->SetNumberField(TEXT("report_version"), 2);
		Report->SetBoolField(TEXT("success"), Result.bSuccess);
		Report->SetObjectField(TEXT("widget"), OutcomeReport::WidgetJson(Result.Widget));
		Report->SetArrayField(TEXT("diagnostics"), OutcomeReport::DiagnosticsJson(Result.Diagnostics));
		for (const FAutoBlueprintResourceResult& Item : Result.Resources)
		{
			const TSharedRef<FJsonObject> Resource = MakeShared<FJsonObject>();

			Resource->SetStringField(TEXT("alias"), Item.Alias);
			Resource->SetStringField(TEXT("source_file"), Item.SourceFile);
			Resource->SetStringField(TEXT("target_asset_path"), Item.TargetAssetPath);
			Resource->SetStringField(TEXT("created_package_name"), Item.CreatedPackageName);
			Resource->SetBoolField(TEXT("attempted"), Item.bAttempted);
			Resource->SetBoolField(TEXT("reused"), Item.bReused);
			Resource->SetBoolField(TEXT("imported"), Item.bImported);
			Resource->SetBoolField(TEXT("saved"), Item.bSaved);
			Resource->SetBoolField(TEXT("success"), Item.bSuccess);
			Resource->SetArrayField(TEXT("diagnostics"), OutcomeReport::DiagnosticsJson(Item.Diagnostics));
			Resources.Add(MakeShared<FJsonValueObject>(Resource));
		}

		Report->SetArrayField(TEXT("resources"), Resources);
		OutcomeReport::Save(Report, Options, Result.ReportFilePath, Result.Diagnostics);
	}
}
