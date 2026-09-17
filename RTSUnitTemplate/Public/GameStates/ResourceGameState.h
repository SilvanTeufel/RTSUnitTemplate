// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/WorkerData.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "ResourceGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillCountChanged, int32, NewCount);

/**
 *
 */
UCLASS()
class RTSUNITTEMPLATE_API AResourceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// Use replicated properties to share data with clients
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	TArray<FResourceArray> TeamResources;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	TArray<bool> IsSupplyLike;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Function to handle changes to the replicated TeamResources
	UFUNCTION()
	void OnRep_TeamResources();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetTeamResources(TArray<FResourceArray> Resources);

	UPROPERTY(ReplicatedUsing = OnRep_LoadingWidgetConfig, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	FLoadingWidgetConfig LoadingWidgetConfig;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	float MatchStartTime = -1.f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	bool bStartupFreezeReleased = false;

	UFUNCTION()
	void OnRep_LoadingWidgetConfig();

	/**
	 * Gefallene Einheiten je Team - Index ist die TeamId.
	 *
	 * Bewusst "Verluste je Team" und nicht "Abschuesse je Team": der Todespfad
	 * (AUnitBase::DeadEffectsAndEvents -> ARTSGameModeBase::CheckWinLoseCondition) kennt nur
	 * die sterbende Einheit, keinen Verursacher. Eine Zuordnung zum Toeter gaebe es nur mit
	 * einer neuen Schadensquelle quer durch Projektile, Faehigkeiten und Effektflaechen.
	 *
	 * Fuer die Anzeige reicht das: GetKillsForTeam() summiert die Verluste aller ANDEREN
	 * Teams. In einem Survival mit einem Spielerteam gegen die Xeno ist das genau die Zahl,
	 * die der Spieler als "erledigte Gegner" erwartet.
	 *
	 * Liegt im GameState und nicht im GameMode, weil der GameMode auf dem Client gar nicht
	 * existiert - die Anzeige wuerde dort sonst leer bleiben.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_TeamUnitsLost, VisibleAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate")
	TArray<int32> TeamUnitsLost;

	UFUNCTION()
	void OnRep_TeamUnitsLost();

	/** Feuert auf dem Server UND auf jedem Client, sobald sich die Zahl aendert. */
	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate")
	FOnKillCountChanged OnKillCountChanged;

	/** Nur auf dem Server aufrufen - der Rest laeuft ueber die Replikation. */
	void AddUnitLoss(int32 TeamId);

	/** Erledigte Gegner aus Sicht von MyTeamId: die Verluste aller anderen Teams. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate")
	int32 GetKillsForTeam(int32 MyTeamId) const;

	/** Verluste des eigenen Teams - fuer eine "gefallen/erledigt"-Anzeige. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate")
	int32 GetLossesForTeam(int32 MyTeamId) const;

private:
	/** Merkt sich je Client die zuletzt gemeldete Zahl, damit OnRep nicht doppelt feuert. */
	int32 ZuletztGemeldeteAbschuesse = -1;
};