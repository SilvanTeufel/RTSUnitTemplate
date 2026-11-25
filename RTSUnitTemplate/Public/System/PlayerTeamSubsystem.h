// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerTeamSubsystem.generated.h"

class APlayerController;

UCLASS()
class RTSUNITTEMPLATE_API UPlayerTeamSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Sets the team for a player using their session PlayerId
	UFUNCTION(BlueprintCallable, Category="Teams")
	void SetTeamForPlayer(APlayerController* PlayerController, int32 TeamId);

	// Gets the team for a player using their session PlayerId
	UFUNCTION(BlueprintCallable, Category="Teams")
	bool GetTeamForPlayer(APlayerController* PlayerController, int32& OutTeamId, bool bConsume = true);

	UFUNCTION(BlueprintCallable, Category="Teams")
	void ClearAll();

	/**
	 * Lobby: slots played by the AI - one entry per AI player, the value is its team (twice the same team = two AI
	 * players in it). The host sets it before the travel; the game mode spawns an AI player for each at a start of
	 * that team (ARTSGameModeBase::SetTeamIdsAndWaypoints).
	 */
	UFUNCTION(BlueprintCallable, Category="Teams")
	void SetAISlots(const TArray<int32>& TeamIds);

	UFUNCTION(BlueprintPure, Category="Teams")
	TArray<int32> GetAISlots() const { return AISlots; }

	/** Adds (or with bAI = false removes) one AI player of a team. */
	UFUNCTION(BlueprintCallable, Category="Teams")
	void SetAISlot(int32 TeamId, bool bAI);

	/** The AI slots, emptied (the game mode takes them once). */
	TArray<int32> ConsumeAISlots();

	UFUNCTION(BlueprintCallable, Category="Teams")
	void SetTeamsAllied(int32 TeamA, int32 TeamB, bool bAllied);

	UFUNCTION(BlueprintPure, Category="Teams")
	bool IsAllied(int32 TeamA, int32 TeamB) const;

	UFUNCTION(BlueprintPure, Category="Teams")
	int64 GetAlliedTeamsMask(int32 TeamId) const;

private:
	// PlayerId -> TeamId (kann über Travel neu vergeben werden)
	UPROPERTY()
	TMap<int32, int32> PendingTeams;

	// Spielername -> TeamId (robuster bei non-seamless Travel, sofern Namen stabil/unique sind)
	UPROPERTY()
	TMap<FString, int32> PendingTeamsByName;

	// Netzwerkadresse (z.B. "IP:Port") -> TeamId als letzter Fallback
	UPROPERTY()
	TMap<FString, int32> PendingTeamsByNetworkAddress;

	// TeamId -> Set of allied TeamIds
	TMap<int32, TArray<int32>> AlliedTeamsMap;

	UPROPERTY()
	TArray<int32> AISlots;

	void UpdateAllUnitsAlliedMask(int32 TeamId);
};