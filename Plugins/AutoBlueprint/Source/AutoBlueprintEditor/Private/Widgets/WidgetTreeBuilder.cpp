#include "Widgets/WidgetTreeBuilder.h"

#include "AutoBlueprintTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Layout/LayoutSpec.h"
#include "WidgetBlueprint.h"
#include "Widgets/WidgetBuildHandlers.h"
#include "Widgets/WidgetTypeRegistry.h"

namespace AutoBlueprint
{
	namespace WidgetTreeBuild
	{
		void AddDiagnostic(TArray<FAutoBlueprintDiagnostic>& Diagnostics, const FLayoutNode& Node,
			const TCHAR* Code, const FString& Message)
		{
			FAutoBlueprintDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();

			Diagnostic.Code = Code;
			Diagnostic.JsonPath = Node.JsonPath;
			Diagnostic.WidgetName = Node.Name.ToString();
			Diagnostic.Message = Message;
		}

		bool ValidateNode(const FLayoutNode& Node, TArray<FAutoBlueprintDiagnostic>& Diagnostics)
		{
			const FWidgetTypeDescriptor* Type = FindWidgetType(Node.Type);

			if (!Type || !Type->bCanBuild)
			{
				AddDiagnostic(Diagnostics, Node, TEXT("BUILD_WIDGET_NOT_IMPLEMENTED"),
					TEXT("P4 builds CanvasPanel, TextBlock, Button and Image. This protocol type has no build handler yet."));
				return false;
			}
			if (Type->ValidateBuild && !Type->ValidateBuild(Node, Diagnostics))
			{
				return false;
			}

			for (const FLayoutNode& Child : Node.Children)
			{
				if (!ValidateNode(Child, Diagnostics))
				{
					return false;
				}
			}

			return true;
		}

		UWidget* ConstructNode(UWidgetTree& Tree, const FLayoutNode& Node, UPanelWidget* Parent,
			const FWidgetTypeDescriptor* ParentType, const FResolvedResources& Resources, TArray<FAutoBlueprintDiagnostic>& Diagnostics)
		{
			const FWidgetTypeDescriptor* Type = FindWidgetType(Node.Type);
			UWidget* Widget = Tree.ConstructWidget<UWidget>(Type->GetClass(), Node.Name);

			if (!Widget)
			{
				AddDiagnostic(Diagnostics, Node, TEXT("WIDGET_TREE_BUILD_FAILED"), TEXT("Cannot construct the requested widget template."));
				return nullptr;
			}

			// Attach immediately so the Blueprint-owned tree retains every successfully created template.
			if (Parent)
			{
				UPanelSlot* Slot = Parent->AddChild(Widget);

				if (!Slot || !ParentType->ApplyChildSlot || !ParentType->ApplyChildSlot(*Slot, Node.Slot))
				{
					AddDiagnostic(Diagnostics, Node, TEXT("WIDGET_TREE_BUILD_FAILED"), TEXT("The parent did not create the registered child Slot."));
					return nullptr;
				}
			}
			else
			{
				Tree.RootWidget = Widget;
			}

			ApplyCommonWidgetProperties(*Widget, Node);
			if (Type->ApplyProperties)
			{
				Type->ApplyProperties(*Widget, Node, Resources);
			}

			UPanelWidget* Panel = Cast<UPanelWidget>(Widget);

			for (const FLayoutNode& Child : Node.Children)
			{
				if (!Panel || !ConstructNode(Tree, Child, Panel, Type, Resources, Diagnostics))
				{
					return nullptr;
				}
			}

			return Widget;
		}
	}

	bool ValidateLayoutBuildCapabilities(const FLayoutSpec& Layout, TArray<FAutoBlueprintDiagnostic>& Diagnostics)
	{
		return WidgetTreeBuild::ValidateNode(Layout.Root, Diagnostics);
	}

	bool BuildLayoutWidgetTree(UWidgetBlueprint& Blueprint, const FLayoutSpec& Layout, const FResolvedResources& Resources,
		TArray<FAutoBlueprintDiagnostic>& Diagnostics)
	{
		if (!Blueprint.WidgetTree)
		{
			Blueprint.WidgetTree = NewObject<UWidgetTree>(&Blueprint, TEXT("WidgetTree"), RF_Transactional);
		}
		if (!Blueprint.WidgetTree || Blueprint.WidgetTree->RootWidget)
		{
			WidgetTreeBuild::AddDiagnostic(Diagnostics, Layout.Root, TEXT("WIDGET_TREE_BUILD_FAILED"),
				TEXT("The new Widget Blueprint must have an empty template tree."));
			return false;
		}

		return WidgetTreeBuild::ConstructNode(*Blueprint.WidgetTree, Layout.Root, nullptr, nullptr, Resources, Diagnostics) != nullptr;
	}
}
