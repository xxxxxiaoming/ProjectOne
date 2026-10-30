#include "Widgets/SmokeTestWidgetTree.h"

#include "AutoBlueprintTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "WidgetBlueprint.h"
#include "Widgets/Layout/Anchors.h"

namespace
{
	bool PlaceOnCanvas(UCanvasPanel& Canvas, UWidget& Child, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Child);
		if (!Slot)
		{
			return false;
		}
		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D::ZeroVector);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
		Slot->SetZOrder(0);
		return true;
	}
}

bool AutoBlueprint::BuildSmokeTestWidgetTree(UWidgetBlueprint& Blueprint, const FAutoBlueprintSmokeTestRequest& Request)
{
	if (!Blueprint.WidgetTree)
	{
		Blueprint.WidgetTree = NewObject<UWidgetTree>(&Blueprint, TEXT("WidgetTree"), RF_Transactional);
	}
	UWidgetTree* Tree = Blueprint.WidgetTree;
	if (!Tree || Tree->RootWidget)
	{
		return false;
	}

	// These are asset templates owned by WidgetTree, not widgets in a preview world.
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_Title"));
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BTN_Confirm"));
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_Confirm"));
	if (!Root || !Title || !Button || !Label)
	{
		return false;
	}

	Tree->RootWidget = Root;
	Root->bIsVariable = false;
	Title->bIsVariable = true;
	Button->bIsVariable = true;
	Label->bIsVariable = true;

	Title->SetText(FText::AsCultureInvariant(Request.TitleText));
	Label->SetText(FText::AsCultureInvariant(Request.ButtonText));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	Button->SetIsEnabled(true);

	if (!PlaceOnCanvas(*Root, *Title, FVector2D(64.0, 64.0), FVector2D(512.0, 48.0))
		|| !PlaceOnCanvas(*Root, *Button, FVector2D(64.0, 160.0), FVector2D(240.0, 64.0)))
	{
		return false;
	}

	UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(Label));
	if (!LabelSlot)
	{
		return false;
	}
	LabelSlot->SetPadding(FMargin(12.0f, 8.0f));
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	return true;
}
