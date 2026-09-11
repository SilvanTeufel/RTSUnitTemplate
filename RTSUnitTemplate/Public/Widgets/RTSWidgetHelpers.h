// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSWidgetHelpers.generated.h"

class UComboBoxString;
class UUserWidget;
class UDataTable;

/**
 * Kleine Helfer fuer Widget-Blueprints, die sich aus dem Blueprint heraus nicht sauber
 * schreiben lassen.
 *
 * Hintergrund zu SetComboBoxOptions: UComboBoxString und UComboBoxKey haben beide
 * ClearOptions/AddOption, und die Blueprint-Knoten teilen sich denselben Bezeichner.
 * Beim programmatischen Erzeugen eines Graphen laesst sich deshalb nicht bestimmen,
 * welche der beiden gemeint ist - der Aufloeser greift zur Key-Variante und die
 * Verbindung schlaegt fehl. Ein eindeutig benannter Aufruf umgeht das.
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSWidgetHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Ersetzt die Eintraege einer ComboBox (String) in einem Zug.
	 *
	 * @param Box       Die Auswahlliste. Nullptr wird still ignoriert.
	 * @param Options   Die neuen Eintraege, in dieser Reihenfolge.
	 * @param bSelectFirst  Waehlt danach den ersten Eintrag aus, damit die Liste nie
	 *                      ohne Auswahl dasteht.
	 * @return Die Zahl der gesetzten Eintraege.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Widgets")
	static int32 SetComboBoxOptions(UComboBoxString* Box, const TArray<FString>& Options,
	                                bool bSelectFirst = true);

	/**
	 * Hebt den aktiven Reiter einer Knopfleiste hervor und daempft die uebrigen.
	 *
	 * Sucht im Widgetbaum von Owner alle Widgets, deren Name mit Prefix beginnt, und
	 * faerbt genau das mit dem Namen Prefix+ActiveSuffix hell ein. Damit braucht der
	 * Blueprint einen einzigen Knoten statt einer if-Kette pro Reiter.
	 *
	 * @param Owner   Das Widget mit der Leiste (per Vorgabe self).
	 * @param Prefix  Namensvorsilbe der Reiter, z. B. "Tab".
	 * @param ActiveSuffix  Der Rest des Namens des aktiven Reiters, z. B. "Campaign".
	 * @return Die Zahl der gefundenen Reiter.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Widgets",
	          meta = (DefaultToSelf = "Owner", AdvancedDisplay = "Active,Inactive"))
	static int32 HighlightTabs(UUserWidget* Owner, const FString& Prefix,
	                           const FString& ActiveSuffix,
	                           FLinearColor Active = FLinearColor(1.f, 1.f, 1.f, 1.f),
	                           FLinearColor Inactive = FLinearColor(0.30f, 0.40f, 0.45f, 0.75f));

	/**
	 * Klartextname eines Teams fuer die Anzeige, z. B. 1 -> "Xeno (Team 1)".
	 *
	 * Die Lobby zeigte bisher nur die nackte Zahl. Der Name muss zurueckuebersetzbar
	 * bleiben, deshalb steht die Id immer in Klammern am Ende - TeamIdFromLabel liest
	 * genau die wieder aus.
	 */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Widgets")
	static FString TeamLabel(int32 TeamId);

	/** Liest die Id aus einem mit TeamLabel erzeugten Namen. -1, wenn keine drin steht. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Widgets")
	static int32 TeamIdFromLabel(const FString& Label);

	/**
	 * Fuellt eine Auswahlliste mit den Klartextnamen der uebergebenen Teams.
	 *
	 * @param Box       Die Auswahlliste.
	 * @param TeamIds   Die erlaubten Teams, in dieser Reihenfolge. Leer = 1 bis 4.
	 * @param bSelectFirst  Waehlt danach den ersten Eintrag - so ist bei Kampagnen- und
	 *                  Koop-Karten mit nur einem erlaubten Team dieses gleich gesetzt.
	 * @return Die Zahl der Eintraege.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Widgets")
	static int32 SetTeamComboOptions(UComboBoxString* Box, const TArray<int32>& TeamIds,
	                                 bool bSelectFirst = true);

	/**
	 * Fuellt die Teamauswahl mit den Teams, die die gewaehlte Karte zulaesst.
	 *
	 * Sucht in MapTable die Zeile, deren MapsDataAsset denselben mapName traegt wie
	 * SelectedMapName, liest dessen allowedTeamIds und setzt die Auswahlliste darauf.
	 * Ist nichts hinterlegt, bleiben alle vier Teams zur Wahl.
	 *
	 * Die Felder liegen an einem Blueprint-DataAsset und werden deshalb ueber Reflection
	 * gelesen - so bleibt der Aufruf im Widget ein einziger Knoten.
	 *
	 * @return Die Zahl der Eintraege, 0 wenn die Karte nicht gefunden wurde.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Widgets")
	static int32 ApplyAllowedTeamsFromTable(UComboBoxString* TeamBox, UDataTable* MapTable,
	                                        const FString& SelectedMapName,
	                                        bool bSelectFirst = true);
};
