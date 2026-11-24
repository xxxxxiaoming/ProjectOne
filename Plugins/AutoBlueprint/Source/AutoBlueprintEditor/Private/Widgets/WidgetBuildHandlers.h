#pragma once

#include "CoreMinimal.h"

class UWidget;
class UPanelSlot;
struct FAutoBlueprintDiagnostic;

namespace AutoBlueprint
{
	class FResolvedResources;
	struct FLayoutNode;
	struct FLayoutValue;

	// Registered callbacks consume schema-normalized values only.
	void ApplyCommonWidgetProperties(UWidget& Widget, const FLayoutNode& Node);
	void ApplyBasicTextProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplyBasicButtonProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplyImageProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplyBorderProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplySizeBoxProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplyScaleBoxProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	void ApplySpacerProperties(UWidget& Widget, const FLayoutNode& Node, const FResolvedResources& Resources);
	bool ApplyCanvasChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyButtonChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyOverlayChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyHorizontalBoxChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyVerticalBoxChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyBorderChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplySizeBoxChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyScaleBoxChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
}
