// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/RTSTravelHelpers.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Core/RTSUnitTemplateSettings.h"
#include "Widgets/LoadingWidget.h"
#include "Blueprint/UserWidget.h"


namespace
{
	/**
	 * Ladebildschirm vor die Reise setzen - bei ALLEN Mitspielern.
	 *
	 * Ohne das bleibt das zuletzt gezeichnete Bild stehen, bis die neue Karte geladen ist:
	 * ServerTravel setzt nur World->NextURL, geladen wird auf einem spaeteren
	 * TickWorldTravel, und bis dahin rendert das alte Level weiter. Beim Storylevel waren
	 * das gemessen 10-15 Sekunden Standbild (beim zweiten Mal weniger, weil vieles schon
	 * im Cache liegt). Dieselbe Schleife steht in ACameraControllerBase fuer den
	 * MapSwitch-Weg; hier fehlte sie.
	 */
	void ZeigeLadebildschirm(UWorld* Welt)
	{
		if (!Welt)
		{
			return;
		}

		// Erst der eingebaute Weg: ACameraControllerBase kennt seine eigene Widgetklasse und
		// erreicht ueber die Client-RPC auch die Mitspieler.
		bool bIrgendwerErreicht = false;
		for (FConstPlayerControllerIterator It = Welt->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get()))
			{
				PC->Client_ShowTravelLoadingScreen();
				bIrgendwerErreicht = true;
			}
		}

		if (bIrgendwerErreicht)
		{
			return;
		}

		// Storylevel laufen mit APlayerController und AGameStateBase - dort greift weder der
		// Cast noch der GameState-Rueckfall. Deshalb hier direkt beim oertlichen Spieler.
		const URTSUnitTemplateSettings* Einstellungen = URTSUnitTemplateSettings::Get();
		if (!Einstellungen || Einstellungen->TravelLoadingWidgetClass.IsNull())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Reise] Kein Ladebildschirm: weder ein ACameraControllerBase noch ein "
				     "TravelLoadingWidgetClass in den Projekteinstellungen."));
			return;
		}

		UClass* WidgetKlasse = Einstellungen->TravelLoadingWidgetClass.LoadSynchronous();
		if (!WidgetKlasse)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Reise] TravelLoadingWidgetClass laesst sich nicht laden."));
			return;
		}

		for (FConstPlayerControllerIterator It = Welt->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController())
			{
				continue;
			}

			if (UUserWidget* Ladebild = CreateWidget<UUserWidget>(PC, WidgetKlasse))
			{
				// Hoher ZOrder, damit es ueber allem liegt, was aus der alten Karte noch steht.
				Ladebild->AddToViewport(1000);
			}
		}
	}
}

bool URTSTravelHelpers::TravelToMap(const UObject* WorldContextObject, FName MapName,
                                    const FString& ExtraOptions)
{
	if (MapName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reise] Kein Kartenname gesetzt - kein Wechsel."));
		return false;
	}

	UWorld* Welt = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!Welt)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reise] Keine Welt zu '%s' - kein Wechsel."),
			*MapName.ToString());
		return false;
	}

	FString Optionen = ExtraOptions;
	if (!Optionen.IsEmpty() && !Optionen.StartsWith(TEXT("?")))
	{
		Optionen = TEXT("?") + Optionen;
	}

	// Nur der Server darf reisen, und nur im Netzwerkspiel nimmt ServerTravel alle mit.
	// Im Einzelspieler (NM_Standalone) ist OpenLevel der richtige Weg.
	const ENetMode Modus = Welt->GetNetMode();
	const bool bNetzwerk = (Modus == NM_ListenServer || Modus == NM_DedicatedServer);

	if (bNetzwerk)
	{
		const FString Reise = MapName.ToString() + Optionen;
		UE_LOG(LogTemp, Log, TEXT("[Reise] ServerTravel nach '%s' (NetMode %d)."),
			*Reise, static_cast<int32>(Modus));
		ZeigeLadebildschirm(Welt);
		Welt->ServerTravel(Reise, /*bAbsolute=*/true);
		return true;
	}

	if (Modus == NM_Client)
	{
		// Ein Client darf den Wechsel nicht selbst ausloesen - er wuerde die Sitzung
		// verlassen. Der Server reist, der Client wird mitgenommen.
		UE_LOG(LogTemp, Log, TEXT("[Reise] Client wartet auf den ServerTravel nach '%s'."),
			*MapName.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[Reise] OpenLevel nach '%s%s' (Einzelspieler)."),
		*MapName.ToString(), *Optionen);
	ZeigeLadebildschirm(Welt);
	UGameplayStatics::OpenLevel(WorldContextObject, MapName, /*bAbsolute=*/true, Optionen);
	return true;
}
