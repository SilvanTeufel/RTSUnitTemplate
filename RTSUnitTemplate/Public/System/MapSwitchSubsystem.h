#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MapSwitchSubsystem.generated.h"

/**
 * Subsystem zur Verwaltung aktivierter MapSwitch-Tags pro Map.
 * Persistiert während der laufenden Session über Level-Travels hinweg.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMapSwitchSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Map Switch")
    void MarkSwitchEnabledForMap(const FString& MapLongPackageName, FName SwitchTag);

    UFUNCTION(BlueprintCallable, Category = "Map Switch")
    bool IsSwitchEnabledForMap(const FString& MapLongPackageName, FName SwitchTag) const;

private:
    // Map-Key wird intern normalisiert (Asset-Name ohne PIE-Präfix)
    FString NormalizeMapKey(const FString& MapIdentifier) const;

    // Key: normalisierter Mapname (z.B. "LevelFive"), Value: aktivierte Switch-Tags
    // Kein UPROPERTY, da verschachtelte Container von UHT nicht unterstützt werden und keine GC/Reflection nötig ist.
    TMap<FString, TSet<FName>> EnabledSwitchTagsByMap;
};
