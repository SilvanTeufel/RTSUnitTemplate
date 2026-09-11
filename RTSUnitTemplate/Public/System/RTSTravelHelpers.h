// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSTravelHelpers.generated.h"

/**
 * Levelwechsel, der im Mehrspieler alle Mitspieler mitnimmt.
 *
 * Hintergrund: BP_StoryStage_AH reiste bisher mit OpenLevel weiter. OpenLevel wirkt nur
 * lokal - startet man ein Kampagnenlevel ueber die Lobby, landet der Host im Spiellevel
 * und die Clients bleiben im Storylevel stehen. ServerTravel nimmt alle mit, ist aber im
 * Einzelspieler nicht immer verfuegbar. Diese Funktion waehlt anhand des NetMode.
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSTravelHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Reist zu einer Karte - per ServerTravel im Netzwerkspiel, sonst per OpenLevel.
	 *
	 * @param WorldContextObject  Beliebiges Objekt aus der Welt.
	 * @param MapName   Kartenname oder voller Paketpfad. Ein bereits angehaengtes "?..."
	 *                  wird uebernommen.
	 * @param ExtraOptions  Zusaetzliche Optionen, mit oder ohne fuehrendes "?". Der
	 *                  GameMode muss hier NICHT stehen: die Kampagnenlevel tragen ihn in
	 *                  ihren WorldSettings.
	 * @return true, wenn der Reisebefehl abgesetzt wurde.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Travel",
	          meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "ExtraOptions"))
	static bool TravelToMap(const UObject* WorldContextObject, FName MapName,
	                        const FString& ExtraOptions = TEXT(""));
};
