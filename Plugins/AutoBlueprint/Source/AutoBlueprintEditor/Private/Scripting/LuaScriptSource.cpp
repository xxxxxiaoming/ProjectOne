#include "Scripting/LuaScriptSource.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

namespace
{
	bool IsIdentifier(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Ch = Value[Index];
			const bool bLetter = (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'));
			const bool bDigit = Ch >= TEXT('0') && Ch <= TEXT('9');
			if (!bLetter && Ch != TEXT('_') && !(Index > 0 && bDigit))
			{
				return false;
			}
		}
		return true;
	}

	bool HasValidComponents(const FString& Value, const TCHAR* Separator)
	{
		if (Value.IsEmpty() || Value.StartsWith(Separator) || Value.EndsWith(Separator))
		{
			return false;
		}
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, Separator, false);
		if (Parts.IsEmpty())
		{
			return false;
		}
		for (const FString& Part : Parts)
		{
			if (!IsIdentifier(Part))
			{
				return false;
			}
		}
		return true;
	}
}

/*
 * check validation of RelativePath (e.g contains character, numbers and '_' only, ends with '.lua')
 * combine Root & RelativePath together into a full script path which is OutPath
 */
bool AutoBlueprint::ResolveLuaScriptPath(const FString& Root, const FString& RelativePath,
	FString& OutPath, FString& OutCode, FString& OutMessage)
{
	OutPath.Reset();
	if (Root.IsEmpty() || RelativePath.Len() > 240 || !FPaths::IsRelative(RelativePath)
		|| !RelativePath.EndsWith(TEXT(".lua"), ESearchCase::CaseSensitive)
		|| !HasValidComponents(RelativePath.LeftChop(4), TEXT("/")))
	{
		OutCode = TEXT("INVALID_SCRIPT_PATH");
		OutMessage = TEXT("Use a relative .lua path below AutoBlueprint/Scripts/Lua. Components must be ASCII identifiers separated by forward slashes.");
		return false;
	}
	FString FullRoot = FPaths::ConvertRelativePathToFull(Root);
	FPaths::NormalizeDirectoryName(FullRoot);
	FullRoot += TEXT("/");
	FString Candidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(FullRoot, RelativePath));
	FPaths::NormalizeFilename(Candidate);
	if (!FPaths::CollapseRelativeDirectories(Candidate) || !Candidate.StartsWith(FullRoot, ESearchCase::CaseSensitive))
	{
		OutCode = TEXT("INVALID_SCRIPT_PATH");
		OutMessage = TEXT("The tool script must stay below AutoBlueprint/Scripts/Lua.");
		return false;
	}
	OutPath = MoveTemp(Candidate);
	return true;
}

bool AutoBlueprint::MakeLuaModulePath(const FString& ModuleName, FString& OutRelativePath)
{
	OutRelativePath.Reset();
	if (ModuleName.Len() > 128 || ModuleName.EndsWith(TEXT(".lua"), ESearchCase::CaseSensitive)
		|| !HasValidComponents(ModuleName, TEXT(".")))
	{
		return false;
	}
	OutRelativePath = ModuleName.Replace(TEXT("."), TEXT("/")) + TEXT(".lua");
	return true;
}

bool AutoBlueprint::LoadLuaScriptSource(const FString& Path, FString& OutSource, FString& OutCode, FString& OutMessage)
{
	OutSource.Reset();
	constexpr int64 MaxSourceBytes = 1024 * 1024;
	if (!IFileManager::Get().FileExists(*Path))
	{
		OutCode = TEXT("LUA_SCRIPT_NOT_FOUND");
		OutMessage = TEXT("Cannot locate Lua script: ") + Path;
		return false;
	}
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
	if (!Reader)
	{
		OutCode = TEXT("LUA_SCRIPT_READ_FAILED");
		OutMessage = TEXT("Cannot open Lua script: ") + Path;
		return false;
	}
	const int64 Size = Reader->TotalSize();
	if (Size < 0 || Size > MaxSourceBytes)
	{
		OutCode = TEXT("LUA_SCRIPT_TOO_LARGE");
		OutMessage = TEXT("Lua tool sources must have a known size no larger than 1 MiB: ") + Path;
		return false;
	}
	if (Size == 0)
	{
		return true;
	}
	// Bound the allocation and read even if the file changes after the size check.
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(static_cast<int32>(Size));
	Reader->Serialize(Bytes.GetData(), Size);
	if (Reader->IsError())
	{
		OutCode = TEXT("LUA_SCRIPT_READ_FAILED");
		OutMessage = TEXT("Cannot read Lua script: ") + Path;
		return false;
	}
	FFileHelper::BufferToString(OutSource, Bytes.GetData(), Bytes.Num());
	return true;
}
