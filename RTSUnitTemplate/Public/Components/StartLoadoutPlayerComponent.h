// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/StartLoadoutTypes.h"
#include "StartLoadoutPlayerComponent.generated.h"

class UStartLoadoutWidget;
class UStartLoadoutDifficultyWidget;

/**
 * The start loadout's line to one player (see UStartLoadoutComponent): the server adds it to the player's controller
 * at match start. It opens the selection widget on the player's machine and sends his choice back.
 */
UCLASS(ClassGroup = (RTSUnitTemplate))
class RTSUNITTEMPLATE_API UStartLoadoutPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStartLoadoutPlayerComponent();

	/** Server: the component of a controller (added and replicated on the first call). */
	static UStartLoadoutPlayerComponent* FindOrAdd(APlayerController* PC);

	/** Opens the selection: the sets the player can choose from and his fixed main base. */
	UFUNCTION(Client, Reliable)
	void Client_OfferLoadout(TSubclassOf<UStartLoadoutWidget> InWidgetClass, const FStartLoadoutFaction& Faction,
		const TArray<FName>& RowNames, const TArray<FStartLoadoutSetRow>& Sets, float Seconds, const FText& Header, const FText& Hint);

	/** The selection is over (ChosenRow is what the server applied). Closes the widget. */
	UFUNCTION(Client, Reliable)
	void Client_EndSelection(FName ChosenRow);

	UFUNCTION(Server, Reliable)
	void Server_ChooseLoadout(FName RowName);

	/** The host chooses the difficulty before the cards. */
	UFUNCTION(Client, Reliable)
	void Client_OfferDifficulty(const TArray<FStartLoadoutDifficulty>& Levels, int32 DefaultIndex, const FText& Header, UMaterialInterface* CardMaterial);

	UFUNCTION(Client, Reliable)
	void Client_EndDifficulty();

	UFUNCTION(Server, Reliable)
	void Server_ChooseDifficulty(int32 Index);

	/** Local player: sends the difficulty to the server (the widget calls it). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void ChooseDifficulty(int32 Index);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UStartLoadoutDifficultyWidget> DifficultyWidget = nullptr;

	/** Local player: sends the choice to the server (the widget calls it). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void ChooseLoadout(FName RowName);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UStartLoadoutWidget> Widget = nullptr;

	/**
	 * Off = no keyboard layout choice before the first start screen. On (default): while no layout
	 * was saved yet, Modern / Classic / User defined comes before the difficulty (or the cards).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	bool bAskKeyLayoutFirst = true;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Opens the layout choice if this player never picked one. True = Then runs after it closed. */
	bool OpenKeyLayoutChoiceFirst(UMaterialInterface* CardMaterial, TFunction<void()> Then);

	void ShowDifficultyWidget(const TArray<FStartLoadoutDifficulty>& Levels, int32 DefaultIndex, const FText& Header, UMaterialInterface* CardMaterial);

	/** A difficulty offer waits for the layout choice; Client_EndDifficulty in between cancels it. */
	bool bDifficultyPending = false;
	bool bKeyLayoutAsked = false;
};
