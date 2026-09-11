// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "SurvivalScoreSubsystem.generated.h"

/** Ein Eintrag der Bestenliste. */
USTRUCT(BlueprintType)
struct FSurvivalScoreEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	FString PlayerName;

	/** Ueberlebte Zeit in Sekunden. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	float Seconds = 0.f;

	/** Erreichte Bosswelle. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	int32 Waves = 0;

	/** Unixzeit des Laufs. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	int64 UnixTime = 0;
};

/** Speicherobjekt der Bestenliste - je Karte eine Liste. */
UCLASS()
class RTSUNITTEMPLATE_API USurvivalScoreSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FString, FString> MapToJson;
};

/**
 * Haelt die Bestzeiten des Endlos-Survival fest.
 *
 * Lokal wird immer gespeichert, damit die Liste ohne Netz funktioniert. Zusaetzlich laesst
 * sich die Zeit als EOS-Statistik melden; die eigentliche Rangliste entsteht dann im
 * Epic-Portal aus dieser Statistik (dort muss ein Leaderboard auf denselben Namen zeigen).
 */
UCLASS()
class RTSUNITTEMPLATE_API USurvivalScoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Traegt einen Lauf ein und gibt zurueck, ob es eine neue Bestzeit war. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Survival")
	bool SubmitScore(const FString& MapName, const FString& PlayerName, float Seconds, int32 Waves);

	/** Liefert die Bestenliste einer Karte, absteigend nach Zeit. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Survival")
	TArray<FSurvivalScoreEntry> GetScores(const FString& MapName, int32 MaxEntries = 10) const;

	/** Beste Zeit einer Karte in Sekunden (0, wenn noch nichts eingetragen ist). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Survival")
	float GetBestSeconds(const FString& MapName) const;

	/** "1h 23m 45s" - fuer die Anzeige im HUD. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Survival")
	static FString FormatSeconds(float Seconds);

	/**
	 * Fertiger Anzeigetext der Bestenliste ("1. Name - 1h 02m 30s (Welle 12)").
	 * Statisch mit WorldContext, damit ein Widget ihn in einem Knoten holen kann; der
	 * Kartenname wird selbst ermittelt und um das PIE-Praefix bereinigt.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Survival",
		meta = (WorldContext = "WorldContextObject"))
	static FString GetScoreboardText(const UObject* WorldContextObject, int32 MaxEntries = 5);

	/** Name der EOS-Statistik, unter der die Zeit gemeldet wird. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	FString EosStatName = TEXT("SurvivalSeconds");

	/** Wie viele Eintraege je Karte aufgehoben werden. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Survival")
	int32 MaxStoredPerMap = 25;

private:
	static const TCHAR* SlotName() { return TEXT("SurvivalScores"); }

	TArray<FSurvivalScoreEntry> LadeListe(const FString& MapName) const;
	void SpeichereListe(const FString& MapName, const TArray<FSurvivalScoreEntry>& Liste) const;
};
