#include "AutoBlueprintEditorContext.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

bool AutoBlueprint::CheckEditorContext(FString& OutCode, FString& OutMessage)
{
	if (!IsInGameThread())
	{
		OutCode = TEXT("WRONG_THREAD");
		OutMessage = TEXT("AutoBlueprint must run on the editor game thread.");
		return false;
	}
	if (!GIsEditor || !GEditor || IsRunningCommandlet())
	{
		OutCode = TEXT("EDITOR_UNAVAILABLE");
		OutMessage = TEXT("P0 requires an interactive Unreal Editor session.");
		return false;
	}
	if (GEditor->IsPlaySessionInProgress() || IsGarbageCollecting() || GIsSavingPackage)
	{
		OutCode = TEXT("EDITOR_BUSY");
		OutMessage = TEXT("Stop PIE/SIE and wait for garbage collection or package saving before running AutoBlueprint.");
		return false;
	}
	return true;
}
