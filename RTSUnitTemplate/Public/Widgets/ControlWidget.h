// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/KeyLayoutWidgets.h"
#include "ControlWidget.generated.h"

class UCheckBox;
class UButton;
class UTextBlock;
class UOverlay;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlWidgetDone);

/**
 * Controls overview (Tab page 2) and keyboard layout editor.
 *
 * A Blueprint child WITHOUT own designer content gets a keyboard built in code: every key shows
 * what it does in the active layout (hover = tooltip), a click opens the key list
 * (URTSKeyBindingListWidget) with that key marked, and the
 * three layout buttons switch between Modern, Classic and User defined. A Blueprint child WITH its
 * own designer layout keeps it (only the three option check boxes are wired, as before).
 */
UCLASS()
class RTSUNITTEMPLATE_API UControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* SwapScrollCheckBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* SwapAttackMoveCheckBox;

	/**
	 * Dreht die Hochachse beim Verstellen des Kamerawinkels.
	 *
	 * BindWidgetOptional und nicht BindWidget: die beiden anderen Kaestchen gibt es in jedem
	 * Widget-Blueprint seit jeher, dieses hier nicht. Mit dem strengen BindWidget wuerde jeder
	 * bestehende ControlWidget-Blueprint - auch in RTSUnitExample und Lux - die Kompilierung
	 * verweigern, bis jemand das Kaestchen von Hand nachtraegt.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* InvertCameraAngleCheckBox;

	UFUNCTION()
	void OnSwapScrollChanged(bool bIsChecked);

	UFUNCTION()
	void OnSwapAttackMoveChanged(bool bIsChecked);

	UFUNCTION()
	void OnInvertCameraAngleChanged(bool bIsChecked);

	// --- Keyboard built in code -----------------------------------------------------------------

	/** Look of keys, buttons and panels. Also used by the layout choice and the rebind dialog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	FRTSKeyboardStyle Style;

	/** Width and height of one key in the code-built keyboard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	float KeySize = 58.f;

	/**
	 * Optional tooltip widget for keys and mouse entries. Its Text variables Name, TypeName and
	 * Description are filled (key, "Keyboard"/"Mouse", what it does). Empty = plain text tooltip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	TSubclassOf<UUserWidget> KeyTooltipClass;

	/** Shows a DONE button and a dark backdrop - set for the keyboard window (OpenAsWindow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	bool bShowAsWindow = false;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|Keys")
	FOnControlWidgetDone OnDone;

	/** True when this widget built the keyboard itself (no designer content). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	bool HasCodeKeyboard() const { return bBuiltKeyboard; }

	/** Puts the keyboard as a window on the viewport (DONE closes it). OnClosed runs after it is gone. */
	static UControlWidget* OpenAsWindow(APlayerController* PC, TSubclassOf<UControlWidget> WidgetClass, TFunction<void()> OnClosed);

	/** The class to open as keyboard window: HUDClass when it builds its keyboard in code, else UControlWidget. */
	static TSubclassOf<UControlWidget> ResolveKeyboardClass(UClass* HUDClass);

	/**
	 * The keyboard the game shows under Esc -> Controls: the HUD's control widget on the player's camera
	 * (faction specific). Null while the camera or its HUD is not there yet.
	 */
	static const UControlWidget* FindGameKeyboard(const APlayerController* PC);

	/** Called by the key caps: opens the key list with Key marked. */
	void OnKeyCapClicked(const FKey& Key);

	/** Rebuilds labels, tooltips and highlights from the active layout. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void RefreshKeys();

	/** Closes the window (DONE). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void CloseWindow();

protected:
	UFUNCTION() void OnBindingsChanged();
	UFUNCTION() void OnModernClicked();
	UFUNCTION() void OnClassicClicked();
	UFUNCTION() void OnUserClicked();
	UFUNCTION() void OnResetModernClicked();
	UFUNCTION() void OnResetClassicClicked();
	UFUNCTION() void OnDoneClicked();
	UFUNCTION() void OnCursorSize100Clicked();
	UFUNCTION() void OnCursorSize125Clicked();
	UFUNCTION() void OnCursorSize150Clicked();
	UFUNCTION() void OnCursorSize200Clicked();

private:
	struct FKeyCap
	{
		FKey Key;
		TWeakObjectPtr<UButton> Button;
		TWeakObjectPtr<UTextBlock> ActionLabel;
		bool bFixed = false;
		FText FixedTooltip;
	};

	void BuildKeyboard();
	class UWidget* MakeKey(const FKey& Key, const FText& Label, float Units, bool bFixed = false, const FText& FixedTooltip = FText());
	class UWidget* MakeGap(float Units);
	class UWidget* MakeMouseEntry(const FText& Label, const FText& Tooltip);
	FText BuildKeyTooltip(const FKey& Key) const;
	void ApplyTooltip(class UWidget* Target, const FText& Title, const FText& Type, const FText& Body);
	void UpdatePresetButtons();

	bool bBuiltKeyboard = false;
	TArray<FKeyCap> KeyCaps;
	TFunction<void()> OnClosedCallback;

	UPROPERTY(Transient) TArray<TObjectPtr<URTSKeyCapProxy>> KeyProxies;
	UPROPERTY(Transient) TObjectPtr<UOverlay> RootOverlay;
	UPROPERTY(Transient) TObjectPtr<UButton> ModernButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ClassicButton;
	UPROPERTY(Transient) TObjectPtr<UButton> UserButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ModernLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClassicLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UserLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> ResetModernButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ResetClassicButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LayoutHintText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UnassignedText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetectEnemiesMouseText;
	UPROPERTY(Transient) TObjectPtr<UButton> LeftClickMouseButton;
	/** Cursor size factors 1, 1.25, 1.5, 2 (on top of the automatic resolution scale). */
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> CursorSizeButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> CursorSizeLabels;
};
