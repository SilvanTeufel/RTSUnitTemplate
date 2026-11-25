// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSKeyBindings.generated.h"

/**
 * Everything a keyboard key can do in the RTS camera. The camera used to listen to the physical
 * keys themselves (W_Pressed, 1_Pressed ...); these actions are what the keys meant there, so a
 * layout can move them to other keys. Mouse buttons, Shift/Ctrl/Alt and Escape are not in here -
 * they stay where they are in every layout.
 */
UENUM(BlueprintType)
enum class ERTSKeyAction : uint8
{
	None,
	CameraForward,
	CameraBack,
	CameraLeft,
	CameraRight,
	CameraRotateLeft,
	CameraRotateRight,
	CameraZoomIn,
	CameraZoomOut,
	CameraJump,
	CameraLockUnit,
	CameraOrbit,
	SpawnEffects,
	Ability1,
	Ability2,
	Ability3,
	Ability4,
	Ability5,
	Ability6,
	SelectGroup1,
	SelectGroup2,
	SelectGroup3,
	SelectGroup4,
	SelectGroup5,
	SelectGroup6,
	SelectAltGroup1,
	SelectAltGroup2,
	SelectAltGroup3,
	SelectAltGroup4,
	SelectAltGroup5,
	SelectAltGroup6,
	SelectGroupQ,
	SelectGroupW,
	SelectGroupE,
	SelectGroupR,
	FKeyGroup1,
	FKeyGroup2,
	FKeyGroup3,
	FKeyGroup4,
	FKeyGroup5,
	FKeyGroup6,
	HoldPosition,
	DetectEnemies,
	CycleFormation,
	ShowHealthbars,
	ToggleOverview,
	OpenMenu,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ERTSKeyLayoutPreset : uint8
{
	/** Abilities on 1-6, groups on Ctrl + 1-6, camera on WASD. */
	Modern,
	/** Abilities on Q W E A S D, groups on 1-6, camera on the arrow keys. */
	Classic,
	/** The player's own layout, edited in the keyboard widget. */
	UserDefined
};

/** A key plus the modifiers that have to be held with it. Shift is never part of a chord - it queues orders. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FRTSKeyChord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	bool bCtrl = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	bool bAlt = false;

	FRTSKeyChord() = default;
	FRTSKeyChord(const FKey& InKey, bool bInCtrl = false, bool bInAlt = false)
		: Key(InKey), bCtrl(bInCtrl), bAlt(bInAlt) {}

	bool IsValid() const { return Key.IsValid(); }
	bool operator==(const FRTSKeyChord& Other) const
	{
		return Key == Other.Key && bCtrl == Other.bCtrl && bAlt == Other.bAlt;
	}
	bool operator!=(const FRTSKeyChord& Other) const { return !(*this == Other); }

	/** "CTRL + 1", "W", "ARROW UP". Empty text for an invalid chord. */
	FText ToText() const;
};

USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FRTSKeyActionBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	ERTSKeyAction Action = ERTSKeyAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Keys")
	FRTSKeyChord Chord;
};

/** Slot "RTSKeyBindings": chosen layout and the player's own layout, kept apart from the presets. */
UCLASS()
class RTSUNITTEMPLATE_API URTSKeyBindingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("RTSKeyBindings");

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	ERTSKeyLayoutPreset Preset = ERTSKeyLayoutPreset::Modern;

	/** False until the player picked a layout once - the start screen asks until then. */
	UPROPERTY()
	bool bLayoutChosen = false;

	/** Empty = never edited; the user layout then starts as a copy of the preset that was active. */
	UPROPERTY()
	TArray<FRTSKeyActionBinding> UserBindings;

	/** Player's cursor size on top of the automatic resolution scale (1 = automatic only). */
	UPROPERTY()
	float CursorScale = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRTSKeyBindingsChanged);

/**
 * Holds the active key layout for this game instance and saves it. Presets are built in code and
 * never change; picking Modern or Classic only switches which table is active, the user layout
 * stays untouched in UserBindings.
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSKeyBindingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static URTSKeyBindingSubsystem* Get(const UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|Keys")
	FOnRTSKeyBindingsChanged OnBindingsChanged;

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	ERTSKeyLayoutPreset GetActivePreset() const { return ActivePreset; }

	/** Switches the layout and marks the choice as made. UserDefined starts from the active preset if it was never edited. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void SetActivePreset(ERTSKeyLayoutPreset Preset);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	bool HasChosenLayout() const { return bLayoutChosen; }

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	FRTSKeyChord GetChord(ERTSKeyAction Action) const;

	/** Chord of Action in the active layout as text ("CTRL + 1"); empty when the action has no key. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	FText GetChordText(ERTSKeyAction Action) const;

	/** Action bound to exactly this chord in the active layout, None if there is none. */
	ERTSKeyAction FindAction(const FRTSKeyChord& Chord) const;

	/** Every binding of the active layout that uses Key, with or without modifiers. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	TArray<FRTSKeyActionBinding> GetBindingsForKey(const FKey& Key) const;

	/** Actions that have no key in the active layout. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	TArray<ERTSKeyAction> GetUnboundActions() const;

	/** The action that already uses Chord in the user layout, other than Action itself. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	ERTSKeyAction FindUserConflict(ERTSKeyAction Action, const FRTSKeyChord& Chord) const;

	/**
	 * Puts Action on Chord in the user layout. Whatever used Chord before loses its key - a chord
	 * never does two things. Switches to the user layout and saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void SetUserBinding(ERTSKeyAction Action, const FRTSKeyChord& Chord);

	/** Removes the key of Action in the user layout (switches to it). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void ClearUserBinding(ERTSKeyAction Action);

	/** Copies a preset into the user layout. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Keys")
	void ResetUserLayout(ERTSKeyLayoutPreset FromPreset);

	/** Keys a player may put an action on: no modifiers alone, no Escape, no left/right mouse, no axes. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	static bool CanBindKey(const FKey& Key);

	/** True for a key the camera handles itself in some preset - those never reach the old per-key input actions. */
	static bool IsManagedKey(const FKey& Key);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	static FText GetActionDisplayName(ERTSKeyAction Action);

	/** Two to eight letters for the label on a key cap ("PAN UP", "ABIL 1"). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	static FText GetActionShortName(ERTSKeyAction Action);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	static FText GetPresetDisplayName(ERTSKeyLayoutPreset Preset);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys")
	static FText GetPresetDescription(ERTSKeyLayoutPreset Preset);

	/** Text for a key without modifiers ("ARROW UP", "1", "PAGE DOWN"). */
	static FText GetKeyText(const FKey& Key);

	static const TArray<FRTSKeyActionBinding>& GetPresetBindings(ERTSKeyLayoutPreset Preset);

	/** Actions whose key also works while Ctrl or Alt are held, as long as that combo is not bound itself (camera panning). */
	static bool IsModifierTolerant(ERTSKeyAction Action);

	/** Replaces chords of the Modern layout in a hint text with the active ones ("press 1" -> "press Q") and resolves {Key:Ability1}. */
	FText LocalizeKeyHints(const FText& Text) const;

	/** The chord that does in the active layout what Chord does in the Modern layout. Invalid when that action has no key. */
	FRTSKeyChord TranslateModernChord(const FRTSKeyChord& Chord) const;

	/** Player's cursor size factor on top of the automatic resolution scale (1 = automatic only). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Cursor")
	float GetCursorScale() const { return CursorScale; }

	/** Saves the cursor size factor (clamped 0.75..3) and re-applies the cursor of every local controller. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Cursor")
	void SetCursorScale(float NewScale);

	/** Back to "never chosen" (RTS.Keys.ResetSave) - the next start asks again. */
	void ResetForTests();

private:
	const TArray<FRTSKeyActionBinding>& ActiveBindings() const;
	void SaveToDisk();
	void Broadcast();

	ERTSKeyLayoutPreset ActivePreset = ERTSKeyLayoutPreset::Modern;
	bool bLayoutChosen = false;
	TArray<FRTSKeyActionBinding> UserBindings;
	float CursorScale = 1.f;
};

/** Blueprint helpers for tooltips: read the active layout without fetching the subsystem. */
UCLASS()
class RTSUNITTEMPLATE_API URTSKeyBindingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** "CTRL + 1" in the active layout, empty when unbound. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys", meta = (WorldContext = "WorldContextObject"))
	static FText GetKeyBindingText(const UObject* WorldContextObject, ERTSKeyAction Action);

	/** Key of ability button Index (0 = first ability) in the active layout. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys", meta = (WorldContext = "WorldContextObject"))
	static FText GetAbilityKeyText(const UObject* WorldContextObject, int32 Index);

	/** "Production  [Q]" - Text with the key of ability button Index appended; Text alone when that slot has no key. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Keys", meta = (WorldContext = "WorldContextObject"))
	static FText AppendAbilityKey(const UObject* WorldContextObject, FText Text, int32 Index);
};
