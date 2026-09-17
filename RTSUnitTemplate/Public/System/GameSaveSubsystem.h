// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Save/RTSSaveGame.h"
#include "GameSaveSubsystem.generated.h"

class URTSSaveGame;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUnitSaveLoad, AUnitBase*, FUnitSaveData&);

/**
 * Subsystem zum Speichern und Laden von Spielzuständen inkl. Map-Wechsel.
 */
UCLASS()
class RTSUNITTEMPLATE_API UGameSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Holt NUR die Freischaltungen der MapSwitch-Ziele aus dem juengsten Spielstand.
     *
     * Wofuer: startet man das Spiel neu, steht man im Menue wieder vor verschlossenen Portalen,
     * obwohl der Fortschritt laengst gespeichert ist. Der Zustand liegt im UMapSwitchSubsystem
     * und wandert zwar in jeden Spielstand, aber bisher kam er nur ueber LoadGameFromSlot
     * zurueck - und das REIST zur gespeicherten Karte. Wer im Menue bleiben will, kann ihn also
     * gar nicht holen.
     *
     * Deshalb dieser schmale Weg: er liest ausschliesslich MapEnabledSwitchTags und ruehrt
     * Einheiten, Ressourcen und Karte nicht an. Das Menuelevel bleibt stehen.
     *
     * "Juengster" heisst hier woertlich der groesste SavedUnixTimeSeconds - NICHT der
     * Spielstand mit dem weitesten Fortschritt. Wer zurueckspringt, soll auch zurueckspringen.
     *
     * @return true, wenn ein Spielstand gefunden und uebernommen wurde.
     */
    UFUNCTION(BlueprintCallable, Category="Save")
    bool RestoreUnlocksFromLatestSave();

    /**
     * Beginnt ein neues Spiel, OHNE etwas zu loeschen.
     *
     * Legt einen frischen Spielstand ohne Fortschritt an. Weil er den juengsten Zeitstempel
     * traegt, ist er der, den RestoreUnlocksFromLatestSave beim naechsten Start findet - die
     * Portale stehen also wieder zu. Die bisherigen Spielstaende bleiben liegen und lassen sich
     * jederzeit wieder laden.
     *
     * Bewusst NICHT ueber SaveCurrentGame: das schriebe den Zustand der gerade offenen Karte
     * mit hinein - im Menue also dessen Aktoren. Ein neues Spiel soll leer sein, nicht ein
     * Abbild des Menues.
     *
     * @param SlotName Name des neuen Spielstands. Leer nimmt einen eindeutigen "NewGame"-Namen.
     * @return Der tatsaechlich verwendete Name, oder leer bei Fehlschlag.
     */
    UFUNCTION(BlueprintCallable, Category="Save")
    FString StartNewGame(const FString& SlotName);

    /**
     * Loescht ALLE Spielstaende und setzt die Freischaltungen zurueck - neues Spiel.
     *
     * Entfernt die .sav-Dateien, die IstSpielstandDatei als Spielstand erkennt. Alles andere im
     * Ordner (Faehigkeiten je Einheit, Replays, Indexdateien) bleibt unberuehrt; dort liegen
     * gemessen ueber siebentausend Dateien, von denen die wenigsten Spielstaende sind.
     *
     * @return Anzahl der geloeschten Spielstaende.
     */
    UFUNCTION(BlueprintCallable, Category="Save")
    int32 ResetAllProgress();

    FOnUnitSaveLoad OnUnitSave;
    FOnUnitSaveLoad OnUnitLoad;

    UFUNCTION(BlueprintCallable, Category="Save")
    void SaveCurrentGame(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category="Save")
    void LoadGameFromSlot(const FString& SlotName);

    // Liefert alle vorhandenen Slots (Dateinamen ohne .sav)
    UFUNCTION(BlueprintCallable, Category="Save")
    TArray<FString> GetAllSaveSlots() const;

    // Liest Metadaten eines Slots (gibt true bei Erfolg)
    UFUNCTION(BlueprintCallable, Category="Save")
    /**
     * Sieht nur in den Kopf der Datei: sieht das nach einem Spielstand aus?
     *
     * Ohne diese Vorpruefung laedt LoadSaveSummary JEDE Datei im Speicherordner vollstaendig -
     * auch Replays von ueber 30 MB. Genau daran hing die lange Wartezeit des SaveGame-Widgets.
     */
    bool IstSpielstandDatei(const FString& SlotName) const;

    bool LoadSaveSummary(const FString& SlotName, FString& OutMapAssetName, FString& OutLongPackageName, int64& OutUnixTime) const;

    UFUNCTION(BlueprintCallable, Category="Save")
    void SetPendingQuickSave(bool bPending);

    UFUNCTION(BlueprintCallable, Category="Save")
    FString GetUniqueSaveSlotName(const FString& BaseName) const;

    // Fallback-Klasse zum Spawn fehlender Einheiten (im Editor/INI konfigurierbar)
    UPROPERTY(EditDefaultsOnly, Category="Save")
    TSubclassOf<AUnitBase> DefaultUnitClass;

private:
    UPROPERTY()
    URTSSaveGame* PendingLoadedSave = nullptr;

    FString PendingSlotName;

    bool bPendingQuickSave = false;

    void ApplyLoadedData(UWorld* LoadedWorld, URTSSaveGame* SaveData);

    /** Einmal je Sitzung: die Freischaltungen sind schon geholt. */
    bool bUnlocksRestored = false;

    // Callback wenn eine Map geladen wurde
    void OnPostLoadMapWithWorld(UWorld* LoadedWorld);
};
