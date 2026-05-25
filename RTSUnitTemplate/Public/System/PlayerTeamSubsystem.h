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

	void UpdateAllUnitsAlliedMask(int32 TeamId);
};