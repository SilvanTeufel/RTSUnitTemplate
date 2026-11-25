// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/StartLoadoutTypes.h"
#include "StartLoadoutWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UStartLoadoutPlayerComponent;
class UStartLoadoutSetButtonProxy;

/**
 * Selection screen of the start loadout: the fixed main base and the card sets of the player's faction (four cards
 * each: workers, army, tech and production building). Click a set, then confirm. Builds its own layout; a Blueprint
 * child can bring its own instead (a widget tree with a root) and use Setup's data through OnLoadoutOffered.
 */
UCLASS()
class RTSUNITTEMPLATE_API UStartLoadoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** InHint: text above the cards; empty = HintText (start) / ReinforcementsHintText. */
	void Setup(UStartLoadoutPlayerComponent* InOwner, const FStartLoadoutFaction& InFaction, const TArray<FName>& InRowNames,
		const TArray<FStartLoadoutSetRow>& InSets, float Seconds, const FText& InHint = FText::GetEmpty());

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void SelectSet(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void ConfirmSelection();

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate|StartLoadout")
	void OnLoadoutOffered(const FStartLoadoutFaction& InFaction, const TArray<FName>& InRowNames, const TArray<FStartLoadoutSetRow>& InSets, float Seconds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText HeaderText = NSLOCTEXT("StartLoadout", "Header", "Choose your start");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText HintText = NSLOCTEXT("StartLoadout", "Hint", "Before the match begins, decide how your base starts. Your main base is always there - pick one of the card sets below: workers, army, a tech and a production building, each with a start bonus for your units. More cards follow later in the match.");

	/** Hint of the later selections (no main base card): reinforcements, no buildings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText ReinforcementsHintText = NSLOCTEXT("StartLoadout", "ReinforcementsHint", "Reinforcements: pick one card set. Its units join you at your base and its bonus goes to your whole army.");

	/** True while a selection screen is on the screen of this world (the tutorial waits for it). */
	static bool IsAnyOpen(const UWorld* World);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText ConfirmText = NSLOCTEXT("StartLoadout", "Confirm", "Confirm");

	/** Font of headings and buttons (size 20 and up). Empty or missing = the engine font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath HeadingFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-SemiBold_Font.ChakraPetch-SemiBold_Font"));

	/** Font of the smaller texts. Empty or missing = the engine font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath BodyFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-Medium_Font.ChakraPetch-Medium_Font"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText WaitingText = NSLOCTEXT("StartLoadout", "Waiting", "Waiting for the other players ...");

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	bool bConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TArray<FName> RowNames;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TArray<FStartLoadoutSetRow> Sets;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutFaction Faction;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildLayout();
	void BuildSets();
	void RefreshSelection();
	UWidget* MakeCard(const FStartLoadoutCard& Card, EStartLoadoutCardType Type, UMaterialInterface* Material, bool bLocked);
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bWrap = false);
	static FText CardTitle(const FStartLoadoutCard& Card);

	UFUNCTION()
	void HandleConfirmClicked();

	UPROPERTY(Transient)
	TObjectPtr<UStartLoadoutPlayerComponent> OwnerComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SetRow = nullptr;

	/** The column of the whole screen, the countdown + confirm bar and the place right of the sets it moves to. */
	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> LayoutColumn = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ConfirmBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> SideColumn = nullptr;

	/** The confirm bar stands right of the sets (the screen is too low for it below them). */
	bool bConfirmAtSide = false;

	void UpdateConfirmPlacement(const FGeometry& MyGeometry);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderBlock = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintBlock = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> SetFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStartLoadoutSetButtonProxy>> Proxies;

	double EndTime = 0.0;
	bool bBuiltLayout = false;
};

/**
 * Difficulty screen before the start selection: one big button per level (name and what it does). A click chooses.
 */
UCLASS()
class RTSUNITTEMPLATE_API UStartLoadoutDifficultyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** CardMaterial: the chooser's faction card material (the cards' face), may be null. */
	void Setup(UStartLoadoutPlayerComponent* InOwner, const TArray<FStartLoadoutDifficulty>& InLevels, int32 InDefaultIndex, const FText& InHeader,
		UMaterialInterface* CardMaterial = nullptr);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|StartLoadout")
	void ChooseLevel(int32 Index);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText HintText = NSLOCTEXT("StartLoadout", "DifficultyHint", "How strong should the enemy be? This only changes the enemies' health and damage - your own units and everything else stay the same.");

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|StartLoadout")
	TArray<FStartLoadoutDifficulty> Levels;

	/** Font of headings and buttons (size 20 and up). Empty or missing = the engine font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath HeadingFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-SemiBold_Font.ChakraPetch-SemiBold_Font"));

	/** Font of the smaller texts. Empty or missing = the engine font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath BodyFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-Medium_Font.ChakraPetch-Medium_Font"));

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bWrap = false);

	UPROPERTY(Transient)
	TObjectPtr<UStartLoadoutPlayerComponent> OwnerComponent = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Proxies;

	int32 DefaultIndex = INDEX_NONE;
	bool bChosen = false;
};

/** Click handler of one difficulty button. */
UCLASS()
class RTSUNITTEMPLATE_API UStartLoadoutDifficultyButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UStartLoadoutDifficultyWidget> Widget = nullptr;

	int32 Index = INDEX_NONE;

	UFUNCTION()
	void HandleClicked() { if (Widget) Widget->ChooseLevel(Index); }
};

/** Click handler of one set button (UButton's OnClicked has no parameter). */
UCLASS()
class RTSUNITTEMPLATE_API UStartLoadoutSetButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UStartLoadoutWidget> Widget = nullptr;

	int32 Index = INDEX_NONE;

	UFUNCTION()
	void HandleClicked() { if (Widget) Widget->SelectSet(Index); }
};
