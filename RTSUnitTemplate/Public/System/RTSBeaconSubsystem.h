// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

	/**
	 * Position und Reichweite des naechstgelegenen Beacons. Fuer die KI-Platzierung: eine Flaeche mit
	 * NeedsBeacon wird abgewiesen, wenn kein Beacon in Reichweite ist, und die Umkreissuche der KI
	 * kreist um den urspruenglichen Punkt - liegt dort keines, scheitert jeder Kandidat. Mit dieser
	 * Abfrage kann die Suche stattdessen um das Beacon kreisen.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Beacon")
	bool GetNearestBeacon(const FVector& Location, FVector& OutBeaconLocation, float& OutRange) const;

private:
	UPROPERTY()
	TArray<FRTSBeaconInfo> ActiveBeacons;
};
