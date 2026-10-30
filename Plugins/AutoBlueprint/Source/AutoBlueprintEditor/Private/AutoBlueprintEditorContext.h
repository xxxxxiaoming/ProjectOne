#pragma once

#include "CoreMinimal.h"

namespace AutoBlueprint
{
	/** Read-only check; performs the thread check before accessing GEditor. */
	bool CheckEditorContext(FString& OutCode, FString& OutMessage);
}
