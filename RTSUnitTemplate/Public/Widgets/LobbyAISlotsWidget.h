// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyAISlotsWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class ULobbyAISlotButtonProxy;

/**
 * Lobby: how many AI players each team gets (host only - the setting lives in the host's UPlayerTeamSubsystem and
 * the game mode spawns the AI players at the match start, PlayerController bIsAi). Builds its own layout: one line per
 * team with - / + buttons. Teams = the map's teams; the lobby can pass them with SetTeams.
 */
UCLASS()
class RTSUNITTEMPLATE_API ULobbyAISlotsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The teams that can get AI players (e.g. the map's allowed team ids). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lobby")
	TArray<int32> Teams = { 1, 2, 3, 4 };

	/** Most AI players per team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lobby", meta = (ClampMin = "0"))
	int32 MaxPerTeam = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lobby")
	FText TitleText = NSLOCTEXT("LobbyAISlots", "Title", "AI players");

	/** Shows these teams (AI players of other teams are dropped). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Lobby")
	void SetTeams(const TArray<int32>& InTeams);

	/** Adds (Delta 1) or removes (-1) an AI player of a team. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Lobby")
	void ChangeAICount(int32 TeamId, int32 Delta);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Lobby")
	int32 GetAICount(int32 TeamId) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void Rebuild();
	class UPlayerTeamSubsystem* GetTeamSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Lines = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULobbyAISlotButtonProxy>> Proxies;
};

/** Click handler of one - / + button. */
UCLASS()
class RTSUNITTEMPLATE_API ULobbyAISlotButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<ULobbyAISlotsWidget> Widget = nullptr;

	int32 TeamId = 0;
	int32 Delta = 0;

	UFUNCTION()
	void HandleClicked() { if (Widget) Widget->ChangeAICount(TeamId, Delta); }
};
