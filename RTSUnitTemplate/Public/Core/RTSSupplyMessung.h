// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/RTSDateiMessung.h"
#include "Engine/World.h"

// SUPPLY-BUCHUNGEN LUECKENLOS MITSCHREIBEN (25.09.2026).
//
// Anlass: der Verbrauch ("Rare") stand am Partieende auf 0, obwohl Einheiten lebten - es wird
// also irgendwo mehr zurueckgegeben als genommen. Zwei Erklaerungen aus reiner Quelltextlesung
// haben schon nicht getragen; deshalb wird jetzt JEDE Buchung protokolliert statt weiter geraten.
//
// Geschrieben wird ueber RTSDateiMessung, nicht ueber UE_LOG: im Shipping ist UE_LOG
// wegkompiliert, und genau dort spielt der Nutzer.
//
// Die Zeile ist bewusst CSV: die Bilanz laesst sich damit je Quelle aufsummieren, und der
// Unterschied zwischen "Quelle X nimmt zu wenig" und "Quelle Y gibt zu viel" faellt sofort auf.
namespace RTSSupplyMessung
{
	/**
	 * Name der Stelle, die gerade bucht.
	 *
	 * ModifyResource kennt seinen Aufrufer nicht, und die Signatur zu erweitern geht nicht: es ist
	 * eine UFUNCTION, die aus den Blueprints heraus aufgerufen wird. Deshalb setzt jede bekannte
	 * C++-Stelle den Namen vorher ueber FQuelle; was ohne Setzer bucht, kommt aus dem Blueprint.
	 */
	inline FString& AktuelleQuelle()
	{
		static FString Quelle(TEXT("Blueprint"));
		return Quelle;
	}

	/** Setzt die Quelle fuer die Dauer eines Blocks und stellt danach die vorherige wieder her. */
	struct FQuelle
	{
		explicit FQuelle(const TCHAR* Neu)
		{
			Vorherige = AktuelleQuelle();
			AktuelleQuelle() = Neu;
		}
		~FQuelle()
		{
			AktuelleQuelle() = Vorherige;
		}

	private:
		FString Vorherige;
	};

	/**
	 * Zusatztext, den die gerade buchende Stelle an ihre Zeile haengt.
	 *
	 * Gleiche Bauart wie FQuelle und aus demselben Grund: die Buchung landet erst mehrere
	 * Aufrufebenen tiefer in ModifyResourceCCost, und dessen Signatur ist eine UFUNCTION.
	 */
	inline FString& AktuellesDetail()
	{
		static FString Detail;
		return Detail;
	}

	struct FDetail
	{
		explicit FDetail(const TCHAR* Neu)
		{
			Vorheriges = AktuellesDetail();
			AktuellesDetail() = Neu;
		}
		~FDetail()
		{
			AktuellesDetail() = Vorheriges;
		}

	private:
		FString Vorheriges;
	};

	inline const TCHAR* TypName(int32 ResourceTypeAlsZahl)
	{
		switch (ResourceTypeAlsZahl)
		{
		case 0:  return TEXT("Primary");
		case 1:  return TEXT("Secondary");
		case 2:  return TEXT("Tertiary");
		case 3:  return TEXT("Rare");
		case 4:  return TEXT("Epic");
		case 5:  return TEXT("Legendary");
		default: return TEXT("?");
		}
	}

	/**
	 * Eine Buchung festhalten.
	 *
	 * Vorgang ist die Art der Buchung (Kosten, Rueckgabe, Deckel), Betrag das, was die Quelle
	 * uebergeben hat - Vorher/Nachher zeigen, was tatsaechlich angekommen ist. Weichen die beiden
	 * voneinander ab, hat der Boden bei 0 zugeschlagen, und das ist der eigentliche Messwert.
	 */
	inline void Buche(const UWorld* Welt, const TCHAR* Vorgang, int32 TeamId, int32 TypAlsZahl,
	                  float Betrag, float VerbrauchVorher, float VerbrauchNachher, float Maximum,
	                  const FString& Detail = FString())
	{
		const float Zeit = Welt ? Welt->GetTimeSeconds() : -1.f;

		// Detail der Buchungsstelle und Detail des Aufrufers zusammenfuehren - beide sind
		// meistens leer, und wenn nicht, gehoeren sie in dieselbe Spalte.
		FString GesamtDetail = Detail;
		if (!AktuellesDetail().IsEmpty())
		{
			GesamtDetail = GesamtDetail.IsEmpty()
				? AktuellesDetail()
				: GesamtDetail + TEXT(" ") + AktuellesDetail();
		}

		RTSDateiMessung::Schreibe(TEXT("SupplyBilanz.csv"),
			FString::Printf(TEXT("%.2f;%s;%s;%d;%s;%.0f;%.0f;%.0f;%.0f;%s"),
				Zeit, *AktuelleQuelle(), Vorgang, TeamId, TypName(TypAlsZahl),
				Betrag, VerbrauchVorher, VerbrauchNachher, Maximum, *GesamtDetail));
	}

	/** Kopfzeile einmal je Partie, damit die Spalten ohne Nachschlagen lesbar sind. */
	inline void SchreibeKopfzeile()
	{
		RTSDateiMessung::Schreibe(TEXT("SupplyBilanz.csv"),
			TEXT("Zeit;Quelle;Vorgang;Team;Typ;Betrag;VerbrauchVorher;VerbrauchNachher;Maximum;Detail"));
	}
}
