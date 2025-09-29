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
};