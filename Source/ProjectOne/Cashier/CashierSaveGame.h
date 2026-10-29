#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CashierSaveGame.generated.h"

UCLASS()
class PROJECTONE_API UCashierShiftSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Cashier")
	int32 BestScore = 0;
};
