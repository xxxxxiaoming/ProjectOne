#include "Layout/LayoutValidation.h"

#include "Widgets/WidgetTypeRegistry.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "HAL/FileManager.h"
#include "Layout/LayoutSchemaValidator.h"
#include "Dom/JsonValue.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

namespace AutoBlueprint
{
namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	using FValue = TSharedPtr<FJsonValue>;
	constexpr int32 MaxJsonChars = 1024 * 1024;

	void Error(FAutoBlueprintValidationResult& R, const FString& Code, const FString& Path,
		const FString& Message, const FString& Widget = FString(), const FString& Asset = FString())
	{
		if (R.Diagnostics.Num() >= 64) { return; }
		FAutoBlueprintDiagnostic& D = R.Diagnostics.AddDefaulted_GetRef();
		D.Code = Code; D.JsonPath = Path; D.Message = Message; D.WidgetName = Widget; D.AssetPath = Asset;
	}

	bool ReadBounded(const FString& Path, int64 Limit, TArray<uint8>& Bytes)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
		if (!Reader || Reader->TotalSize() < 0 || Reader->TotalSize() > Limit) { return false; }
		Bytes.SetNumUninitialized(static_cast<int32>(Reader->TotalSize()));
		if (Bytes.Num()) { Reader->Serialize(Bytes.GetData(), Bytes.Num()); }
		return !Reader->IsError();
	}

	bool ProjectFile(const FString& Relative, const FString& Allowed, FString& Full)
	{
		if (!FPaths::IsRelative(Relative) || Relative.Contains(TEXT("\\")) || Relative.Contains(TEXT(":"))) { return false; }
		TArray<FString> Parts;
		Relative.ParseIntoArray(Parts, TEXT("/"), false);
		for (const FString& Part : Parts)
		{
			if (Part.IsEmpty() || Part == TEXT(".") || Part == TEXT("..")) { return false; }
		}
		Full = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
		FPaths::NormalizeFilename(Full);
		FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Allowed));
		FPaths::NormalizeDirectoryName(Root);
		return Relative.StartsWith(Allowed + TEXT("/"), ESearchCase::CaseSensitive)
			&& Full.StartsWith(Root + TEXT("/"), ESearchCase::CaseSensitive);
	}

	// Bound nesting before the JSON deserializer allocates a recursive object graph.
	bool SafeJson(const FString& Json)
	{
		int32 Depth = 0;
		bool bString = false, bEscape = false;
		for (TCHAR C : Json)
		{
			if (bString)
			{
				if (bEscape) { bEscape = false; }
				else if (C == TEXT('\\')) { bEscape = true; }
				else if (C == TEXT('"')) { bString = false; }
			}
			else if (C == TEXT('"')) { bString = true; }
			else if (C == TEXT('{') || C == TEXT('[')) { if (++Depth > 128) { return false; } }
			else if (C == TEXT('}') || C == TEXT(']')) { --Depth; }
		}
		return true;
	}

	FLayoutValue Convert(const FValue& V)
	{
		FLayoutValue R;
		switch (V->Type)
		{
		case EJson::String: R.Kind = ELayoutValueKind::String; R.String = V->AsString(); break;
		case EJson::Number: R.Kind = ELayoutValueKind::Number; R.Number = V->AsNumber(); break;
		case EJson::Boolean: R.Kind = ELayoutValueKind::Boolean; R.bBoolean = V->AsBool(); break;
		case EJson::Array: R.Kind = ELayoutValueKind::Array; for (const FValue& E : V->AsArray()) { R.Elements.Add(Convert(E)); } break;
		case EJson::Object: for (const auto& P : V->AsObject()->Values) { R.Fields.Add(P.Key, MakeShared<FLayoutValue>(Convert(P.Value))); } break;
		default: break; // Null is rejected by schema before conversion.
		}
		return R;
	}

	bool NewPackage(const FString& Package, const FString& Path, FAutoBlueprintValidationResult& R)
	{
		FString Filename;
		FText Reason;
		if (!Package.StartsWith(TEXT("/Game/UI/"), ESearchCase::CaseSensitive)
			|| !FPackageName::IsValidLongPackageName(Package, false, &Reason)
			|| !FPackageName::TryConvertLongPackageNameToFilename(Package, Filename, FPackageName::GetAssetPackageExtension()))
		{ Error(R, TEXT("INVALID_PACKAGE_PATH"), Path, TEXT("Expected a long package name below /Game/UI/."), FString(), Package); return false; }
		if (FindPackage(nullptr, *Package) || FPackageName::DoesPackageExist(Package) || IFileManager::Get().FileExists(*Filename))
		{ Error(R, TEXT("ASSET_ALREADY_EXISTS"), Path, TEXT("Package exists in memory or on disk."), FString(), Package); return false; }
		return true;
	}

	bool ExistingAsset(const FString& Path, UClass* Class, const FString& JsonPath, FAutoBlueprintValidationResult& R)
	{
		FText Reason;
		if (!Path.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive) || !FPackageName::IsValidObjectPath(Path, &Reason))
		{ Error(R, TEXT("INVALID_OBJECT_PATH"), JsonPath, TEXT("Expected a project asset object path."), FString(), Path); return false; }
		UObject* Object = FSoftObjectPath(Path).TryLoad();
		if (!Object) { Error(R, TEXT("MISSING_RESOURCE"), JsonPath, TEXT("Cannot load the referenced resource."), FString(), Path); return false; }
		if (!Object->IsA(Class)) { Error(R, TEXT("INVALID_RESOURCE_TYPE"), JsonPath, TEXT("Resource has the wrong UE class."), FString(), Path); return false; }
		return true;
	}

	void ResourceReferences(const FLayoutValue& Value, const TSet<FString>& Aliases, const FString& Path,
		const FString& Widget, FAutoBlueprintValidationResult& R)
	{
		for (const auto& Pair : Value.Fields)
		{
			const FString P = Path + TEXT(".") + Pair.Key;
			if (Pair.Key == TEXT("resource") && !Aliases.Contains(Pair.Value->String)) { Error(R, TEXT("MISSING_RESOURCE"), P, TEXT("Unknown resource alias: ") + Pair.Value->String, Widget); }
			else if (Pair.Key == TEXT("font")) { ExistingAsset(Pair.Value->String, UFont::StaticClass(), P, R); }
			ResourceReferences(*Pair.Value, Aliases, P, Widget, R);
		}
	}

	FLayoutNode ParseNode(const FObject& O, const FWidgetTypeDescriptor* Parent, const FString& Path, int32 Depth,
		int32& Count, TSet<FName>& Names, const TSet<FString>& Aliases, FAutoBlueprintValidationResult& R)
	{
		FLayoutNode N;
		N.JsonPath = Path; N.Name = FName(*O->GetStringField(TEXT("name"))); N.Type = O->GetStringField(TEXT("type")); N.bIsVariable = O->GetBoolField(TEXT("is_variable"));
		if (++Count > 1024 || Depth > 32) { Error(R, TEXT("LAYOUT_LIMIT"), Path, TEXT("At most 1024 nodes and 32 widget levels are supported."), N.Name.ToString()); return N; }
		if (N.Name.IsNone()) { Error(R, TEXT("INVALID_VALUE"), Path + TEXT(".name"), TEXT("None is reserved by Unreal.")); }
		if (Names.Contains(N.Name)) { Error(R, TEXT("DUPLICATE_WIDGET_NAME"), Path + TEXT(".name"), TEXT("Names must be unique, ignoring case."), N.Name.ToString()); }
		Names.Add(N.Name);
		const FWidgetTypeDescriptor* Type = FindWidgetType(N.Type);
		if (!Type) { Error(R, TEXT("UNSUPPORTED_WIDGET"), Path + TEXT(".type"), TEXT("Widget is not registered.")); return N; }
		const FObject* Slot = nullptr;
		if (O->TryGetObjectField(TEXT("slot"), Slot))
		{
			N.SlotType = (*Slot)->GetStringField(TEXT("type")); N.Slot = Convert(MakeShared<FJsonValueObject>(*Slot));
			if (!Parent || N.SlotType != Parent->ChildSlot) { Error(R, TEXT("INVALID_SLOT"), Path + TEXT(".slot.type"), TEXT("Slot must match the parent container; root cannot have a slot."), N.Name.ToString()); }
			if ((*Slot)->HasField(TEXT("anchors")))
			{
				FObject A = (*Slot)->GetObjectField(TEXT("anchors"));
				const TArray<FValue>& Offsets = (*Slot)->GetArrayField(TEXT("offsets"));
				for (int32 I = 0; I < 2; ++I)
				{
					if (A->GetArrayField(TEXT("min"))[I]->AsNumber() == A->GetArrayField(TEXT("max"))[I]->AsNumber() && Offsets[I + 2]->AsNumber() < 0)
					{ Error(R, TEXT("INVALID_SLOT"), Path + TEXT(".slot.offsets"), TEXT("Fixed-anchor width and height must be nonnegative."), N.Name.ToString()); }
				}
				for (int32 I = 0; I < 2; ++I) { if (A->GetArrayField(TEXT("min"))[I]->AsNumber() > A->GetArrayField(TEXT("max"))[I]->AsNumber()) { Error(R, TEXT("INVALID_SLOT"), Path + TEXT(".slot.anchors"), TEXT("Minimum anchor must not exceed maximum."), N.Name.ToString()); } }
			}
		}
		else if (Parent) { Error(R, TEXT("INVALID_SLOT"), Path + TEXT(".slot"), TEXT("Every non-root node requires an explicit slot."), N.Name.ToString()); }
		const FObject* Props = nullptr;
		if (O->TryGetObjectField(TEXT("properties"), Props)) { N.Properties = Convert(MakeShared<FJsonValueObject>(*Props)); ResourceReferences(N.Properties, Aliases, Path + TEXT(".properties"), N.Name.ToString(), R); }
		const TArray<FValue>* Children = nullptr;
		if (O->TryGetArrayField(TEXT("children"), Children))
		{
			const int32 MaxChildren = Type->Children == EChildPolicy::Leaf ? 0 : Type->Children == EChildPolicy::Single ? 1 : 1024;
			if (Children->Num() > MaxChildren) { Error(R, TEXT("INVALID_CHILD_COUNT"), Path + TEXT(".children"), TEXT("Child policy violated."), N.Name.ToString()); }
			for (int32 I = 0; I < Children->Num() && Count <= 1024; ++I) { N.Children.Add(ParseNode((*Children)[I]->AsObject(), Type, FString::Printf(TEXT("%s.children[%d]"), *Path, I), Depth + 1, Count, Names, Aliases, R)); }
		}
		return N;
	}
}

bool ReadLayoutFile(const FString& RelativePath, FString& OutJson, FAutoBlueprintValidationResult& Result)
{
	FString Path;
	OutJson.Reset();
	if ((!ProjectFile(RelativePath, TEXT("UIBlueprintLayouts"), Path) && !ProjectFile(RelativePath, TEXT("Plugins/AutoBlueprint/Examples"), Path))
		|| !RelativePath.EndsWith(TEXT(".json"), ESearchCase::CaseSensitive))
	{ Error(Result, TEXT("INVALID_LAYOUT_PATH"), TEXT("$"), TEXT("Use a relative .json below UIBlueprintLayouts or Plugins/AutoBlueprint/Examples.")); return false; }
	TArray<uint8> Bytes;
	if (!ReadBounded(Path, MaxJsonChars, Bytes)) { Error(Result, TEXT("LAYOUT_READ_FAILED"), TEXT("$"), TEXT("Layout is missing, unreadable, or exceeds 1 MiB.")); return false; }
	if (Bytes.Num()) { FFileHelper::BufferToString(OutJson, Bytes.GetData(), Bytes.Num()); }
	return true;
}

FAutoBlueprintValidationResult ValidateLayoutJson(const FString& Json, FLayoutSpec& OutLayout)
{
	FAutoBlueprintValidationResult R;
	OutLayout = FLayoutSpec();
	if (Json.Len() > MaxJsonChars || !SafeJson(Json)) { Error(R, TEXT("LAYOUT_LIMIT"), TEXT("$"), TEXT("JSON exceeds size or nesting limits.")); return R; }
	// Reject duplicate JSON keys rather than silently accepting the last value.
	const TSharedRef<TJsonReader<>> KeyReader = TJsonReaderFactory<>::Create(Json);
	TArray<TSet<FString>> Keys;
	TArray<bool> Objects;
	EJsonNotation Notation;
	while (KeyReader->ReadNext(Notation))
	{
		const bool bEnd = Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd;
		if (!bEnd && Objects.Num() && Objects.Last())
		{
			const FString Key = KeyReader->GetIdentifier();
			if (Keys.Last().Contains(Key)) { Error(R, TEXT("DUPLICATE_JSON_FIELD"), TEXT("$"), TEXT("Duplicate JSON member: ") + Key); return R; }
			Keys.Last().Add(Key);
		}
		if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
		{ Objects.Add(Notation == EJsonNotation::ObjectStart); Keys.AddDefaulted(); }
		else if (bEnd && Objects.Num()) { Objects.Pop(); Keys.Pop(); }
	}
	FObject Document;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Document) || !Document.IsValid()) { Error(R, TEXT("INVALID_JSON"), TEXT("$"), Reader->GetErrorMessage()); return R; }
	if (!ValidateLayoutStructure(Document, R)) { return R; }
	FLayoutSpec Layout;
	Layout.PackagePath = Document->GetObjectField(TEXT("asset"))->GetStringField(TEXT("package_path"));
	const FObject Design = Document->GetObjectField(TEXT("design"));
	Layout.DesignSize = FVector2D(Design->GetNumberField(TEXT("width")), Design->GetNumberField(TEXT("height")));
	if (NewPackage(Layout.PackagePath, TEXT("$.asset.package_path"), R)) { R.PlannedAssetPath = Layout.PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(Layout.PackagePath); }
	TSet<FString> Aliases;
	TSet<FName> Targets;
	Targets.Add(FName(*Layout.PackagePath));
	const FObject* Resources = nullptr;
	if (Document->TryGetObjectField(TEXT("resources"), Resources))
	{
		TArray<FString> Keys;
		(*Resources)->Values.GetKeys(Keys); Keys.Sort();
		for (const FString& Key : Keys)
		{
			Aliases.Add(Key);
			const FObject O = (*Resources)->GetObjectField(Key);
			const FString P = TEXT("$.resources.") + Key;
			FLayoutResource Resource; Resource.Alias = Key;
			if (O->TryGetStringField(TEXT("object_path"), Resource.ObjectPath)) { ExistingAsset(Resource.ObjectPath, UTexture2D::StaticClass(), P + TEXT(".object_path"), R); }
			else
			{
				Resource.SourceFile = O->GetStringField(TEXT("source_file")); Resource.PackagePath = O->GetStringField(TEXT("package_path"));
				if (Targets.Contains(FName(*Resource.PackagePath))) { Error(R, TEXT("DUPLICATE_PACKAGE_TARGET"), P + TEXT(".package_path"), TEXT("Resource and Widget package targets must be distinct."), FString(), Resource.PackagePath); }
				Targets.Add(FName(*Resource.PackagePath));
				const bool bNew = NewPackage(Resource.PackagePath, P + TEXT(".package_path"), R);
				FString Full;
				const FString Extension = FPaths::GetExtension(Resource.SourceFile).ToLower();
				if (!ProjectFile(Resource.SourceFile, TEXT("Content/UI"), Full) || (Extension != TEXT("png") && Extension != TEXT("jpg") && Extension != TEXT("jpeg")))
				{ Error(R, TEXT("INVALID_SOURCE_PATH"), P + TEXT(".source_file"), TEXT("Expected PNG/JPG below Content/UI.")); }
				else
				{
					TArray<uint8> Bytes;
					if (!ReadBounded(Full, 32 * 1024 * 1024, Bytes) || Bytes.IsEmpty()) { Error(R, TEXT("MISSING_RESOURCE"), P + TEXT(".source_file"), TEXT("Image missing, unreadable, empty, or exceeds 32 MiB.")); }
					else
					{
						const ANSICHAR* Lfs = "version https://git-lfs.github.com/spec/v1";
						if (Bytes.Num() >= FCStringAnsi::Strlen(Lfs) && FMemory::Memcmp(Bytes.GetData(), Lfs, FCStringAnsi::Strlen(Lfs)) == 0)
						{ Error(R, TEXT("LFS_SOURCE_NOT_MATERIALIZED"), P + TEXT(".source_file"), TEXT("Download the Git LFS image first.")); }
						else
						{
							IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
							const EImageFormat Format = Images.DetectImageFormat(Bytes.GetData(), Bytes.Num());
							TSharedPtr<IImageWrapper> Image;
							if (Format == EImageFormat::PNG || Format == EImageFormat::JPEG) { Image = Images.CreateImageWrapper(Format); }
							TArray64<uint8> Pixels;
							if ((Format != EImageFormat::PNG && Format != EImageFormat::JPEG) || !Image.IsValid()
								|| !Image->SetCompressed(Bytes.GetData(), Bytes.Num()) || Image->GetWidth() <= 0 || Image->GetHeight() <= 0
								|| static_cast<int64>(Image->GetWidth()) * Image->GetHeight() > 16 * 1024 * 1024
								|| !Image->GetRaw(ERGBFormat::BGRA, 8, Pixels))
							{ Error(R, TEXT("INVALID_IMAGE"), P + TEXT(".source_file"), TEXT("Image cannot be decoded or exceeds 16 megapixels.")); }
							else if (bNew) { R.PlannedImports.Add(Resource.PackagePath); }
						}
					}
				}
			}
			Layout.Resources.Add(MoveTemp(Resource));
		}
	}
	int32 Count = 0;
	TSet<FName> Names;
	Layout.Root = ParseNode(Document->GetObjectField(TEXT("root")), nullptr, TEXT("$.root"), 1, Count, Names, Aliases, R);
	R.bValid = R.Diagnostics.IsEmpty();
	if (R.bValid) { OutLayout = MoveTemp(Layout); }
	return R;
}
}
