#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSaveSubsystem.generated.h"

class URTSSaveGame;
class AUnitBase;

/**
 * Subsystem zum Speichern und Laden von Spielzuständen inkl. Map-Wechsel.
 */
UCLASS()
class RTSUNITTEMPLATE_API UGameSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Save")
    void SaveCurrentGame(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category="Save")
    void LoadGameFromSlot(const FString& SlotName);

    // Liefert alle vorhandenen Slots (Dateinamen ohne .sav)
    UFUNCTION(BlueprintCallable, Category="Save")
    TArray<FString> GetAllSaveSlots() const;

    // Liest Metadaten eines Slots (gibt true bei Erfolg)
    UFUNCTION(BlueprintCallable, Category="Save")
    bool LoadSaveSummary(const FString& SlotName, FString& OutMapAssetName, FString& OutLongPackageName, int64& OutUnixTime) const;

    // Fallback-Klasse zum Spawn fehlender Einheiten (im Editor/INI konfigurierbar)
    UPROPERTY(EditDefaultsOnly, Category="Save")
    TSubclassOf<AUnitBase> DefaultUnitClass;

private:
    UPROPERTY()
    URTSSaveGame* PendingLoadedSave = nullptr;

    FString PendingSlotName;

    void ApplyLoadedData(UWorld* LoadedWorld, URTSSaveGame* SaveData);

    // Callback wenn eine Map geladen wurde
    void OnPostLoadMapWithWorld(UWorld* LoadedWorld);
};
