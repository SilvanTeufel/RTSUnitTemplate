// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "MainMenuWidget.generated.h"

class SBox;
class USaveGameWidget;
class USoundBase;

/** What a main menu button does. */
UENUM(BlueprintType)
enum class EMainMenuAction : uint8
{
	/** Travels to TargetMap (loading screen first). */
	OpenMap,
	/** Puts WidgetClass on the screen above the menu - e.g. the replay browser. */
	ShowWidget,
	/** Opens SaveGameWidgetClass of the menu (save / load). */
	SaveLoad,
	/** Opens URL in the system browser - e.g. the store page for the wishlist. */
	OpenURL,
	/** Closes the game. */
	Quit
};

/** One button of the main menu. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FMainMenuEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FText Label;

	/** Small line under the label. Empty = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	EMainMenuAction Action = EMainMenuAction::OpenMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu", meta = (EditCondition = "Action == EMainMenuAction::OpenMap", EditConditionHides))
	TSoftObjectPtr<UWorld> TargetMap;

	/** Like AMapSwitchActor: this tag is switched on for TargetMap when travelling there. None = nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu", meta = (EditCondition = "Action == EMainMenuAction::OpenMap", EditConditionHides))
	FName DestinationSwitchTagToEnable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu", meta = (EditCondition = "Action == EMainMenuAction::ShowWidget", EditConditionHides))
	TSoftClassPtr<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu", meta = (EditCondition = "Action == EMainMenuAction::OpenURL", EditConditionHides))
	FString URL;

	/**
	 * Gate: the button stays greyed out until one of the two conditions below is met.
	 * Both empty = always open.
	 *
	 * A save slot with this name exists (the prologue writes "PrologueDone" when it is won).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Unlock")
	FString RequiredSaveSlot;

	/** ...or this MapSwitch tag is enabled for RequiredSwitchTagMap (UMapSwitchSubsystem). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Unlock")
	FName RequiredSwitchTag;

	/**
	 * Map the tag is read against - map name without path, e.g. Main_Menu. Tags are stored per map;
	 * the old menu map is where the campaign unlocks the story. Empty = the current map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Unlock")
	FString RequiredSwitchTagMap;

	/** Shown instead of Description while locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Unlock")
	FText LockedHint;

	/** Extra gap above this button - to group the buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	bool bGapBefore = false;
};

/** Look of the main menu. Without any texture it draws plain dark colours. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FMainMenuStyle
{
	GENERATED_BODY()

	FMainMenuStyle();

	/** Fills the whole screen (scaled to fill, cropped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush Background;

	/** Darkens the side the menu sits on, so the text stays readable on any picture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush Shade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	float ShadeWidth = 640.f;

	/** Frame behind title and buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush Panel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FMargin PanelPadding = FMargin(48.f, 40.f);

	/** Distance of the panel from the left screen edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	float PanelOffsetX = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush ButtonNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush ButtonHovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush ButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateBrush ButtonDisabled;

	/**
	 * Minimum size of a button. All buttons share one width: the widest label decides, X only sets
	 * a lower limit (0 = as narrow as the longest text allows).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FVector2D ButtonSize = FVector2D(0.f, 58.f);

	/** Space between the button frame and its text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FMargin ButtonPadding = FMargin(36.f, 8.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	float ButtonSpacing = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateFontInfo SubtitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateFontInfo ButtonFont;

	/** Description / locked hint under the button label, and the footer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FSlateFontInfo SmallFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FLinearColor TitleColor = FLinearColor(0.75f, 0.92f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FLinearColor AccentColor = FLinearColor(0.f, 0.75f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FLinearColor TextColor = FLinearColor(0.92f, 0.96f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FLinearColor DimTextColor = FLinearColor(0.55f, 0.66f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	FLinearColor LockedTextColor = FLinearColor(0.4f, 0.42f, 0.46f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	TObjectPtr<USoundBase> HoverSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Style")
	TObjectPtr<USoundBase> ClickSound;
};

/**
 * Faction look of the menu - frame, buttons and the two accent colours. Laid over FMainMenuStyle,
 * everything else (background, fonts, sizes) stays as the style has it.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FMainMenuTheme
{
	GENERATED_BODY()

	FMainMenuTheme();

	/** Shown on the switch button, e.g. XENO. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FSlateBrush Panel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FSlateBrush ButtonNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FSlateBrush ButtonHovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FSlateBrush ButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FSlateBrush ButtonDisabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FLinearColor TitleColor = FLinearColor(0.75f, 0.92f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Theme")
	FLinearColor AccentColor = FLinearColor(0.f, 0.75f, 1.f);
};

/** Everything the main menu shows and does. Lives on AMainMenuActor and is handed to the widget. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FMainMenuConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FText Subtitle;

	/** Bottom right corner, e.g. the version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FText Footer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	TArray<FMainMenuEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FMainMenuStyle Style;

	/**
	 * Two or more = a small button in the top right corner switches between them (Singularians,
	 * Xeno, ...). The choice is kept in GameUserSettings. Empty = Style alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	TArray<FMainMenuTheme> Themes;

	/** Put on the screen before a map is opened. Empty = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	/** Window of the SaveLoad button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	TSubclassOf<USaveGameWidget> SaveGameWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FString SaveSlotName = TEXT("QuickSave");
};

/**
 * Classic main menu as a widget: background picture, title and a column of buttons - the same
 * choices the walk-in menu map offers with its switch actors (maps, replay, save/load, store page).
 *
 * Built in Slate in C++, so it needs no widget Blueprint; the look comes from FMainMenuStyle.
 * Place an AMainMenuActor in a map to show it, or create it yourself and call SetupMenu.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void SetupMenu(const FMainMenuConfig& InConfig);

	/** Re-reads the unlock state of every button (save slots, switch tags) and rebuilds the column. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void RefreshMenu();

	/** Runs the action of entry Index, as if its button was clicked. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void ActivateEntry(int32 Index);

	UFUNCTION(BlueprintPure, Category = "MainMenu")
	bool IsEntryUnlocked(int32 Index) const;

	/** Switches to Config.Themes[Index] and remembers it. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void SetTheme(int32 Index);

	UFUNCTION(BlueprintPure, Category = "MainMenu")
	int32 GetTheme() const { return ThemeIndex; }

	/** Z order of windows opened from the menu (save/load, replay browser) and of the loading screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	int32 PopupZOrder = 50;

	/**
	 * Seconds the loading screen is on screen before the map load starts. The load blocks the game
	 * thread, so without this the screen never gets drawn and the menu freezes instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu", meta = (ClampMin = "0"))
	float LoadingScreenLeadTime = 0.1f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;

private:
	/** Config.Style with the active theme laid over it. */
	void ApplyTheme();
	TSharedRef<SWidget> BuildRoot();
	TSharedRef<SWidget> BuildPanel();
	FReply HandleEntryClicked(int32 Index);
	FReply HandleThemeClicked();

	void OpenMap(const FMainMenuEntry& Entry);
	void ShowPopup(TSubclassOf<UUserWidget> WidgetClass);
	void OpenSaveLoad();

	UPROPERTY(EditAnywhere, Category = "MainMenu")
	FMainMenuConfig Config;

	/** What is drawn: Config.Style plus the theme. Slate keeps pointers to its brushes. */
	FMainMenuStyle ActiveStyle;

	int32 ThemeIndex = 0;

	/** Built from the style brushes; Slate keeps a pointer to it. */
	FButtonStyle ButtonStyle;

	/** Same look, slimmer padding - the theme switch in the corner. */
	FButtonStyle ThemeButtonStyle;

	/** Line under the title. An SImage without a brush would draw the default checkerboard. */
	FSlateBrush AccentLineBrush;

	TSharedPtr<SBox> RootHost;
	TSharedPtr<SBox> PanelHost;

	/** No second travel while the first is under way. */
	bool bTravelling = false;
};
