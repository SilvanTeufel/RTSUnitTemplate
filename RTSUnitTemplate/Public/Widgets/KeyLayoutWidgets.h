// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Controller/Input/RTSKeyBindings.h"
#include "KeyLayoutWidgets.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UVerticalBox;
class UWidgetTree;
class UControlWidget;
class UMaterialInterface;

/**
 * Look of the keyboard widget and the layout windows. All brushes are optional: without them the
 * widgets draw plain dark boxes (that is how the plugin ships). Astra Helix sets its Singularian /
 * Xeno materials in the Blueprint children of UControlWidget.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FRTSKeyboardStyle
{
	GENERATED_BODY()

	/** Background of the panels (keyboard, dialog, layout cards). Material or texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> Panel;

	/** Key cap of a key that does something. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> KeyNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> KeyHover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> KeyPressed;

	/** Key cap of a key without a function (and of the fixed keys Shift/Ctrl/Alt/Esc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> KeyUnbound;

	/** Wide buttons (presets, DONE, REBIND ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> ButtonNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> ButtonHover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.MaterialInterface,/Script/Engine.Texture2D"))
	TObjectPtr<UObject> ButtonPressed;

	/** Font of titles and key names. Missing = engine font. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath HeadingFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-SemiBold_Font.ChakraPetch-SemiBold_Font"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath BodyFontPath = FSoftObjectPath(TEXT("/Game/AIGenerated/Fonts/ChakraPetch-Medium_Font.ChakraPetch-Medium_Font"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor TitleColor = FLinearColor(0.82f, 0.89f, 0.96f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor TextColor = FLinearColor(0.92f, 0.95f, 1.f, 1.f);

	/** Key names, active preset, selected function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor AccentColor = FLinearColor(0.21f, 0.64f, 0.69f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor DimColor = FLinearColor(0.55f, 0.62f, 0.72f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor WarningColor = FLinearColor(1.f, 0.62f, 0.25f, 1.f);

	/** Darkens the game behind the windows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BackdropColor = FLinearColor(0.f, 0.005f, 0.01f, 0.78f);
};

/** Helpers shared by the code-built key widgets. */
namespace RTSKeyUI
{
	RTSUNITTEMPLATE_API FSlateBrush MakeBrush(UObject* Resource, const FLinearColor& FallbackColor, const FLinearColor& Tint = FLinearColor::White);
	RTSUNITTEMPLATE_API FSlateFontInfo MakeFont(const FSoftObjectPath& Path, int32 Size, const ANSICHAR* FallbackStyle);
	RTSUNITTEMPLATE_API UTextBlock* MakeText(UWidgetTree* Tree, const FRTSKeyboardStyle& Style, const FText& Text, int32 Size,
		const FLinearColor& Color, bool bHeading = false, int32 LetterSpacing = 0);
	/** Button with the wide-button brushes and a centered label. */
	RTSUNITTEMPLATE_API UButton* MakeButton(UWidgetTree* Tree, const FRTSKeyboardStyle& Style, const FText& Label, int32 Size,
		UTextBlock** OutLabel = nullptr, const FLinearColor& Tint = FLinearColor::White);
	RTSUNITTEMPLATE_API void SetButtonTint(UButton* Button, const FRTSKeyboardStyle& Style, const FLinearColor& Tint);
}

/** One key cap of the keyboard widget: knows its key and tells the owner when it was clicked. */
UCLASS()
class RTSUNITTEMPLATE_API URTSKeyCapProxy : public UObject
{
	GENERATED_BODY()

public:
	FKey Key;

	UPROPERTY()
	TWeakObjectPtr<UControlWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};

/** One row of the key list: tells the list which action's key button (or clear button) was clicked. */
UCLASS()
class RTSUNITTEMPLATE_API URTSKeyRowProxy : public UObject
{
	GENERATED_BODY()

public:
	ERTSKeyAction Action = ERTSKeyAction::None;

	UPROPERTY()
	TWeakObjectPtr<class URTSKeyBindingListWidget> Owner;

	UFUNCTION()
	void HandleBind();

	UFUNCTION()
	void HandleClear();
};

/**
 * Key list for the user-defined layout: every function on the left (grouped: camera, abilities,
 * selection, commands, interface), its key on the right. Clicking the key button waits for the new
 * key (CTRL / ALT combos allowed, ESC cancels); a key already in use asks before it is taken away.
 * Opened from the layout choice (User defined), the Esc menu and by clicking a key on the keyboard.
 */
UCLASS(BlueprintType)
class RTSUNITTEMPLATE_API URTSKeyBindingListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Puts the list on the viewport. FocusKey: rows using this key are marked and scrolled to. */
	static URTSKeyBindingListWidget* Open(APlayerController* PC, const FRTSKeyboardStyle& InStyle, const FKey& FocusKey, TFunction<void()> OnClosed);

	/** Starts waiting for the new key of Action (also switches a preset to the user layout). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void BeginCapture(ERTSKeyAction Action);

	/** What a key press does while capturing - callable for tests. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void SubmitKey(FKey Key, bool bCtrl, bool bAlt);

	/** Removes the key of Action in the user layout. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void ClearAction(ERTSKeyAction Action);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void ConfirmReplace();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void CancelCapture();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void CloseList();

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	bool IsCapturing() const { return CaptureAction != ERTSKeyAction::None; }

protected:
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION() void OnBindingsChanged();
	UFUNCTION() void OnModernClicked();
	UFUNCTION() void OnClassicClicked();
	UFUNCTION() void OnUserClicked();
	UFUNCTION() void OnResetModernClicked();
	UFUNCTION() void OnResetClassicClicked();

private:
	struct FRow
	{
		ERTSKeyAction Action = ERTSKeyAction::None;
		TWeakObjectPtr<UButton> KeyButton;
		TWeakObjectPtr<UTextBlock> KeyLabel;
		TWeakObjectPtr<UButton> ClearButton;
		TWeakObjectPtr<UTextBlock> NameLabel;
	};

	void Build();
	void Refresh();
	FReply HandleKey(const FKey& Key, bool bCtrl, bool bAlt);
	void SetStatus(const FText& Text, const FLinearColor& Color);

	FRTSKeyboardStyle Style;
	FKey FocusKey;
	TArray<FRow> Rows;
	ERTSKeyAction CaptureAction = ERTSKeyAction::None;
	ERTSKeyAction PendingConflict = ERTSKeyAction::None;
	FRTSKeyChord PendingChord;
	TFunction<void()> OnClosedCallback;

	UPROPERTY(Transient) TArray<TObjectPtr<URTSKeyRowProxy>> RowProxies;
	UPROPERTY(Transient) TObjectPtr<class UScrollBox> List;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReplaceButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ModernButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ClassicButton;
	UPROPERTY(Transient) TObjectPtr<UButton> UserButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ModernLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClassicLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UserLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> ResetModernButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ResetClassicButton;
};

/**
 * Modern / Classic / User defined. Shown before the difficulty when no layout was saved yet, and
 * from the Esc menu. User defined opens the key list (URTSKeyBindingListWidget) to edit the own layout.
 */
UCLASS(BlueprintType)
class RTSUNITTEMPLATE_API URTSKeyLayoutChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Creates the window on the viewport. KeyboardClass: its CDO gives the style (also of
	 * the key list that User defined opens). CardMaterial overrides the card background (the faction card of the start
	 * screen). bFirstRun: no close button - a layout has to be picked. OnFinished runs once the
	 * window is gone (after the keyboard closed, for User defined).
	 */
	static URTSKeyLayoutChoiceWidget* Open(APlayerController* PC, TSubclassOf<UControlWidget> KeyboardClass, UMaterialInterface* CardMaterial,
		bool bFirstRun, TFunction<void()> OnFinished);

	/** Closes the topmost key window of World (keyboard window first). False when none was open or it may not close. */
	static bool CloseTopmostWindow(const UWorld* World);

	/** True while a layout or keyboard window is open in World. */
	static bool IsAnyWindowOpen(const UWorld* World);

	static void RegisterWindow(UUserWidget* Window, bool bClosableWithEsc);
	static void UnregisterWindow(UUserWidget* Window);

	void Finish();

	/** Same as a click on a card. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void ChoosePreset(ERTSKeyLayoutPreset Preset) { Choose(Preset); }

protected:
	virtual void NativeDestruct() override;

	UFUNCTION() void OnModernClicked();
	UFUNCTION() void OnClassicClicked();
	UFUNCTION() void OnUserClicked();
	UFUNCTION() void OnCloseClicked();

private:
	void Build(UMaterialInterface* CardMaterial);
	void Choose(ERTSKeyLayoutPreset Preset);

	TSubclassOf<UControlWidget> KeyboardClass;
	FRTSKeyboardStyle Style;
	bool bFirstRun = false;
	bool bFinished = false;
	TFunction<void()> OnFinishedCallback;
};
