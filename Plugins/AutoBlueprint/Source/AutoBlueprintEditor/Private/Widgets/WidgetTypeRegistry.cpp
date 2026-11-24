#include "Widgets/WidgetTypeRegistry.h"

#include "Widgets/WidgetBuildHandlers.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"

namespace AutoBlueprint
{
	const FWidgetTypeDescriptor* FindWidgetType(const FString& Name)
	{
		static const FWidgetTypeDescriptor Types[] =
		{
			{ TEXT("CanvasPanel"), TEXT("Canvas"), EChildPolicy::Panel, &UCanvasPanel::StaticClass, true, nullptr, nullptr, &ApplyCanvasChildSlot },
			{ TEXT("Overlay"), TEXT("Overlay"), EChildPolicy::Panel, &UOverlay::StaticClass, true, nullptr, nullptr, &ApplyOverlayChildSlot },
			{ TEXT("HorizontalBox"), TEXT("HorizontalBox"), EChildPolicy::Panel, &UHorizontalBox::StaticClass, true, nullptr, nullptr, &ApplyHorizontalBoxChildSlot },
			{ TEXT("VerticalBox"), TEXT("VerticalBox"), EChildPolicy::Panel, &UVerticalBox::StaticClass, true, nullptr, nullptr, &ApplyVerticalBoxChildSlot },
			{ TEXT("Border"), TEXT("Border"), EChildPolicy::Single, &UBorder::StaticClass, true, nullptr, &ApplyBorderProperties, &ApplyBorderChildSlot },
			{ TEXT("SizeBox"), TEXT("SizeBox"), EChildPolicy::Single, &USizeBox::StaticClass, true, nullptr, &ApplySizeBoxProperties, &ApplySizeBoxChildSlot },
			{ TEXT("ScaleBox"), TEXT("ScaleBox"), EChildPolicy::Single, &UScaleBox::StaticClass, true, nullptr, &ApplyScaleBoxProperties, &ApplyScaleBoxChildSlot },
			{ TEXT("Button"), TEXT("Button"), EChildPolicy::Single, &UButton::StaticClass, true, nullptr, &ApplyBasicButtonProperties, &ApplyButtonChildSlot },
			{ TEXT("Image"), TEXT(""), EChildPolicy::Leaf, &UImage::StaticClass, true, nullptr, &ApplyImageProperties, nullptr },
			{ TEXT("TextBlock"), TEXT(""), EChildPolicy::Leaf, &UTextBlock::StaticClass, true, nullptr, &ApplyBasicTextProperties, nullptr },
			{ TEXT("Spacer"), TEXT(""), EChildPolicy::Leaf, &USpacer::StaticClass, true, nullptr, &ApplySpacerProperties, nullptr },
		};

		for (const FWidgetTypeDescriptor& Type : Types)
		{
			if (Name == Type.Name)
			{
				return &Type;
			}
		}

		return nullptr;
	}
}
