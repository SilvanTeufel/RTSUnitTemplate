// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossWaveSpawner.generated.h"

class AUnitBase;

/**
 * Setzt in festen Abstaenden einen Boss in die Welt und macht ihn mit jeder Welle
 * staerker. Gedacht fuer Endlos-Survival: das Spiel endet nicht an einer Wellenzahl,
 * sondern daran, dass die Bosse irgendwann zu stark werden.
 *
 * Der Spawner laeuft nur auf dem Server; die Einheiten replizieren sich von selbst.
 */
UCLASS()
class RTSUNITTEMPLATE_API ABossWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABossWaveSpawner();

	/** Bossklassen; die Wellen gehen der Reihe nach durch die Liste und fangen dann von vorn an. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	TArray<TSubclassOf<AUnitBase>> BossClasses;

	/** Abstand zwischen zwei Bossen in Sekunden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	float IntervalSeconds = 300.f;

	/** Wartezeit bis zum ersten Boss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	float FirstWaveDelay = 300.f;

	/** Stufe des ersten Bosses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 StartLevel = 5;

	/** Um so viele Stufen waechst jeder weitere Boss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 LevelPerWave = 3;

	/** Obergrenze, damit die Werte nicht ins Unendliche laufen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 MaxLevel = 200;

	/** Team der Bosse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 TeamId = 2;

	/** Wie viele Bosse pro Welle. Waechst nicht - die Staerke kommt ueber die Stufe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 BossesPerWave = 1;

	/**
	 * Begleiteinheiten, die mit jeder Bosswelle kommen.
	 *
	 * Leer gelassen aendert sich nichts - der Spawner setzt dann nur Bosse wie bisher. Gedacht
	 * gegen das gemeldete Abflauen: nach dem ersten Boss kamen kaum noch Gegner, weil die
	 * Wellentabellen des GameMode durchgelaufen waren und nur noch der Boss nachkam.
	 *
	 * Die Begleiter bekommen dieselbe Stufe wie der Boss der Welle, werden also mit ihr staerker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	TArray<TSubclassOf<AUnitBase>> EscortClasses;

	/** Begleiter in der ERSTEN Bosswelle. 0 = keine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 EscortsPerWave = 0;

	/** Wieviele Begleiter je weiterer Welle dazukommen. 0 = die Zahl bleibt gleich. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 EscortsAddedPerWave = 0;

	/** Obergrenze, damit eine lange Partie nicht in der Einheitenzahl erstickt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	int32 MaxEscortsPerWave = 40;

	/** Streuung um den Spawner herum (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	float SpawnRadius = 800.f;

	/** Wegpunkt-Kennzeichen, das die Bosse bekommen (leer = keins). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	FString WaypointTag;

	/** Abschalten, ohne den Aktor zu entfernen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|BossWave")
	bool bEnabled = true;

	/** Bisher gestartete Wellen. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "RTSUnitTemplate|BossWave")
	int32 WaveIndex = 0;

	/** Stufe der naechsten Welle - fuer Anzeigen im HUD. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|BossWave")
	int32 GetNextWaveLevel() const;

	/** Startet die naechste Welle sofort, unabhaengig vom Zeitgeber. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RTSUnitTemplate|BossWave")
	void SpawnWaveNow();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle WaveTimer;

	void SpawnWave();
	void HebeAufStufe(AUnitBase* Einheit, int32 Zielstufe) const;
};
