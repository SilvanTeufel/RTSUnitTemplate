// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/SurvivalScoreSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	/** Die Liste liegt als JSON im Speicherobjekt - so bleibt das Format erweiterbar. */
	TArray<FSurvivalScoreEntry> AusJson(const FString& Json)
	{
		TArray<FSurvivalScoreEntry> Ergebnis;
		TArray<TSharedPtr<FJsonValue>> Werte;
		const TSharedRef<TJsonReader<>> Leser = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Leser, Werte))
		{
			return Ergebnis;
		}
		for (const TSharedPtr<FJsonValue>& Wert : Werte)
		{
			const TSharedPtr<FJsonObject>* Objekt = nullptr;
			if (!Wert.IsValid() || !Wert->TryGetObject(Objekt))
			{
				continue;
			}
			FSurvivalScoreEntry Eintrag;
			Eintrag.PlayerName = (*Objekt)->GetStringField(TEXT("name"));
			Eintrag.Seconds = (*Objekt)->GetNumberField(TEXT("seconds"));
			Eintrag.Waves = (*Objekt)->GetIntegerField(TEXT("waves"));
			Eintrag.UnixTime = static_cast<int64>((*Objekt)->GetNumberField(TEXT("time")));
			Ergebnis.Add(Eintrag);
		}
		return Ergebnis;
	}

	FString NachJson(const TArray<FSurvivalScoreEntry>& Liste)
	{
		TArray<TSharedPtr<FJsonValue>> Werte;
		for (const FSurvivalScoreEntry& E : Liste)
		{
			TSharedRef<FJsonObject> Objekt = MakeShared<FJsonObject>();
			Objekt->SetStringField(TEXT("name"), E.PlayerName);
			Objekt->SetNumberField(TEXT("seconds"), E.Seconds);
			Objekt->SetNumberField(TEXT("waves"), E.Waves);
			Objekt->SetNumberField(TEXT("time"), static_cast<double>(E.UnixTime));
			Werte.Add(MakeShared<FJsonValueObject>(Objekt));
		}
		FString Aus;
		const TSharedRef<TJsonWriter<>> Schreiber = TJsonWriterFactory<>::Create(&Aus);
		FJsonSerializer::Serialize(Werte, Schreiber);
		return Aus;
	}
}

TArray<FSurvivalScoreEntry> USurvivalScoreSubsystem::LadeListe(const FString& MapName) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName(), 0))
	{
		return TArray<FSurvivalScoreEntry>();
	}
	USurvivalScoreSave* Save = Cast<USurvivalScoreSave>(
		UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
	if (!Save)
	{
		return TArray<FSurvivalScoreEntry>();
	}
	const FString* Json = Save->MapToJson.Find(MapName);
	return Json ? AusJson(*Json) : TArray<FSurvivalScoreEntry>();
}

void USurvivalScoreSubsystem::SpeichereListe(const FString& MapName,
                                             const TArray<FSurvivalScoreEntry>& Liste) const
{
	USurvivalScoreSave* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName(), 0))
	{
		Save = Cast<USurvivalScoreSave>(UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
	}
	if (!Save)
	{
		Save = Cast<USurvivalScoreSave>(
			UGameplayStatics::CreateSaveGameObject(USurvivalScoreSave::StaticClass()));
	}
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Survival-Rangliste] Speicherobjekt liess sich nicht anlegen"));
		return;
	}
	Save->MapToJson.Add(MapName, NachJson(Liste));
	UGameplayStatics::SaveGameToSlot(Save, SlotName(), 0);
}

bool USurvivalScoreSubsystem::SubmitScore(const FString& MapName, const FString& PlayerName,
                                          float Seconds, int32 Waves)
{
	TArray<FSurvivalScoreEntry> Liste = LadeListe(MapName);
	const float Bisher = Liste.Num() > 0 ? Liste[0].Seconds : 0.f;

	FSurvivalScoreEntry Neu;
	Neu.PlayerName = PlayerName.IsEmpty() ? TEXT("Spieler") : PlayerName;
	Neu.Seconds = Seconds;
	Neu.Waves = Waves;
	Neu.UnixTime = FDateTime::UtcNow().ToUnixTimestamp();
	Liste.Add(Neu);

	Liste.Sort([](const FSurvivalScoreEntry& A, const FSurvivalScoreEntry& B)
	{
		return A.Seconds > B.Seconds;
	});
	if (Liste.Num() > FMath::Max(1, MaxStoredPerMap))
	{
		Liste.SetNum(FMath::Max(1, MaxStoredPerMap));
	}
	SpeichereListe(MapName, Liste);

	const bool bNeueBestzeit = Seconds > Bisher;
	UE_LOG(LogTemp, Log, TEXT("[Survival-Rangliste] %s: %s nach %s (Welle %d)%s"),
		*MapName, *Neu.PlayerName, *FormatSeconds(Seconds), Waves,
		bNeueBestzeit ? TEXT(" - neue Bestzeit") : TEXT(""));
	return bNeueBestzeit;
}

TArray<FSurvivalScoreEntry> USurvivalScoreSubsystem::GetScores(const FString& MapName,
                                                               int32 MaxEntries) const
{
	TArray<FSurvivalScoreEntry> Liste = LadeListe(MapName);
	if (MaxEntries > 0 && Liste.Num() > MaxEntries)
	{
		Liste.SetNum(MaxEntries);
	}
	return Liste;
}

float USurvivalScoreSubsystem::GetBestSeconds(const FString& MapName) const
{
	const TArray<FSurvivalScoreEntry> Liste = LadeListe(MapName);
	return Liste.Num() > 0 ? Liste[0].Seconds : 0.f;
}

FString USurvivalScoreSubsystem::FormatSeconds(float Seconds)
{
	const int32 Gesamt = FMath::Max(0, FMath::FloorToInt(Seconds));
	const int32 Stunden = Gesamt / 3600;
	const int32 Minuten = (Gesamt % 3600) / 60;
	const int32 Rest = Gesamt % 60;
	if (Stunden > 0)
	{
		return FString::Printf(TEXT("%dh %02dm %02ds"), Stunden, Minuten, Rest);
	}
	return FString::Printf(TEXT("%dm %02ds"), Minuten, Rest);
}

FString USurvivalScoreSubsystem::GetScoreboardText(const UObject* WorldContextObject, int32 MaxEntries)
{
	UWorld* Welt = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!Welt || !Welt->GetGameInstance())
	{
		return FString();
	}
	USurvivalScoreSubsystem* Rangliste =
		Welt->GetGameInstance()->GetSubsystem<USurvivalScoreSubsystem>();
	if (!Rangliste)
	{
		return FString();
	}

	FString Karte = Welt->GetMapName();
	Karte.RemoveFromStart(Welt->StreamingLevelsPrefix);

	const TArray<FSurvivalScoreEntry> Liste = Rangliste->GetScores(Karte, MaxEntries);
	if (Liste.Num() == 0)
	{
		return FString();
	}

	FString Text = TEXT("Best times\n");
	for (int32 i = 0; i < Liste.Num(); ++i)
	{
		Text += FString::Printf(TEXT("%d. %s - %s (wave %d)\n"), i + 1, *Liste[i].PlayerName,
			*FormatSeconds(Liste[i].Seconds), Liste[i].Waves);
	}
	return Text;
}
