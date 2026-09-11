// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/BossWaveSpawner.h"

#include "Characters/Unit/UnitBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABossWaveSpawner::ABossWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ABossWaveSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossWaveSpawner, WaveIndex);
}

void ABossWaveSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || !bEnabled || BossClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossWelle] inaktiv (Authority=%d Enabled=%d Klassen=%d)"),
			HasAuthority() ? 1 : 0, bEnabled ? 1 : 0, BossClasses.Num());
		return;
	}

	GetWorldTimerManager().SetTimer(WaveTimer, this, &ABossWaveSpawner::SpawnWave,
		FMath::Max(1.f, IntervalSeconds), true, FMath::Max(1.f, FirstWaveDelay));

	UE_LOG(LogTemp, Log, TEXT("[BossWelle] bereit: erster Boss nach %.0f s, danach alle %.0f s, Stufe %d (+%d je Welle)"),
		FirstWaveDelay, IntervalSeconds, StartLevel, LevelPerWave);
}

int32 ABossWaveSpawner::GetNextWaveLevel() const
{
	return FMath::Clamp(StartLevel + WaveIndex * LevelPerWave, 1, FMath::Max(1, MaxLevel));
}

void ABossWaveSpawner::SpawnWaveNow()
{
	SpawnWave();
}

void ABossWaveSpawner::SpawnWave()
{
	if (!HasAuthority() || BossClasses.Num() == 0)
	{
		return;
	}

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossWelle] kein RTSGameModeBase - Welle faellt aus"));
		return;
	}

	const int32 Stufe = GetNextWaveLevel();
	int32 Gesetzt = 0;

	for (int32 i = 0; i < FMath::Max(1, BossesPerWave); ++i)
	{
		TSubclassOf<AUnitBase> Klasse = BossClasses[(WaveIndex + i) % BossClasses.Num()];
		if (!Klasse)
		{
			continue;
		}

		// Ueber den GameMode spawnen, damit die Einheit in AllUnits landet - sonst sehen
		// weder die Siegpruefung noch die Auswahl den Boss.
		FUnitSpawnParameter Parameter;
		Parameter.UnitBaseClass = Klasse;
		Parameter.UnitCount = 1;
		Parameter.State = UnitData::PatrolRandom;
		Parameter.StatePlaceholder = UnitData::PatrolRandom;
		Parameter.WaypointTag = WaypointTag;
		Parameter.TeamId = TeamId;
		Parameter.CanBeSelected = false;

		const FVector Streuung(FMath::FRandRange(-SpawnRadius, SpawnRadius),
		                       FMath::FRandRange(-SpawnRadius, SpawnRadius), 0.f);
		AUnitBase* Boss = GameMode->SpawnSingleUnit(Parameter, GetActorLocation() + Streuung,
			nullptr, TeamId, nullptr);

		if (Boss)
		{
			HebeAufStufe(Boss, Stufe);
			++Gesetzt;
		}
	}

	++WaveIndex;
	UE_LOG(LogTemp, Log, TEXT("[BossWelle] Welle %d: %d Boss(e) auf Stufe %d gesetzt (naechste Stufe %d)"),
		WaveIndex, Gesetzt, Stufe, GetNextWaveLevel());
}

void ABossWaveSpawner::HebeAufStufe(AUnitBase* Einheit, int32 Zielstufe) const
{
	if (!Einheit)
	{
		return;
	}

	// LevelUp() steigt nur, wenn genug Erfahrung da ist, und zieht sie danach wieder ab.
	// Deshalb vor jedem Schritt auffuellen; AutoLevelUp investiert die Talentpunkte gleich mit,
	// sonst waere der Boss zwar hochstufig, aber nicht staerker.
	int32 Schritte = 0;
	while (Einheit->LevelData.CharacterLevel < Zielstufe && Schritte < 500)
	{
		Einheit->LevelData.Experience =
			Einheit->LevelUpData.ExperiencePerLevel * (Einheit->LevelData.CharacterLevel + 1) + 1;
		const int32 Vorher = Einheit->LevelData.CharacterLevel;
		Einheit->AutoLevelUp();
		if (Einheit->LevelData.CharacterLevel == Vorher)
		{
			// MaxCharacterLevel erreicht - weitere Versuche wuerden nur laufen.
			break;
		}
		++Schritte;
	}

	UE_LOG(LogTemp, Log, TEXT("[BossWelle] %s auf Stufe %d (Ziel %d)"),
		*Einheit->GetName(), Einheit->LevelData.CharacterLevel, Zielstufe);
}
