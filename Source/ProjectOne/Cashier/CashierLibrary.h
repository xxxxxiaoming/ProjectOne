#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Cashier/CashierDirector.h"
#include "CashierLibrary.generated.h"

UCLASS()
class PROJECTONE_API UCashierLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Cashier", meta = (WorldContext = "WorldContextObject"))
	static ACashierDirector* GetDirector(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Cashier")
	static int32 LoadBestScore();

	/** Writes the score when it is strictly greater than the stored best. */
	UFUNCTION(BlueprintCallable, Category = "Cashier")
	static bool CommitBestScore(int32 Score);

	/** Locked action names. Editor IMC/IA assets should use these strings. */
	UFUNCTION(BlueprintPure, Category = "Cashier|Input")
	static TArray<FName> GetLogicalActionNames();

	UFUNCTION(BlueprintPure, Category = "Cashier")
	static bool IsLegalDenom(int32 Value);
};
