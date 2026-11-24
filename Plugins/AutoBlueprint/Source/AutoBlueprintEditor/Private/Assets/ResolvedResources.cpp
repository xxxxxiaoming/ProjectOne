#include "Assets/ResolvedResources.h"

#include "Assets/CreateOnlyAssetWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "Layout/LayoutValidation.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

namespace AutoBlueprint
{
	namespace ResourceResolution
	{
		void Error(TArray<FAutoBlueprintDiagnostic>& Diagnostics, const FString& AssetPath,
			const FString& Code, const FString& Message)
		{
			FAutoBlueprintDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();

			Diagnostic.Code = Code;
			Diagnostic.AssetPath = AssetPath;
			Diagnostic.Message = Message;
		}

	}

	bool FResolvedResources::LoadFonts(const FLayoutNode& Node, TArray<FAutoBlueprintDiagnostic>& Diagnostics)
	{
		const TSharedPtr<FLayoutValue>* Value = Node.Properties.Fields.Find(TEXT("font"));

		if (Value && !Fonts.Contains((*Value)->String))
		{
			const FString& Path = (*Value)->String;
			UFont* Font = Cast<UFont>(FSoftObjectPath(Path).TryLoad());

			if (!Font)
			{
				ResourceResolution::Error(Diagnostics, Path, TEXT("RESOURCE_RESOLVE_FAILED"), TEXT("The validated UFont could not be loaded at build time."));
				return false;
			}

			Fonts.Add(Path, Font);
		}

		for (const FLayoutNode& Child : Node.Children)
		{
			if (!LoadFonts(Child, Diagnostics))
			{
				return false;
			}
		}

		return true;
	}

	void FResolvedResources::AddReferencedObjects(FReferenceCollector& Collector)
	{
		for (TPair<FString, UTexture2D*>& Item : Textures)
		{
			Collector.AddReferencedObject(Item.Value);
		}
		for (TPair<FString, UFont*>& Item : Fonts)
		{
			Collector.AddReferencedObject(Item.Value);
		}
	}

	FString FResolvedResources::GetReferencerName() const
	{
		return TEXT("AutoBlueprint::FResolvedResources");
	}

	UTexture2D* FResolvedResources::GetTexture(const FString& Alias) const
	{
		return Textures.FindChecked(Alias);
	}

	UFont* FResolvedResources::GetFont(const FString& Path) const
	{
		return Fonts.FindChecked(Path);
	}

	bool FResolvedResources::Resolve(const FLayoutSpec& Layout, FAutoBlueprintGenerationResult& Result, bool bSaveRequested)
	{
		for (const FLayoutResource& Resource : Layout.Resources)
		{
			FAutoBlueprintResourceResult& Item = Result.Resources.AddDefaulted_GetRef();

			Item.Alias = Resource.Alias;
			Item.SourceFile = Resource.SourceFile;
			Item.TargetAssetPath = Resource.ObjectPath.IsEmpty()
				? Resource.PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(Resource.PackagePath) : Resource.ObjectPath;
		}

		if (!LoadFonts(Layout.Root, Result.Diagnostics))
		{
			return false;
		}

		for (int32 Index = 0; Index < Layout.Resources.Num(); ++Index)
		{
			const FLayoutResource& Resource = Layout.Resources[Index];
			FAutoBlueprintResourceResult& Item = Result.Resources[Index];

			Item.bAttempted = true;
			if (!Resource.ObjectPath.IsEmpty())
			{
				// reference an existing UTexture2D.
				UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Resource.ObjectPath).TryLoad());

				if (!Texture)
				{
					ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, TEXT("RESOURCE_RESOLVE_FAILED"),
						TEXT("The validated UTexture2D could not be loaded at build time."));
					return false;
				}

				Textures.Add(Resource.Alias, Texture);
				Item.bReused = true;
				Item.bSuccess = true;
				continue;
			}

			// import to ue become texture asset.
			FString Full;
			TArray<uint8> Bytes;
			FAutoBlueprintValidationResult ReadResult;

			// Import exactly the bytes just checked; do not reopen the source inside the factory.
			if (!ReadValidatedSourceImage(Resource.SourceFile, TEXT("$.resources.") + Resource.Alias + TEXT(".source_file"), Full, Bytes, ReadResult))
			{
				Item.Diagnostics = MoveTemp(ReadResult.Diagnostics);
				return false;
			}

			const FString Filename = FPackageName::LongPackageNameToFilename(Resource.PackagePath, FPackageName::GetAssetPackageExtension());

			if (FindPackage(nullptr, *Resource.PackagePath) || FPackageName::DoesPackageExist(Resource.PackagePath)
				|| IFileManager::Get().FileExists(*Filename))
			{
				ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, TEXT("ASSET_ALREADY_EXISTS"),
					TEXT("The texture target appeared before import. CreateOnly refuses to replace it."));
				return false;
			}

			TStrongObjectPtr<UPackage> Package(CreatePackage(*Resource.PackagePath));

			if (!Package.IsValid())
			{
				ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, TEXT("RESOURCE_IMPORT_FAILED"), TEXT("Cannot create an import package."));
				return false;
			}

			Item.CreatedPackageName = Package->GetName();

			TStrongObjectPtr<UTextureFactory> Factory(NewObject<UTextureFactory>());

			if (!Factory.IsValid())
			{
				ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, TEXT("RESOURCE_IMPORT_FAILED"), TEXT("Cannot create an import factory. The package remains unsaved."));
				return false;
			}

			const uint8* Buffer = Bytes.GetData();
			UTexture2D* Texture = Cast<UTexture2D>(Factory->FactoryCreateBinary(UTexture2D::StaticClass(), Package.Get(),
				FName(*FPackageName::GetLongPackageAssetName(Resource.PackagePath)), RF_Public | RF_Standalone | RF_Transactional,
				nullptr, *FPaths::GetExtension(Full), Buffer, Bytes.GetData() + Bytes.Num(), GWarn));

			if (!Texture)
			{
				ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, TEXT("RESOURCE_IMPORT_FAILED"),
					TEXT("Unreal's texture factory failed. The new package remains unsaved."));
				return false;
			}

			Textures.Add(Resource.Alias, Texture);
			Item.bImported = true;

			// The factory can return while an async texture build is still running. Finish
			// the previous edit before changing settings used by its derived-data key.
			Texture->PreEditChange(nullptr);
			Texture->LODGroup = TEXTUREGROUP_UI;
			Texture->CompressionSettings = TC_EditorIcon;
			Texture->MipGenSettings = TMGS_NoMipmaps;
			Texture->SRGB = true;
			if (Texture->AssetImportData)
			{
				Texture->AssetImportData->Update(Full);
			}

			Texture->PostEditChange();

			UTexture* TexturesToFinish[] = { Texture };

			// This synchronous task needs final dimensions/resources for brushes and saving.
			// Drain only this new texture, including the manager's post-compilation work.
			FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
			FAssetRegistryModule::AssetCreated(Texture);
			Texture->MarkPackageDirty();
			Item.bSuccess = !bSaveRequested;
		}

		return true;
	}

	bool FResolvedResources::SaveImports(FAutoBlueprintGenerationResult& Result)
	{
		for (FAutoBlueprintResourceResult& Item : Result.Resources)
		{
			if (!Item.bImported)
			{
				continue;
			}

			FString Code;
			FString Message;

			Item.bSaved = SaveCreatedAsset(*Textures.FindChecked(Item.Alias), Code, Message);
			Item.bSuccess = Item.bSaved;
			if (!Item.bSaved)
			{
				ResourceResolution::Error(Item.Diagnostics, Item.TargetAssetPath, Code, Message);
				return false;
			}
		}

		return true;
	}
}
