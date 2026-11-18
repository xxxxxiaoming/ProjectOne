#include "Assets/CreateOnlyAssetWriter.h"

#include "AutoBlueprintLog.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

bool AutoBlueprint::SaveCreatedAsset(UObject& Asset, FString& Code, FString& Message)
{
	UPackage* Package = Asset.GetOutermost();
	const FString PackageName = Package->GetName();
	FString Filename;

	if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
	{
		Code = TEXT("INVALID_PACKAGE_PATH");
		Message = TEXT("Cannot resolve the package to a project asset filename.");
		return false;
	}

	// Recheck after compilation as editor callbacks may have created a disk asset.
	if (FPackageName::DoesPackageExist(PackageName) || IFileManager::Get().FileExists(*Filename))
	{
		Code = TEXT("ASSET_ALREADY_EXISTS");
		Message = TEXT("A disk asset appeared before saving. CreateOnly refuses to replace it.");
		return false;
	}
	if (IFileManager::Get().FileExists(*FPaths::GetPath(Filename))
		|| !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
	{
		Code = TEXT("PACKAGE_SAVE_FAILED");
		Message = TEXT("Cannot create the target asset directory: ") + FPaths::GetPath(Filename);
		return false;
	}

	FSavePackageArgs SaveArgs;

	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GWarn;
	if (!UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs))
	{
		Code = TEXT("PACKAGE_SAVE_FAILED");
		Message = TEXT("Unreal failed to save the new package: ") + Filename;
		return false;
	}

	// Preserve this evidence even if the calling Lua script throws after the save.
	UE_LOG(LogAutoBlueprint, Log, TEXT("Saved package: %s"), *PackageName);
	return true;
}
