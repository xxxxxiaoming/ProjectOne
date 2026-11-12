#include "Layout/LayoutSchemaValidator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace AutoBlueprint
{
namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	using FValue = TSharedPtr<FJsonValue>;
	// Unity builds combine .cpp files; keep this helper distinct from the other validator.
	void AddLayoutSchemaError(FAutoBlueprintValidationResult& R, const FString& Code, const FString& Path, const FString& Message)
	{
		if (R.Diagnostics.Num() >= 64) { return; }
		FAutoBlueprintDiagnostic& D = R.Diagnostics.AddDefaulted_GetRef();
		D.Code = Code; D.JsonPath = Path; D.Message = Message;
	}
	bool Identifier(const FString& S)
	{
		if (S.IsEmpty() || S.Len() > 64) { return false; }
		for (int32 I = 0; I < S.Len(); ++I)
		{
			const TCHAR C = S[I];
			if (!(C == TEXT('_') || (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z'))
				|| (I > 0 && C >= TEXT('0') && C <= TEXT('9')))) { return false; }
		}
		return true;
	}

	FValue CloneJson(const FValue& V)
	{
		if (V->Type == EJson::Object)
		{
			FObject O = MakeShared<FJsonObject>();
			for (const auto& P : V->AsObject()->Values) { O->SetField(P.Key, CloneJson(P.Value)); }
			return MakeShared<FJsonValueObject>(O);
		}
		if (V->Type == EJson::Array)
		{
			TArray<FValue> A;
			for (const FValue& E : V->AsArray()) { A.Add(CloneJson(E)); }
			return MakeShared<FJsonValueArray>(A);
		}
		return V; // Scalars are immutable during validation.
	}

	// Interpreter for the checked-in schema's deliberately small keyword subset.
	// UE-specific relationships and asset inspection are handled after this structural pass.
	class FSchemaCheck
	{
	public:
		FObject Schema;
		FAutoBlueprintValidationResult& Result;
		FSchemaCheck(const FObject& InSchema, FAutoBlueprintValidationResult& InResult) : Schema(InSchema), Result(InResult) {}

		bool Check(const FValue& V, const FObject& Rule, const FString& Path, int32 Depth = 0)
		{
			if (Depth > 256 || !V.IsValid() || !Rule.IsValid()) { return Fail(TEXT("INVALID_SCHEMA"), Path, TEXT("Invalid schema or excessive depth.")); }
			FString Ref;
			if (Rule->TryGetStringField(TEXT("$ref"), Ref))
			{
				const FObject Defs = Schema->GetObjectField(TEXT("$defs"));
				return Check(V, Defs->GetObjectField(Ref.Mid(8)), Path, Depth + 1);
			}
			const TArray<FValue>* Alternatives = nullptr;
			if (Rule->TryGetArrayField(TEXT("oneOf"), Alternatives))
			{
				// Prefer the tagged node/slot variant, producing useful field-level diagnostics.
				if (V->Type == EJson::Object)
				{
					FString Tag;
					if (V->AsObject()->TryGetStringField(TEXT("type"), Tag))
					{
						for (const FValue& A : *Alternatives)
						{
							FObject Candidate = A->AsObject();
							FString CandidateRef;
							if (Candidate->TryGetStringField(TEXT("$ref"), CandidateRef)) { Candidate = Schema->GetObjectField(TEXT("$defs"))->GetObjectField(CandidateRef.Mid(8)); }
							const FObject* Properties = nullptr;
							if (Candidate->TryGetObjectField(TEXT("properties"), Properties) && (*Properties)->HasField(TEXT("type")))
							{
								const TArray<FValue>& Tags = (*Properties)->GetObjectField(TEXT("type"))->GetArrayField(TEXT("enum"));
								if (Tags[0]->AsString() == Tag)
								{
									const int32 FirstDiagnostic = Result.Diagnostics.Num();
									const bool bValid = Check(V, Candidate, Path, Depth + 1);
									FString Widget;
									if (V->AsObject()->TryGetStringField(TEXT("name"), Widget))
									{
										for (int32 I = FirstDiagnostic; I < Result.Diagnostics.Num(); ++I)
										{ if (Result.Diagnostics[I].WidgetName.IsEmpty()) { Result.Diagnostics[I].WidgetName = Widget; } }
									}
									return bValid;
								}
							}
						}
						return Fail(Path.EndsWith(TEXT(".slot")) ? TEXT("INVALID_SLOT") : TEXT("UNSUPPORTED_WIDGET"), Path + TEXT(".type"), TEXT("Unknown type: ") + Tag);
					}
				}
				int32 Matches = 0;
				for (const FValue& A : *Alternatives)
				{
					FAutoBlueprintValidationResult Trial;
					FSchemaCheck Checker(Schema, Trial);
					Matches += Checker.Check(V, A->AsObject(), Path, Depth + 1) ? 1 : 0;
				}
				return Matches == 1 || Fail(TEXT("INVALID_FIELD_TYPE"), Path, TEXT("Expected exactly one schema alternative (check required fields and exclusive resource forms)."));
			}
			const FString Type = Rule->GetStringField(TEXT("type"));
			const bool bType = (Type == TEXT("object") && V->Type == EJson::Object)
				|| (Type == TEXT("array") && V->Type == EJson::Array) || (Type == TEXT("string") && V->Type == EJson::String)
				|| (Type == TEXT("boolean") && V->Type == EJson::Boolean)
				|| ((Type == TEXT("number") || Type == TEXT("integer")) && V->Type == EJson::Number);
			if (!bType) { return Fail(TEXT("INVALID_FIELD_TYPE"), Path, TEXT("Expected ") + Type); }
			const TArray<FValue>* Choices = nullptr;
			if (Rule->TryGetArrayField(TEXT("enum"), Choices))
			{
				bool bMatch = false;
				for (const FValue& Choice : *Choices)
				{
					bMatch |= V->Type == Choice->Type && ((V->Type == EJson::String && V->AsString() == Choice->AsString())
						|| (V->Type == EJson::Number && V->AsNumber() == Choice->AsNumber()));
				}
				if (!bMatch) { return Fail(Path == TEXT("$.schema_version") ? TEXT("UNSUPPORTED_SCHEMA_VERSION") : TEXT("INVALID_VALUE"), Path, TEXT("Value is not in the supported enum.")); }
			}
			double Bound = 0;
			if (V->Type == EJson::Number)
			{
				const double N = V->AsNumber();
				if (!FMath::IsFinite(N) || (Type == TEXT("integer") && FMath::FloorToDouble(N) != N)
					|| (Rule->TryGetNumberField(TEXT("minimum"), Bound) && N < Bound)
					|| (Rule->TryGetNumberField(TEXT("maximum"), Bound) && N > Bound)) { return Fail(TEXT("INVALID_VALUE"), Path, TEXT("Number is outside the supported range or is not an integer.")); }
			}
			if (V->Type == EJson::String)
			{
				const FString S = V->AsString();
				if ((Rule->TryGetNumberField(TEXT("minLength"), Bound) && S.Len() < Bound)
					|| (Rule->TryGetNumberField(TEXT("maxLength"), Bound) && S.Len() > Bound)
					|| (Rule->HasField(TEXT("pattern")) && !Identifier(S))) { return Fail(TEXT("INVALID_VALUE"), Path, TEXT("Invalid string length or identifier.")); }
			}
			bool bValid = true;
			if (V->Type == EJson::Array)
			{
				const TArray<FValue>& Values = V->AsArray();
				if ((Rule->TryGetNumberField(TEXT("minItems"), Bound) && Values.Num() < Bound)
					|| (Rule->TryGetNumberField(TEXT("maxItems"), Bound) && Values.Num() > Bound)) { return Fail(TEXT("INVALID_CHILD_COUNT"), Path, TEXT("Array length is outside the supported range.")); }
				for (int32 I = 0; I < Values.Num(); ++I) { bValid &= Check(Values[I], Rule->GetObjectField(TEXT("items")), FString::Printf(TEXT("%s[%d]"), *Path, I), Depth + 1); }
			}
			if (V->Type == EJson::Object)
			{
				const FObject O = V->AsObject();
				if (Rule->TryGetNumberField(TEXT("maxProperties"), Bound) && O->Values.Num() > Bound) { return Fail(TEXT("LAYOUT_LIMIT"), Path, TEXT("Too many object members.")); }
				const TArray<FValue>* Required = nullptr;
				if (Rule->TryGetArrayField(TEXT("required"), Required))
				{
					for (const FValue& Key : *Required) { if (!O->HasField(Key->AsString())) { bValid = Fail(TEXT("MISSING_FIELD"), Path + TEXT(".") + Key->AsString(), TEXT("Required field is missing.")); } }
				}
				const FObject* Properties = nullptr;
				Rule->TryGetObjectField(TEXT("properties"), Properties);
				if (Properties)
				{
					for (const auto& Property : (*Properties)->Values)
					{
						const FValue* Default = Property.Value->AsObject()->Values.Find(TEXT("default"));
						if (Default && !O->HasField(Property.Key)) { O->SetField(Property.Key, CloneJson(*Default)); }
					}
				}
				for (const auto& Pair : O->Values)
				{
					const FString FieldPath = Path + TEXT(".") + Pair.Key;
					if (Rule->HasField(TEXT("propertyNames")) && !Identifier(Pair.Key)) { bValid = Fail(TEXT("INVALID_VALUE"), FieldPath, TEXT("Invalid resource alias.")); }
					if (Properties && (*Properties)->HasField(Pair.Key)) { bValid &= Check(Pair.Value, (*Properties)->GetObjectField(Pair.Key), FieldPath, Depth + 1); }
					else
					{
						const FValue* Extra = Rule->Values.Find(TEXT("additionalProperties"));
						if (Extra && (*Extra)->Type == EJson::Object) { bValid &= Check(Pair.Value, (*Extra)->AsObject(), FieldPath, Depth + 1); }
						else { bValid = Fail(Pair.Key == TEXT("focused") ? TEXT("UNSUPPORTED_FOCUS_BRUSH") : TEXT("UNKNOWN_FIELD"), FieldPath, TEXT("Unsupported field.")); }
					}
				}
			}
			return bValid;
		}
	private:
		bool Fail(const TCHAR* Code, const FString& Path, const FString& Message) { AddLayoutSchemaError(Result, Code, Path, Message); return false; }
	};

}
bool ValidateLayoutStructure(const TSharedPtr<FJsonObject>& Document, FAutoBlueprintValidationResult& R)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
	FString SchemaText;
	FObject Schema;
	if (!Plugin || !FFileHelper::LoadFileToString(SchemaText, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Schemas/layout.schema.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SchemaText), Schema))
	{ AddLayoutSchemaError(R, TEXT("SCHEMA_UNAVAILABLE"), TEXT("$"), TEXT("Cannot read bundled layout schema.")); return false; }
	FSchemaCheck Checker(Schema, R);
	return Checker.Check(MakeShared<FJsonValueObject>(Document), Schema, TEXT("$"));
}
}
