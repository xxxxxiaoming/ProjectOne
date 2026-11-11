#include "Widgets/WidgetTypeRegistry.h"
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
		static const FWidgetTypeDescriptor Types[] = {
			{ TEXT("CanvasPanel"), TEXT("Canvas"), EChildPolicy::Panel, &UCanvasPanel::StaticClass },
			{ TEXT("Overlay"), TEXT("Overlay"), EChildPolicy::Panel, &UOverlay::StaticClass },
			{ TEXT("HorizontalBox"), TEXT("HorizontalBox"), EChildPolicy::Panel, &UHorizontalBox::StaticClass },
			{ TEXT("VerticalBox"), TEXT("VerticalBox"), EChildPolicy::Panel, &UVerticalBox::StaticClass },
			{ TEXT("Border"), TEXT("Border"), EChildPolicy::Single, &UBorder::StaticClass },
			{ TEXT("SizeBox"), TEXT("SizeBox"), EChildPolicy::Single, &USizeBox::StaticClass },
			{ TEXT("ScaleBox"), TEXT("ScaleBox"), EChildPolicy::Single, &UScaleBox::StaticClass },
			{ TEXT("Button"), TEXT("Button"), EChildPolicy::Single, &UButton::StaticClass },
			{ TEXT("Image"), TEXT(""), EChildPolicy::Leaf, &UImage::StaticClass },
			{ TEXT("TextBlock"), TEXT(""), EChildPolicy::Leaf, &UTextBlock::StaticClass },
			{ TEXT("Spacer"), TEXT(""), EChildPolicy::Leaf, &USpacer::StaticClass },
		};
		for (const FWidgetTypeDescriptor& Type : Types)
		{
			if (Name == Type.Name) { return &Type; }
		}
		return nullptr;
	}
}
