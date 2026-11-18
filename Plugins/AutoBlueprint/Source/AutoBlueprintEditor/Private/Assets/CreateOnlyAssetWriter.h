#pragma once

#include "CoreMinimal.h"

namespace AutoBlueprint
{
	/** Save one newly created asset only. Caller owns it and has passed full preflight on the editor thread. */
	bool SaveCreatedAsset(UObject& Asset, FString& Code, FString& Message);
}
