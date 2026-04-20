#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSBeaconSubsystem.generated.h"

USTRUCT()
struct FRTSBeaconInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Range = 0.f;
};

/**
 * Subsystem to store and query active beacons (BuildingBase and EffectArea with BeaconRange > 0)
 * Updated by UMassBeaconProcessor
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSBeaconSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Updates the list of active beacons. Called by UMassBeaconProcessor. */
	void UpdateBeacons(TArray<FRTSBeaconInfo>&& InBeacons);

	/** Returns true if the given location is within range of any active beacon. */
	UFUNCTION(BlueprintCallable, Category = "RTS|Beacon")
	bool IsLocationInBeaconRange(const FVector& Location) const;

private:
	UPROPERTY()
	TArray<FRTSBeaconInfo> ActiveBeacons;
};
