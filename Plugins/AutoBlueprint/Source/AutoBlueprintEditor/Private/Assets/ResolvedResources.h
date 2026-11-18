#pragma once

#include "AutoBlueprintTypes.h"
#include "UObject/GCObject.h"

class UTexture2D;
class UFont;

namespace AutoBlueprint
{
	struct FLayoutSpec;
	struct FLayoutNode;

	/** Task-local strong references, including resources not yet referenced by a Widget template. */
	class FResolvedResources final : public FGCObject
	{
	public:
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override;

		bool Resolve(const FLayoutSpec& Layout, FAutoBlueprintGenerationResult& Result, bool bSaveRequested);
		bool SaveImports(FAutoBlueprintGenerationResult& Result);
		UTexture2D* GetTexture(const FString& Alias) const;
		UFont* GetFont(const FString& Path) const;

	private:
		bool LoadFonts(const FLayoutNode& Node, TArray<FAutoBlueprintDiagnostic>& Diagnostics);

		TMap<FString, UTexture2D*> Textures;
		TMap<FString, UFont*> Fonts;
	};
}
