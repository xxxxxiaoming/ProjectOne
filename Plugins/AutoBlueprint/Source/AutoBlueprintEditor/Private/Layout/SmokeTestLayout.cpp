#include "Layout/SmokeTestLayout.h"

#include "AutoBlueprintTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString AutoBlueprint::MakeSmokeTestLayoutJson(const FAutoBlueprintSmokeTestRequest& Request)
{
	// This is fixture data only. P0 and file/JSON builds use the same validation, tree and writer.
	const FString Fixture = TEXT(R"json({
		"schema_version": 1,
		"asset": {"package_path": ""},
		"design": {"width": 1080, "height": 1920},
		"root": {"name": "RootCanvas", "type": "CanvasPanel", "is_variable": false, "children": [
			{"name": "TXT_Title", "type": "TextBlock", "is_variable": true,
			 "slot": {"type": "Canvas", "offsets": [64,64,512,48]},
			 "properties": {"text": "", "visibility": "HitTestInvisible"}},
			{"name": "BTN_Confirm", "type": "Button", "is_variable": true,
			 "slot": {"type": "Canvas", "offsets": [64,160,240,64]}, "children": [
				{"name": "TXT_Confirm", "type": "TextBlock", "is_variable": true,
				 "slot": {"type": "Button", "padding": [12,8,12,8],
					"horizontal_alignment": "Center", "vertical_alignment": "Center"},
				 "properties": {"text": "", "visibility": "HitTestInvisible"}}
			 ]}
		]}
	})json");
	TSharedPtr<FJsonObject> Object;
	FString Json;

	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Fixture), Object) || !Object.IsValid())
	{
		return Json;
	}

	const TArray<TSharedPtr<FJsonValue>>& Children = Object->GetObjectField(TEXT("root"))->GetArrayField(TEXT("children"));

	Object->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Request.PackagePath);
	Children[0]->AsObject()->GetObjectField(TEXT("properties"))->SetStringField(TEXT("text"), Request.TitleText);
	Children[1]->AsObject()->GetArrayField(TEXT("children"))[0]->AsObject()
		->GetObjectField(TEXT("properties"))->SetStringField(TEXT("text"), Request.ButtonText);
	FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	return Json;
}
