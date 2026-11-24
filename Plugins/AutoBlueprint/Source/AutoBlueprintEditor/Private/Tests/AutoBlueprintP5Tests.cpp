#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Margin.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/SlateBrush.h"
#include "Templates/SubclassOf.h"
#include "Types/SlateEnums.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace AutoBlueprintP5Tests
{
	FString Serialize(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;

		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	TSharedPtr<FJsonObject> FindNode(const TSharedPtr<FJsonObject>& Node, const FString& Name)
	{
		if (Node->GetStringField(TEXT("name")) == Name)
		{
			return Node;
		}

		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;

		if (Node->TryGetArrayField(TEXT("children"), Children))
		{
			for (const TSharedPtr<FJsonValue>& Child : *Children)
			{
				TSharedPtr<FJsonObject> Found = FindNode(Child->AsObject(), Name);

				if (Found.IsValid())
				{
					return Found;
				}
			}
		}

		return nullptr;
	}

	struct FFixture
	{
		FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Base = TEXT("/Game/UI/AutoBlueprintTests/P5_") + Id;
		FString Source = TEXT("Content/UI/AutoBlueprintTests/P5_") + Id + TEXT(".png");
		FString File = TEXT("UIBlueprintLayouts/P5_") + Id + TEXT(".json");
		TSharedPtr<FJsonObject> Input;

		bool Initialize(FAutomationTestBase& Test, bool bTexture = true)
		{
			FString Code;
			FString Message;

			if (!AutoBlueprint::CheckEditorContext(Code, Message))
			{
				Test.AddError(Code + TEXT(": ") + Message);
				return false;
			}

			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
			FString Json;

			if (!Test.TestTrue(TEXT("P5 example readable"), Plugin.IsValid()
				&& FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Examples/p5_nested.layout.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Input) && Input.IsValid()))
			{
				return false;
			}

			Input->GetObjectField(TEXT("asset"))->SetStringField(TEXT("package_path"), Base + TEXT("/WBP_Nested"));
			if (!bTexture)
			{
				return true;
			}

			TArray<uint8> Bytes;

			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / FPaths::GetPath(Source)), true);
			if (!Test.TestTrue(TEXT("Test PNG written"), FBase64::Decode(TEXT("iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAYAAADED76LAAAAE0lEQVR4nGP4v0XjPz7MMDIUAAAyCraBzBAkmwAAAABJRU5ErkJggg=="), Bytes)
				&& FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectDir() / Source))))
			{
				return false;
			}

			const TSharedRef<FJsonObject> Resource = MakeShared<FJsonObject>();
			const TSharedRef<FJsonObject> Resources = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> Brush;

			Resource->SetStringField(TEXT("source_file"), Source);
			Resource->SetStringField(TEXT("package_path"), Base + TEXT("/T_Frame"));
			Resources->SetObjectField(TEXT("frame"), Resource);
			Input->SetObjectField(TEXT("resources"), Resources);

			const FString BrushJson = TEXT(R"({"resource":"frame","draw_as":"Box","margin":[0.25,0.25,0.25,0.25],"tint":[0.2,0.4,0.6,0.8]})");

			if (!Test.TestTrue(TEXT("Brush fixture parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BrushJson), Brush)))
			{
				return false;
			}
			FindNode(Input->GetObjectField(TEXT("root")), TEXT("Border_Card"))->GetObjectField(TEXT("properties"))->SetObjectField(TEXT("brush"), Brush);
			FindNode(Input->GetObjectField(TEXT("root")), TEXT("IMG_Art"))->GetObjectField(TEXT("properties"))->SetObjectField(TEXT("brush"), Brush);
			return true;
		}

		bool WriteLayout() const
		{
			IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir() / TEXT("UIBlueprintLayouts")), true);
			return FFileHelper::SaveStringToFile(Serialize(Input), *(FPaths::ProjectDir() / File), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		~FFixture()
		{
			IFileManager::Get().Delete(*(FPaths::ProjectDir() / Source), false, true);
			IFileManager::Get().Delete(*(FPaths::ProjectDir() / File), false, true);
		}
	};

	void ReleaseUnsaved(const FAutoBlueprintGenerationResult& Result)
	{
		TArray<FString> Paths;

		if (Result.Widget.bAssetCreated && !Result.Widget.bSaved)
		{
			Paths.Add(Result.Widget.TargetAssetPath);
		}

		for (const FAutoBlueprintResourceResult& Item : Result.Resources)
		{
			if (Item.bImported && !Item.bSaved)
			{
				Paths.Add(Item.TargetAssetPath);
			}
		}

		for (const FString& Path : Paths)
		{
			if (UObject* Object = FindObject<UObject>(nullptr, *Path))
			{
				Object->ClearFlags(RF_Standalone);
				Object->GetOutermost()->SetDirtyFlag(false);
			}
		}
	}

	// Read serialized properties to check persistence without depending on newer UE getters.
	template <typename TValue>
	TValue ReadStruct(FAutomationTestBase& Test, const UObject& Object, const TCHAR* Name)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(Object.GetClass(), Name);

		if (!Test.TestNotNull(FString(TEXT("Struct property: ")) + Name, Property)
			|| !Test.TestTrue(TEXT("Expected struct size"), Property->Struct->GetStructureSize() == static_cast<int32>(sizeof(TValue))))
		{
			return TValue();
		}

		return *Property->ContainerPtrToValuePtr<TValue>(&Object);
	}

	uint8 ReadByte(FAutomationTestBase& Test, const UObject& Object, const TCHAR* Name)
	{
		const FByteProperty* Property = FindFProperty<FByteProperty>(Object.GetClass(), Name);

		return Test.TestNotNull(FString(TEXT("Byte property: ")) + Name, Property) ? Property->GetPropertyValue_InContainer(&Object) : 0;
	}

	float ReadFloat(FAutomationTestBase& Test, const UObject& Object, const TCHAR* Name)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object.GetClass(), Name);

		return Test.TestNotNull(FString(TEXT("Float property: ")) + Name, Property) ? Property->GetPropertyValue_InContainer(&Object) : 0.0f;
	}

	bool ReadBool(FAutomationTestBase& Test, const UObject& Object, const TCHAR* Name)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object.GetClass(), Name);

		return Test.TestNotNull(FString(TEXT("Bool property: ")) + Name, Property) && Property->GetPropertyValue_InContainer(&Object);
	}

	bool Inspect(FAutomationTestBase& Test, UWidgetTree* Tree, bool bTexture)
	{
		if (!Test.TestNotNull(TEXT("Widget tree exists"), Tree))
		{
			return false;
		}

		TArray<UWidget*> Widgets;

		Tree->GetAllWidgets(Widgets);
		Test.TestEqual(TEXT("All 17 configured widgets survive"), Widgets.Num(), 17);

		struct FNodeExpectation
		{
			const TCHAR* Name;
			UClass* Class;
			int32 Parent;
			int32 ChildIndex;
		};

		const FNodeExpectation Nodes[] =
		{
			{ TEXT("RootCanvas"), UCanvasPanel::StaticClass(), -1, -1 },
			{ TEXT("Overlay_Main"), UOverlay::StaticClass(), 0, 0 },
			{ TEXT("Border_Card"), UBorder::StaticClass(), 1, 0 },
			{ TEXT("Column_Card"), UVerticalBox::StaticClass(), 2, 0 },
			{ TEXT("TXT_Header"), UTextBlock::StaticClass(), 3, 0 },
			{ TEXT("Row_Content"), UHorizontalBox::StaticClass(), 3, 1 },
			{ TEXT("Size_Art"), USizeBox::StaticClass(), 5, 0 },
			{ TEXT("Scale_Art"), UScaleBox::StaticClass(), 6, 0 },
			{ TEXT("IMG_Art"), UImage::StaticClass(), 7, 0 },
			{ TEXT("Gap"), USpacer::StaticClass(), 5, 1 },
			{ TEXT("BTN_Confirm"), UButton::StaticClass(), 5, 2 },
			{ TEXT("TXT_Button"), UTextBlock::StaticClass(), 10, 0 },
			{ TEXT("Tail"), USpacer::StaticClass(), 3, 2 },
			{ TEXT("Corner"), USpacer::StaticClass(), 1, 1 },
			{ TEXT("Size_Default"), USizeBox::StaticClass(), 0, 1 },
			{ TEXT("TXT_Default"), UTextBlock::StaticClass(), 14, 0 },
			{ TEXT("Size_Zero"), USizeBox::StaticClass(), 0, 2 },
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Nodes); ++Index)
		{
			UWidget* Widget = Tree->FindWidget(Nodes[Index].Name);

			if (!Test.TestNotNull(Nodes[Index].Name, Widget))
			{
				return false;
			}

			Test.TestTrue(TEXT("Native class matches layout"), Widget->GetClass() == Nodes[Index].Class);
			if (Nodes[Index].Parent >= 0)
			{
				UPanelWidget* Parent = Cast<UPanelWidget>(Tree->FindWidget(Nodes[Nodes[Index].Parent].Name));

				Test.TestTrue(TEXT("Configured parent and sibling order"), Parent && Widget->GetParent() == Parent && Parent->GetChildAt(Nodes[Index].ChildIndex) == Widget);
			}
		}

		// Each child carries the slot of its actual parent, never its own widget type.
		struct FSlotExpectation
		{
			const TCHAR* Child;
			const TCHAR* Class;
			FMargin Padding;
			uint8 Horizontal;
			uint8 Vertical;
			bool bHasPadding;
		};

		const FSlotExpectation Slots[] =
		{
			{ TEXT("Border_Card"), TEXT("OverlaySlot"), FMargin(4, 6, 8, 10), HAlign_Fill, VAlign_Fill, true },
			{ TEXT("Column_Card"), TEXT("BorderSlot"), FMargin(12, 14, 16, 18), HAlign_Fill, VAlign_Fill, true },
			{ TEXT("TXT_Header"), TEXT("VerticalBoxSlot"), FMargin(1, 2, 3, 4), HAlign_Center, VAlign_Top, true },
			{ TEXT("Row_Content"), TEXT("VerticalBoxSlot"), FMargin(5, 6, 7, 8), HAlign_Fill, VAlign_Fill, true },
			{ TEXT("Size_Art"), TEXT("HorizontalBoxSlot"), FMargin(9, 10, 11, 12), HAlign_Left, VAlign_Center, true },
			{ TEXT("Gap"), TEXT("HorizontalBoxSlot"), FMargin(0), HAlign_Fill, VAlign_Fill, true },
			{ TEXT("BTN_Confirm"), TEXT("HorizontalBoxSlot"), FMargin(1, 2, 3, 4), HAlign_Fill, VAlign_Center, true },
			{ TEXT("Scale_Art"), TEXT("SizeBoxSlot"), FMargin(3, 4, 5, 6), HAlign_Center, VAlign_Bottom, true },
			{ TEXT("IMG_Art"), TEXT("ScaleBoxSlot"), FMargin(0), HAlign_Right, VAlign_Top, false },
			{ TEXT("TXT_Button"), TEXT("ButtonSlot"), FMargin(6, 7, 8, 9), HAlign_Center, VAlign_Center, true },
			{ TEXT("Corner"), TEXT("OverlaySlot"), FMargin(2, 4, 6, 8), HAlign_Right, VAlign_Bottom, true },
			{ TEXT("TXT_Default"), TEXT("SizeBoxSlot"), FMargin(0), HAlign_Fill, VAlign_Fill, true },
		};

		for (const FSlotExpectation& Expected : Slots)
		{
			UPanelSlot* Slot = Tree->FindWidget(Expected.Child)->Slot;

			if (!Test.TestNotNull(TEXT("Actual parent slot exists"), Slot))
			{
				return false;
			}

			Test.TestEqual(TEXT("Correct native slot class"), Slot->GetClass()->GetName(), FString(Expected.Class));
			Test.TestEqual(TEXT("Horizontal alignment persists"), ReadByte(Test, *Slot, TEXT("HorizontalAlignment")), Expected.Horizontal);
			Test.TestEqual(TEXT("Vertical alignment persists"), ReadByte(Test, *Slot, TEXT("VerticalAlignment")), Expected.Vertical);
			if (Expected.bHasPadding)
			{
				Test.TestTrue(TEXT("Padding persists"), ReadStruct<FMargin>(Test, *Slot, TEXT("Padding")) == Expected.Padding);
			}
		}

		const FSlateChildSize Auto = ReadStruct<FSlateChildSize>(Test, *Tree->FindWidget(TEXT("Size_Art"))->Slot, TEXT("Size"));
		const FSlateChildSize HorizontalFill = ReadStruct<FSlateChildSize>(Test, *Tree->FindWidget(TEXT("BTN_Confirm"))->Slot, TEXT("Size"));
		const FSlateChildSize VerticalFill = ReadStruct<FSlateChildSize>(Test, *Tree->FindWidget(TEXT("Row_Content"))->Slot, TEXT("Size"));

		Test.TestTrue(TEXT("Auto size rule"), Auto.SizeRule == ESlateSizeRule::Automatic && Auto.Value == 1.0f);
		Test.TestTrue(TEXT("Horizontal fill weight"), HorizontalFill.SizeRule == ESlateSizeRule::Fill && HorizontalFill.Value == 3.0f);
		Test.TestTrue(TEXT("Vertical fill weight"), VerticalFill.SizeRule == ESlateSizeRule::Fill && VerticalFill.Value == 2.0f);

		UWidget* ArtSize = Tree->FindWidget(TEXT("Size_Art"));
		UWidget* DefaultSize = Tree->FindWidget(TEXT("Size_Default"));
		UWidget* ZeroSize = Tree->FindWidget(TEXT("Size_Zero"));
		UWidget* Scale = Tree->FindWidget(TEXT("Scale_Art"));
		UCanvasPanelSlot* Stretched = Cast<UCanvasPanelSlot>(Tree->FindWidget(TEXT("Overlay_Main"))->Slot);
		UCanvasPanelSlot* AutoSlot = Cast<UCanvasPanelSlot>(DefaultSize->Slot);

		Test.TestTrue(TEXT("Both explicit overrides enabled"), ReadBool(Test, *ArtSize, TEXT("bOverride_WidthOverride")) && ReadBool(Test, *ArtSize, TEXT("bOverride_HeightOverride")));
		Test.TestEqual(TEXT("Width override"), ReadFloat(Test, *ArtSize, TEXT("WidthOverride")), 160.0f);
		Test.TestEqual(TEXT("Height override"), ReadFloat(Test, *ArtSize, TEXT("HeightOverride")), 100.0f);
		Test.TestFalse(TEXT("Absent width does not force zero"), ReadBool(Test, *DefaultSize, TEXT("bOverride_WidthOverride")));
		Test.TestFalse(TEXT("Absent height does not force zero"), ReadBool(Test, *DefaultSize, TEXT("bOverride_HeightOverride")));
		Test.TestTrue(TEXT("Explicit zero overrides remain enabled"), ReadBool(Test, *ZeroSize, TEXT("bOverride_WidthOverride")) && ReadBool(Test, *ZeroSize, TEXT("bOverride_HeightOverride")));
		Test.TestEqual(TEXT("Zero width"), ReadFloat(Test, *ZeroSize, TEXT("WidthOverride")), 0.0f);
		Test.TestEqual(TEXT("Zero height"), ReadFloat(Test, *ZeroSize, TEXT("HeightOverride")), 0.0f);
		Test.TestEqual(TEXT("ScaleToFit mode"), ReadByte(Test, *Scale, TEXT("Stretch")), static_cast<uint8>(EStretch::ScaleToFit));
		Test.TestEqual(TEXT("DownOnly direction"), ReadByte(Test, *Scale, TEXT("StretchDirection")), static_cast<uint8>(EStretchDirection::DownOnly));
		Test.TestEqual(TEXT("Stored user scale"), ReadFloat(Test, *Scale, TEXT("UserSpecifiedScale")), 1.25f);
		Test.TestTrue(TEXT("Spacer desired size"), ReadStruct<FVector2D>(Test, *Tree->FindWidget(TEXT("Gap")), TEXT("Size")) == FVector2D(24, 12));
		Test.TestTrue(TEXT("Canvas stretched anchors, offsets and z-order"), Stretched && Stretched->GetAnchors().Minimum == FVector2D(0,0)
			&& Stretched->GetAnchors().Maximum == FVector2D(1,1) && Stretched->GetOffsets() == FMargin(24) && Stretched->GetZOrder() == 2);
		Test.TestTrue(TEXT("Canvas fixed auto-size and alignment"), AutoSlot && AutoSlot->GetAutoSize()
			&& AutoSlot->GetOffsets() == FMargin(50,500,0,0) && AutoSlot->GetAlignment() == FVector2D(0.5,0.5) && AutoSlot->GetZOrder() == 7);

		UWidget* Border = Tree->FindWidget(TEXT("Border_Card"));

		Test.TestTrue(TEXT("Border content color"), ReadStruct<FLinearColor>(Test, *Border, TEXT("ContentColorAndOpacity")).Equals(FLinearColor(0.9f,0.8f,0.7f,1)));
		if (bTexture)
		{
			const FSlateBrush Background = ReadStruct<FSlateBrush>(Test, *Border, TEXT("Background"));
			const FSlateBrush Image = ReadStruct<FSlateBrush>(Test, *Tree->FindWidget(TEXT("IMG_Art")), TEXT("Brush"));

			Test.TestNotNull(TEXT("Border references runtime texture"), Cast<UTexture2D>(Background.GetResourceObject()));
			Test.TestTrue(TEXT("Border and Image share one imported texture"), Background.GetResourceObject() == Image.GetResourceObject());
			Test.TestTrue(TEXT("Border nine-slice and linear tint"), Background.DrawAs == ESlateBrushDrawType::Box && Background.Margin == FMargin(0.25f)
				&& Background.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.2f,0.4f,0.6f,0.8f)));
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP5PersistenceTest,
	"AutoBlueprint.P5.NestedSaveReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP5PersistenceTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP5Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	FAutoBlueprintBuildOptions Options;
	const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP5Tests::Serialize(Fixture.Input), Options);

	ON_SCOPE_EXIT
	{
		AutoBlueprintP5Tests::ReleaseUnsaved(Result);
	};

	if (!TestTrue(TEXT("Nested task saved"), Result.bSuccess && Result.Widget.bSaved)
		|| !TestTrue(TEXT("Shared texture saved once"), Result.Resources.Num() == 1 && Result.Resources[0].bSaved))
	{
		return false;
	}

	UWidgetBlueprint* Blueprint = FindObject<UWidgetBlueprint>(nullptr, *Result.Widget.TargetAssetPath);

	if (!TestNotNull(TEXT("Blueprint exists"), Blueprint) || !AutoBlueprintP5Tests::Inspect(*this, Blueprint->WidgetTree, true))
	{
		return false;
	}

	for (const TCHAR* Name : { TEXT("Overlay_Main"), TEXT("Border_Card"), TEXT("Column_Card"), TEXT("Row_Content"), TEXT("Size_Art"), TEXT("Scale_Art"), TEXT("IMG_Art") })
	{
		TestNotNull(TEXT("New container/leaf variable generated"), FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, Name));
	}

	TestNull(TEXT("Non-variable spacer has no generated member"), FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("Gap")));

	TArray<UPackage*> Packages;

	Packages.Add(Blueprint->GetOutermost());
	Packages.Add(FindPackage(nullptr, *Result.Resources[0].CreatedPackageName));
	Blueprint = nullptr;
	if (!TestTrue(TEXT("Widget and texture unload"), UPackageTools::UnloadPackages(Packages)))
	{
		return false;
	}

	Packages.Reset();
	TestNull(TEXT("No cached Widget before reload"), FindPackage(nullptr, *Result.Widget.CreatedPackageName));

	TStrongObjectPtr<UWidgetBlueprint> Reloaded(LoadObject<UWidgetBlueprint>(nullptr, *Result.Widget.TargetAssetPath));

	if (!TestNotNull(TEXT("Blueprint loads from disk"), Reloaded.Get()) || !AutoBlueprintP5Tests::Inspect(*this, Reloaded->WidgetTree, true))
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	const TSubclassOf<UUserWidget> WidgetClass(Reloaded->GeneratedClass.Get());

	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	TStrongObjectPtr<UUserWidget> Instance(CreateWidget<UUserWidget>(World, WidgetClass));

	if (TestNotNull(TEXT("Reloaded class instantiates"), Instance.Get()))
	{
		AutoBlueprintP5Tests::Inspect(*this, Instance->WidgetTree, true);
	}

	const FAutoBlueprintGenerationResult Conflict = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP5Tests::Serialize(Fixture.Input), Options);

	TestTrue(TEXT("CreateOnly preserves existing nested asset"), !Conflict.bSuccess && !Conflict.Widget.bAssetCreated
		&& FindObject<UWidgetBlueprint>(nullptr, *Result.Widget.TargetAssetPath) == Reloaded.Get());
	AddInfo(TEXT("Saved P5 fixture retained for inspection: ") + Result.Widget.TargetAssetPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP5ScaleModesTest,
	"AutoBlueprint.P5.ScaleModesAndDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP5ScaleModesTest::RunTest(const FString& Parameters)
{
	const TCHAR* Modes[] = { TEXT("None"), TEXT("Fill"), TEXT("ScaleToFit"), TEXT("ScaleToFitX"), TEXT("ScaleToFitY"), TEXT("ScaleToFill"), TEXT("UserSpecified") };
	const uint8 Expected[] = { EStretch::None, EStretch::Fill, EStretch::ScaleToFit, EStretch::ScaleToFitX, EStretch::ScaleToFitY, EStretch::ScaleToFill, EStretch::UserSpecified };
	const TCHAR* Directions[] = { TEXT("Both"), TEXT("DownOnly"), TEXT("UpOnly") };
	const uint8 ExpectedDirections[] = { EStretchDirection::Both, EStretchDirection::DownOnly, EStretchDirection::UpOnly };

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Modes) + 1; ++Index)
	{
		AutoBlueprintP5Tests::FFixture Fixture;

		if (!Fixture.Initialize(*this, false))
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Scale = AutoBlueprintP5Tests::FindNode(Fixture.Input->GetObjectField(TEXT("root")), TEXT("Scale_Art"));
		const TSharedPtr<FJsonObject> Properties = Scale->GetObjectField(TEXT("properties"));

		if (Index == UE_ARRAY_COUNT(Modes))
		{
			Properties->Values.Empty();
		}
		else
		{
			Properties->SetStringField(TEXT("stretch"), Modes[Index]);
			Properties->SetStringField(TEXT("stretch_direction"), Directions[Index % 3]);
			Properties->SetNumberField(TEXT("user_specified_scale"), 2.5);
		}

		FAutoBlueprintBuildOptions Options;

		Options.bSave = false;
		Options.bWriteReport = false;

		const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(AutoBlueprintP5Tests::Serialize(Fixture.Input), Options);

		ON_SCOPE_EXIT
		{
			AutoBlueprintP5Tests::ReleaseUnsaved(Result);
		};

		UWidgetBlueprint* Blueprint = FindObject<UWidgetBlueprint>(nullptr, *Result.Widget.TargetAssetPath);

		if (!TestTrue(TEXT("Resource-free nested layout builds"), Result.bSuccess && !Result.Widget.bSaved)
			|| !TestNotNull(TEXT("In-memory Blueprint"), Blueprint))
		{
			return false;
		}

		UWidget* Widget = Blueprint->WidgetTree->FindWidget(TEXT("Scale_Art"));
		const bool bDefaults = Index == UE_ARRAY_COUNT(Modes);

		if (!TestNotNull(TEXT("ScaleBox template"), Widget))
		{
			return false;
		}

		TestEqual(TEXT("Stretch enum mapping"), AutoBlueprintP5Tests::ReadByte(*this, *Widget, TEXT("Stretch")), bDefaults ? static_cast<uint8>(EStretch::ScaleToFit) : Expected[Index]);
		TestEqual(TEXT("Direction enum mapping"), AutoBlueprintP5Tests::ReadByte(*this, *Widget, TEXT("StretchDirection")), bDefaults ? static_cast<uint8>(EStretchDirection::Both) : ExpectedDirections[Index % 3]);
		TestEqual(TEXT("User scale mapping/default"), AutoBlueprintP5Tests::ReadFloat(*this, *Widget, TEXT("UserSpecifiedScale")), bDefaults ? 1.0f : 2.5f);
		TestFalse(TEXT("No-save never writes Widget file"), FPackageName::DoesPackageExist(Result.Widget.CreatedPackageName));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP5InvalidTest,
	"AutoBlueprint.P5.InvalidLayoutsNoWrites", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP5InvalidTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 10; ++Case)
	{
		AutoBlueprintP5Tests::FFixture Fixture;

		if (!Fixture.Initialize(*this))
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Root = Fixture.Input->GetObjectField(TEXT("root"));
		const TSharedPtr<FJsonObject> Image = AutoBlueprintP5Tests::FindNode(Root, TEXT("IMG_Art"));
		const TSharedPtr<FJsonObject> Scale = AutoBlueprintP5Tests::FindNode(Root, TEXT("Scale_Art"));
		const TSharedPtr<FJsonObject> Gap = AutoBlueprintP5Tests::FindNode(Root, TEXT("Gap"));
		FString Expected;
		FString ExpectedPath;

		if (Case == 0)
		{
			Image->GetObjectField(TEXT("slot"))->SetStringField(TEXT("type"), TEXT("Overlay"));
			Expected = TEXT("INVALID_SLOT");
		}
		else if (Case == 1)
		{
			Image->SetArrayField(TEXT("children"), { MakeShared<FJsonValueObject>(Gap) });
			Expected = TEXT("INVALID_CHILD_COUNT");
		}
		else if (Case == 2)
		{
			Scale->SetArrayField(TEXT("children"), { MakeShared<FJsonValueObject>(Image), MakeShared<FJsonValueObject>(Gap) });
			Expected = TEXT("INVALID_CHILD_COUNT");
		}
		else if (Case == 3)
		{
			Image->GetObjectField(TEXT("slot"))->SetArrayField(TEXT("padding"), { MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0) });
			Expected = TEXT("UNSUPPORTED_SLOT_PROPERTY");
			ExpectedPath = TEXT("$.root.children[0].children[0].children[0].children[1].children[0].children[0].children[0].slot.padding");
		}
		else if (Case == 4)
		{
			AutoBlueprintP5Tests::FindNode(Root, TEXT("BTN_Confirm"))->GetObjectField(TEXT("slot"))->SetNumberField(TEXT("fill"), 0);
			Expected = TEXT("INVALID_VALUE");
		}
		else if (Case == 5)
		{
			Scale->SetStringField(TEXT("type"), TEXT("ListView"));
			Expected = TEXT("UNSUPPORTED_WIDGET");
		}
		else if (Case == 6)
		{
			Root->SetObjectField(TEXT("slot"), Root->GetArrayField(TEXT("children"))[0]->AsObject()->GetObjectField(TEXT("slot")));
			Expected = TEXT("INVALID_SLOT");
		}
		else if (Case == 7)
		{
			AutoBlueprintP5Tests::FindNode(Root, TEXT("Size_Art"))->GetObjectField(TEXT("properties"))->SetNumberField(TEXT("width_override"), -1);
			Expected = TEXT("INVALID_VALUE");
		}
		else if (Case == 8)
		{
			Image->RemoveField(TEXT("slot"));
			Expected = TEXT("INVALID_SLOT");
		}
		else
		{
			Gap->SetStringField(TEXT("name"), TEXT("IMG_Art"));
			Expected = TEXT("DUPLICATE_WIDGET_NAME");
		}

		FAutoBlueprintBuildOptions Options;

		Options.bWriteReport = false;

		const FString Json = AutoBlueprintP5Tests::Serialize(Fixture.Input);
		const FAutoBlueprintValidationResult Validation = UAutoBlueprintLibrary::ValidateLayout(Json);
		const FAutoBlueprintGenerationResult Result = UAutoBlueprintLibrary::GenerateFromLayout(Json, Options);
		const FAutoBlueprintDiagnostic* Diagnostic = Result.Widget.Diagnostics.FindByPredicate([&Expected](const FAutoBlueprintDiagnostic& Item)
		{
			return Item.Code == Expected;
		});

		TestFalse(TEXT("Invalid layout fails read-only validation"), Validation.bValid);
		TestFalse(TEXT("Invalid layout fails generation"), Result.bSuccess);
		if (TestNotNull(TEXT("Specific diagnostic present"), Diagnostic))
		{
			TestFalse(TEXT("Diagnostic locates JSON input"), Diagnostic->JsonPath.IsEmpty());
			if (!ExpectedPath.IsEmpty())
			{
				TestEqual(TEXT("Unsupported ScaleBox padding is precisely located"), Diagnostic->JsonPath, ExpectedPath);
			}
		}

		TestTrue(TEXT("Preflight rejects before import stage"), Result.Resources.IsEmpty());
		TestFalse(TEXT("No Widget created"), Result.Widget.bAssetCreated);
		TestNull(TEXT("No Widget package in memory"), FindPackage(nullptr, *(Fixture.Base + TEXT("/WBP_Nested"))));
		TestNull(TEXT("No texture package in memory"), FindPackage(nullptr, *(Fixture.Base + TEXT("/T_Frame"))));
		TestFalse(TEXT("No Widget file"), FPackageName::DoesPackageExist(Fixture.Base + TEXT("/WBP_Nested")));
		TestFalse(TEXT("No texture file"), FPackageName::DoesPackageExist(Fixture.Base + TEXT("/T_Frame")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoBlueprintP5LuaTest,
	"AutoBlueprint.P5.FileLuaNoSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutoBlueprintP5LuaTest::RunTest(const FString& Parameters)
{
	AutoBlueprintP5Tests::FFixture Fixture;

	if (!Fixture.Initialize(*this) || !TestTrue(TEXT("Nested layout file written"), Fixture.WriteLayout()))
	{
		return false;
	}

	FAutoBlueprintLuaRunRequest Request;

	Request.ScriptPath = TEXT("tests/p5_generate_no_save.lua");
	Request.Arguments.Add(Fixture.File);

	const FAutoBlueprintLuaRunResult Script = UAutoBlueprintLibrary::RunEditorScript(Request);
	FAutoBlueprintGenerationResult Created;

	Created.Widget.TargetAssetPath = Fixture.Base + TEXT("/WBP_Nested.WBP_Nested");
	Created.Widget.bAssetCreated = FindObject<UWidgetBlueprint>(nullptr, *Created.Widget.TargetAssetPath) != nullptr;

	FAutoBlueprintResourceResult& Texture = Created.Resources.AddDefaulted_GetRef();

	Texture.TargetAssetPath = Fixture.Base + TEXT("/T_Frame.T_Frame");
	Texture.bImported = FindObject<UTexture2D>(nullptr, *Texture.TargetAssetPath) != nullptr;

	ON_SCOPE_EXIT
	{
		AutoBlueprintP5Tests::ReleaseUnsaved(Created);
	};

	if (!TestTrue(TEXT("Real UnLua generates nested containers without saving"), Script.bSuccess))
	{
		AddError(Script.Message);
		return false;
	}

	UWidgetBlueprint* Blueprint = FindObject<UWidgetBlueprint>(nullptr, *Created.Widget.TargetAssetPath);

	if (TestNotNull(TEXT("Lua created Widget"), Blueprint))
	{
		AutoBlueprintP5Tests::Inspect(*this, Blueprint->WidgetTree, true);
	}

	TestFalse(TEXT("Lua no-save leaves no Widget file"), FPackageName::DoesPackageExist(Fixture.Base + TEXT("/WBP_Nested")));
	TestFalse(TEXT("Lua no-save leaves no texture file"), FPackageName::DoesPackageExist(Fixture.Base + TEXT("/T_Frame")));
	return true;
}

#endif
