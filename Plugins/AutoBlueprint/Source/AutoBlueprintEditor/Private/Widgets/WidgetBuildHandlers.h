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
	bool ApplyCanvasChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyButtonChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
}
