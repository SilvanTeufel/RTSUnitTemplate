// FogRevealerInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FogRevealerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UFogRevealerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can reveal fog of war.
 */
class RTSUNITTEMPLATE_API IFogRevealerInterface
{
	GENERATED_BODY()

public:
	// Declare BlueprintNativeEvent functions. Implementations go in .cpp with _Implementation.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Fog Of War")
	FVector GetRevealerLocation() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Fog Of War")
	float GetVisionRadius() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Fog Of War")
	bool IsRevealerActive() const;
};
