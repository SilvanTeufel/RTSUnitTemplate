// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"

// MESSUNG, DIE IM SHIPPING UEBERLEBT (24.09.2026).
//
// UE_LOG ist im Shipping wegkompiliert - siehe AstraHelix.Target.cs: bUseLoggingInShipping laesst
// sich mit der installierten Binaerengine nicht setzen, weil sie ein GETEILTES Build-Environment
// erzwingt. Ueber das Log ist im Shipping also nichts messbar.
//
// FFileHelper wird nicht wegkompiliert. Diese Hilfe haengt eine Zeile an eine Datei unter
// Saved/ an; damit lassen sich Shipping-eigene Ursachen belegen, statt sie zu erraten.
//
// Bewusst ungepuffert und ohne Zwischenspeicher: die gemessenen Vorgaenge sind selten (einmal je
// Bindung, einmal je Baufortschrittsschritt). Wer damit etwas Bildweises misst, bremst das Spiel.
namespace RTSDateiMessung
{
	/**
	 * Schalter fuer ALLE CSV-Messungen. Vorgabe: AUS.
	 *
	 * Die Messungen haben ihren Zweck erfuellt (Versorgungsbilanz und Faehigkeits-Diagnose sind
	 * belegt), deshalb schreibt das Spiel keine Dateien mehr. Die Aufrufstellen bleiben
	 * unveraendert stehen - sie sind der eigentliche Wert, nicht die Dateien. Mit
	 * "rts.csv.messung 1" auf der Konsole ist alles sofort wieder da, ohne Neubau.
	 *
	 * BEWUSST ohne zwischengespeicherten Zeiger: wird die Variable erst nach dem ersten Aufruf
	 * registriert, wuerde ein gecachter Nullzeiger die Messung dauerhaft totlegen. Die Suche
	 * kostet nichts gegen einen Dateizugriff, und die Aufrufe sind selten.
	 */
	inline bool MessungAktiv()
	{
		const IConsoleVariable* Schalter =
			IConsoleManager::Get().FindConsoleVariable(TEXT("rts.csv.messung"));
		return Schalter != nullptr && Schalter->GetInt() != 0;
	}

	inline void Schreibe(const TCHAR* Dateiname, const FString& Zeile)
	{
		if (!MessungAktiv())
		{
			return;
		}

		const FString Pfad = FPaths::ProjectSavedDir() / Dateiname;
		FFileHelper::SaveStringToFile(
			Zeile + LINE_TERMINATOR,
			*Pfad,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(),
			EFileWrite::FILEWRITE_Append);
	}
}
