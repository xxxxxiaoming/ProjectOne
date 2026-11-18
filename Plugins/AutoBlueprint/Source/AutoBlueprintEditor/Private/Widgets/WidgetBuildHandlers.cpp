#include "Widgets/WidgetBuildHandlers.h"

#include "AutoBlueprintTypes.h"
#include "Assets/ResolvedResources.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Components/Widget.h"
#include "Layout/LayoutSpec.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateEnums.h"
#include "Styling/SlateColor.h"
#include "Widgets/Layout/Anchors.h"

namespace AutoBlueprint
{
	namespace WidgetBuildValues
	{
		const FLayoutValue& Field(const FLayoutValue& Object, const TCHAR* Name)
		{
			return *Object.Fields.FindChecked(Name);
		}

		FVector2D Vector(const FLayoutValue& Value)
		{
			return FVector2D(Value.Elements[0].Number, Value.Elements[1].Number);
		}

		FMargin Margin(const FLayoutValue& Value)
		{
			return FMargin(static_cast<float>(Value.Elements[0].Number), static_cast<float>(Value.Elements[1].Number),
				static_cast<float>(Value.Elements[2].Number), static_cast<float>(Value.Elements[3].Number));
		}

		FLinearColor Color(const FLayoutValue& Value)
		{
			return FLinearColor(static_cast<float>(Value.Elements[0].Number), static_cast<float>(Value.Elements[1].Number),
				static_cast<float>(Value.Elements[2].Number), static_cast<float>(Value.Elements[3].Number));
		}

		FSlateBrush Brush(const FLayoutValue& Value, const FResolvedResources& Resources)
		{
			UTexture2D* Texture = Resources.GetTexture(Field(Value, TEXT("resource")).String);
			FSlateBrush Result;

			Result.SetResourceObject(Texture);
			Result.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Result.DrawAs = Field(Value, TEXT("draw_as")).String == TEXT("Box") ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
			Result.Margin = Margin(Field(Value, TEXT("margin")));
			Result.TintColor = FSlateColor(Color(Field(Value, TEXT("tint"))));
			return Result;
		}
	}

	void ApplyCommonWidgetProperties(UWidget& Widget, const FLayoutNode& Node)
	{
		const FLayoutValue& Properties = Node.Properties;
		const FString& Visibility = WidgetBuildValues::Field(Properties, TEXT("visibility")).String;
		ESlateVisibility SlateVisibility = ESlateVisibility::Visible;

		if (Visibility == TEXT("Collapsed"))
		{
			SlateVisibility = ESlateVisibility::Collapsed;
		}
		else if (Visibility == TEXT("Hidden"))
		{
			SlateVisibility = ESlateVisibility::Hidden;
		}
		else if (Visibility == TEXT("HitTestInvisible"))
		{
			SlateVisibility = ESlateVisibility::HitTestInvisible;
		}
		else if (Visibility == TEXT("SelfHitTestInvisible"))
		{
			SlateVisibility = ESlateVisibility::SelfHitTestInvisible;
		}

		Widget.bIsVariable = Node.bIsVariable;
		Widget.SetVisibility(SlateVisibility);
		Widget.SetIsEnabled(WidgetBuildValues::Field(Properties, TEXT("is_enabled")).bBoolean);
		Widget.SetRenderOpacity(static_cast<float>(WidgetBuildValues::Field(Properties, TEXT("render_opacity")).Number));
	}

	void ApplyBasicTextProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources)
	{
		UTextBlock& Text = *CastChecked<UTextBlock>(&Widget);
		const FLayoutValue& Properties = Node.Properties;
		const FLayoutValue& Color = WidgetBuildValues::Field(Properties, TEXT("color"));
		const FString& Justification = WidgetBuildValues::Field(Properties, TEXT("justification")).String;
		FSlateFontInfo Font = Text.GetFont();

		const TSharedPtr<FLayoutValue>* FontPath = Properties.Fields.Find(TEXT("font"));

		if (FontPath)
		{
			Font.FontObject = Resources.GetFont((*FontPath)->String);
			Font.TypefaceFontName = NAME_None;
		}

		Font.Size = static_cast<float>(WidgetBuildValues::Field(Properties, TEXT("font_size")).Number);
		Text.SetFont(Font);
		Text.SetText(FText::AsCultureInvariant(WidgetBuildValues::Field(Properties, TEXT("text")).String));
		Text.SetColorAndOpacity(FSlateColor(FLinearColor(static_cast<float>(Color.Elements[0].Number), static_cast<float>(Color.Elements[1].Number),
			static_cast<float>(Color.Elements[2].Number), static_cast<float>(Color.Elements[3].Number))));
		Text.SetJustification(Justification == TEXT("Center") ? ETextJustify::Center
			: Justification == TEXT("Right") ? ETextJustify::Right : ETextJustify::Left);
		Text.SetAutoWrapText(WidgetBuildValues::Field(Properties, TEXT("auto_wrap_text")).bBoolean);
	}

	void ApplyBasicButtonProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources)
	{
		UButton& Button = *CastChecked<UButton>(&Widget);

		const TSharedPtr<FLayoutValue>* StyleValue = Node.Properties.Fields.Find(TEXT("style"));

		if (StyleValue)
		{
			FButtonStyle Style = Button.GetStyle();
			const FLayoutValue& Value = **StyleValue;
			const TCHAR* Names[] = { TEXT("normal"), TEXT("hovered"), TEXT("pressed"), TEXT("disabled") };
			FSlateBrush* Brushes[] = { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled };

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
			{
				const TSharedPtr<FLayoutValue>* State = Value.Fields.Find(Names[Index]);

				if (State)
				{
					*Brushes[Index] = WidgetBuildValues::Brush(**State, Resources);
				}
			}

			Button.SetStyle(Style);
		}

		// UE 5.3 has no public UButton focus setter. Set the serialized template property
		// before a Slate instance exists; this never edits a live preview button.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Button.IsFocusable = WidgetBuildValues::Field(Node.Properties, TEXT("is_focusable")).bBoolean;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	void ApplyImageProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources)
	{
		UImage& Image = *CastChecked<UImage>(&Widget);
		const TSharedPtr<FLayoutValue>* Value = Node.Properties.Fields.Find(TEXT("brush"));

		if (Value)
		{
			Image.SetBrush(WidgetBuildValues::Brush(**Value, Resources));
		}
	}

	bool ApplyCanvasChildSlot(UPanelSlot& Slot, const FLayoutValue& Values)
	{
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(&Slot);

		if (!CanvasSlot)
		{
			return false;
		}

		const FLayoutValue& Anchors = WidgetBuildValues::Field(Values, TEXT("anchors"));
		const FVector2D Minimum = WidgetBuildValues::Vector(WidgetBuildValues::Field(Anchors, TEXT("min")));
		const FVector2D Maximum = WidgetBuildValues::Vector(WidgetBuildValues::Field(Anchors, TEXT("max")));

		CanvasSlot->SetAnchors(FAnchors(static_cast<float>(Minimum.X), static_cast<float>(Minimum.Y), static_cast<float>(Maximum.X), static_cast<float>(Maximum.Y)));
		CanvasSlot->SetOffsets(WidgetBuildValues::Margin(WidgetBuildValues::Field(Values, TEXT("offsets"))));
		CanvasSlot->SetAlignment(WidgetBuildValues::Vector(WidgetBuildValues::Field(Values, TEXT("alignment"))));
		CanvasSlot->SetAutoSize(WidgetBuildValues::Field(Values, TEXT("auto_size")).bBoolean);
		CanvasSlot->SetZOrder(static_cast<int32>(WidgetBuildValues::Field(Values, TEXT("z_order")).Number));
		return true;
	}

	bool ApplyButtonChildSlot(UPanelSlot& Slot, const FLayoutValue& Values)
	{
		UButtonSlot* ButtonSlot = Cast<UButtonSlot>(&Slot);

		if (!ButtonSlot)
		{
			return false;
		}

		const FString& Horizontal = WidgetBuildValues::Field(Values, TEXT("horizontal_alignment")).String;
		const FString& Vertical = WidgetBuildValues::Field(Values, TEXT("vertical_alignment")).String;

		ButtonSlot->SetPadding(WidgetBuildValues::Margin(WidgetBuildValues::Field(Values, TEXT("padding"))));
		ButtonSlot->SetHorizontalAlignment(Horizontal == TEXT("Left") ? HAlign_Left
			: Horizontal == TEXT("Center") ? HAlign_Center : Horizontal == TEXT("Right") ? HAlign_Right : HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(Vertical == TEXT("Top") ? VAlign_Top
			: Vertical == TEXT("Center") ? VAlign_Center : Vertical == TEXT("Bottom") ? VAlign_Bottom : VAlign_Fill);
		return true;
	}
}
