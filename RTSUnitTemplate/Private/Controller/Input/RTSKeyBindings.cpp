// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/Input/RTSKeyBindings.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/LocalPlayer.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Internationalization/Regex.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Widgets/ControlWidget.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "RTSKeyBindings"

namespace RTSKeyBindingsLocal
{
	using A = ERTSKeyAction;

	void AddShared(TArray<FRTSKeyActionBinding>& Out)
	{
		auto Add = [&Out](A Action, const FKey& Key, bool bCtrl = false, bool bAlt = false)
		{
			FRTSKeyActionBinding B;
			B.Action = Action;
			B.Chord = FRTSKeyChord(Key, bCtrl, bAlt);
			Out.Add(B);
		};
		Add(A::CameraZoomIn, EKeys::X);
		Add(A::CameraZoomOut, EKeys::Y);
		Add(A::CameraJump, EKeys::SpaceBar);
		Add(A::CameraLockUnit, EKeys::G);
		Add(A::CameraOrbit, EKeys::P, true);
		Add(A::SpawnEffects, EKeys::O, true);
		const FKey Digits[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i)
		{
			Add(static_cast<A>(static_cast<uint8>(A::SelectAltGroup1) + i), Digits[i], false, true);
		}
		Add(A::SelectGroupQ, EKeys::Q, true);
		Add(A::SelectGroupW, EKeys::W, true);
		Add(A::SelectGroupE, EKeys::E, true);
		Add(A::SelectGroupR, EKeys::R, true);
		const FKey FKeys[6] = { EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6 };
		for (int32 i = 0; i < 6; ++i)
		{
			Add(static_cast<A>(static_cast<uint8>(A::FKeyGroup1) + i), FKeys[i]);
		}
		Add(A::HoldPosition, EKeys::R);
		Add(A::DetectEnemies, EKeys::T);
		Add(A::CycleFormation, EKeys::C);
		Add(A::ShowHealthbars, EKeys::V);
		Add(A::ToggleOverview, EKeys::Tab);
		Add(A::OpenMenu, EKeys::M);
	}

	TArray<FRTSKeyActionBinding> BuildModern()
	{
		TArray<FRTSKeyActionBinding> Out;
		auto Add = [&Out](A Action, const FKey& Key, bool bCtrl = false)
		{
			FRTSKeyActionBinding B;
			B.Action = Action;
			B.Chord = FRTSKeyChord(Key, bCtrl, false);
			Out.Add(B);
		};
		Add(A::CameraForward, EKeys::W);
		Add(A::CameraBack, EKeys::S);
		Add(A::CameraLeft, EKeys::A);
		Add(A::CameraRight, EKeys::D);
		Add(A::CameraRotateLeft, EKeys::Q);
		Add(A::CameraRotateRight, EKeys::E);
		const FKey Digits[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i)
		{
			Add(static_cast<A>(static_cast<uint8>(A::Ability1) + i), Digits[i]);
			Add(static_cast<A>(static_cast<uint8>(A::SelectGroup1) + i), Digits[i], true);
		}
		AddShared(Out);
		return Out;
	}

	TArray<FRTSKeyActionBinding> BuildClassic()
	{
		TArray<FRTSKeyActionBinding> Out;
		auto Add = [&Out](A Action, const FKey& Key)
		{
			FRTSKeyActionBinding B;
			B.Action = Action;
			B.Chord = FRTSKeyChord(Key, false, false);
			Out.Add(B);
		};
		Add(A::CameraForward, EKeys::Up);
		Add(A::CameraBack, EKeys::Down);
		Add(A::CameraLeft, EKeys::Left);
		Add(A::CameraRight, EKeys::Right);
		Add(A::CameraRotateLeft, EKeys::Delete);
		Add(A::CameraRotateRight, EKeys::PageDown);
		const FKey AbilityKeys[6] = { EKeys::Q, EKeys::W, EKeys::E, EKeys::A, EKeys::S, EKeys::D };
		const FKey Digits[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i)
		{
			Add(static_cast<A>(static_cast<uint8>(A::Ability1) + i), AbilityKeys[i]);
			Add(static_cast<A>(static_cast<uint8>(A::SelectGroup1) + i), Digits[i]);
		}
		AddShared(Out);
		return Out;
	}

	const FRTSKeyActionBinding* FindIn(const TArray<FRTSKeyActionBinding>& Bindings, ERTSKeyAction Action)
	{
		return Bindings.FindByPredicate([Action](const FRTSKeyActionBinding& B) { return B.Action == Action; });
	}

	/** Word in a hint text ("1", "W", "F2", "Tab", "Space") -> key. */
	FKey KeyFromHintToken(const FString& Token)
	{
		static const TMap<FString, FKey> Named = {
			{ TEXT("0"), EKeys::Zero }, { TEXT("1"), EKeys::One }, { TEXT("2"), EKeys::Two }, { TEXT("3"), EKeys::Three },
			{ TEXT("4"), EKeys::Four }, { TEXT("5"), EKeys::Five }, { TEXT("6"), EKeys::Six }, { TEXT("7"), EKeys::Seven },
			{ TEXT("8"), EKeys::Eight }, { TEXT("9"), EKeys::Nine }, { TEXT("TAB"), EKeys::Tab }, { TEXT("SPACE"), EKeys::SpaceBar },
		};
		const FString Upper = Token.ToUpper();
		if (const FKey* Found = Named.Find(Upper))
		{
			return *Found;
		}
		const FKey Key(*Upper);
		return Key.IsValid() && !Key.IsMouseButton() && !Key.IsGamepadKey() ? Key : FKey();
	}
}

FText FRTSKeyChord::ToText() const
{
	if (!Key.IsValid())
	{
		return FText::GetEmpty();
	}
	FString Out;
	if (bCtrl)
	{
		Out += TEXT("CTRL + ");
	}
	if (bAlt)
	{
		Out += TEXT("ALT + ");
	}
	Out += URTSKeyBindingSubsystem::GetKeyText(Key).ToString();
	return FText::FromString(Out);
}

void URTSKeyBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const URTSKeyBindingsSave* Save = Cast<URTSKeyBindingsSave>(
		UGameplayStatics::LoadGameFromSlot(URTSKeyBindingsSave::SlotName, 0)))
	{
		ActivePreset = Save->Preset;
		bLayoutChosen = Save->bLayoutChosen;
		UserBindings = Save->UserBindings;
		CursorScale = FMath::Clamp(Save->CursorScale > 0.f ? Save->CursorScale : 1.f, 0.75f, 3.f);
		UserBindings.RemoveAll([](const FRTSKeyActionBinding& B)
		{
			return B.Action == ERTSKeyAction::None || B.Action >= ERTSKeyAction::Count || !B.Chord.IsValid();
		});
	}
	if (ActivePreset == ERTSKeyLayoutPreset::UserDefined && UserBindings.Num() == 0)
	{
		UserBindings = GetPresetBindings(ERTSKeyLayoutPreset::Modern);
	}
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] Layout %s, gewaehlt=%d, eigene Belegungen=%d."),
		*GetPresetDisplayName(ActivePreset).ToString(), bLayoutChosen ? 1 : 0, UserBindings.Num());
}

URTSKeyBindingSubsystem* URTSKeyBindingSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<URTSKeyBindingSubsystem>() : nullptr;
}

const TArray<FRTSKeyActionBinding>& URTSKeyBindingSubsystem::GetPresetBindings(ERTSKeyLayoutPreset Preset)
{
	static const TArray<FRTSKeyActionBinding> Modern = RTSKeyBindingsLocal::BuildModern();
	static const TArray<FRTSKeyActionBinding> Classic = RTSKeyBindingsLocal::BuildClassic();
	return Preset == ERTSKeyLayoutPreset::Classic ? Classic : Modern;
}

const TArray<FRTSKeyActionBinding>& URTSKeyBindingSubsystem::ActiveBindings() const
{
	if (ActivePreset == ERTSKeyLayoutPreset::UserDefined)
	{
		return UserBindings;
	}
	return GetPresetBindings(ActivePreset);
}

void URTSKeyBindingSubsystem::SetActivePreset(ERTSKeyLayoutPreset Preset)
{
	if (Preset == ERTSKeyLayoutPreset::UserDefined && UserBindings.Num() == 0)
	{
		// Erste eigene Belegung: von dem ausgehen, was der Spieler gerade gewohnt ist.
		UserBindings = GetPresetBindings(ActivePreset == ERTSKeyLayoutPreset::UserDefined ? ERTSKeyLayoutPreset::Modern : ActivePreset);
	}
	ActivePreset = Preset;
	bLayoutChosen = true;
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] Layout gewechselt auf %s."), *GetPresetDisplayName(Preset).ToString());
	SaveToDisk();
	Broadcast();
}

FRTSKeyChord URTSKeyBindingSubsystem::GetChord(ERTSKeyAction Action) const
{
	const FRTSKeyActionBinding* Found = RTSKeyBindingsLocal::FindIn(ActiveBindings(), Action);
	return Found ? Found->Chord : FRTSKeyChord();
}

FText URTSKeyBindingSubsystem::GetChordText(ERTSKeyAction Action) const
{
	return GetChord(Action).ToText();
}

ERTSKeyAction URTSKeyBindingSubsystem::FindAction(const FRTSKeyChord& Chord) const
{
	for (const FRTSKeyActionBinding& B : ActiveBindings())
	{
		if (B.Chord == Chord)
		{
			return B.Action;
		}
	}
	return ERTSKeyAction::None;
}

TArray<FRTSKeyActionBinding> URTSKeyBindingSubsystem::GetBindingsForKey(const FKey& Key) const
{
	TArray<FRTSKeyActionBinding> Out;
	for (const FRTSKeyActionBinding& B : ActiveBindings())
	{
		if (B.Chord.Key == Key)
		{
			Out.Add(B);
		}
	}
	Out.Sort([](const FRTSKeyActionBinding& L, const FRTSKeyActionBinding& R)
	{
		const int32 LM = (L.Chord.bCtrl ? 1 : 0) + (L.Chord.bAlt ? 2 : 0);
		const int32 RM = (R.Chord.bCtrl ? 1 : 0) + (R.Chord.bAlt ? 2 : 0);
		return LM < RM;
	});
	return Out;
}

TArray<ERTSKeyAction> URTSKeyBindingSubsystem::GetUnboundActions() const
{
	TArray<ERTSKeyAction> Out;
	for (uint8 i = static_cast<uint8>(ERTSKeyAction::None) + 1; i < static_cast<uint8>(ERTSKeyAction::Count); ++i)
	{
		const ERTSKeyAction Action = static_cast<ERTSKeyAction>(i);
		if (!GetChord(Action).IsValid())
		{
			Out.Add(Action);
		}
	}
	return Out;
}

ERTSKeyAction URTSKeyBindingSubsystem::FindUserConflict(ERTSKeyAction Action, const FRTSKeyChord& Chord) const
{
	const TArray<FRTSKeyActionBinding>& Source = UserBindings.Num() > 0 ? UserBindings : ActiveBindings();
	for (const FRTSKeyActionBinding& B : Source)
	{
		if (B.Action != Action && B.Chord == Chord)
		{
			return B.Action;
		}
	}
	return ERTSKeyAction::None;
}

void URTSKeyBindingSubsystem::SetUserBinding(ERTSKeyAction Action, const FRTSKeyChord& Chord)
{
	if (Action == ERTSKeyAction::None || Action >= ERTSKeyAction::Count || !Chord.IsValid() || !CanBindKey(Chord.Key))
	{
		return;
	}
	if (UserBindings.Num() == 0)
	{
		UserBindings = ActiveBindings();
	}
	// Eine Taste (mit ihren Modifikatoren) macht genau eine Sache - die alte Belegung faellt weg.
	UserBindings.RemoveAll([&](const FRTSKeyActionBinding& B) { return B.Action == Action || B.Chord == Chord; });
	FRTSKeyActionBinding NewBinding;
	NewBinding.Action = Action;
	NewBinding.Chord = Chord;
	UserBindings.Add(NewBinding);

	ActivePreset = ERTSKeyLayoutPreset::UserDefined;
	bLayoutChosen = true;
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] %s liegt jetzt auf %s."),
		*GetActionDisplayName(Action).ToString(), *Chord.ToText().ToString());
	SaveToDisk();
	Broadcast();
}

void URTSKeyBindingSubsystem::ClearUserBinding(ERTSKeyAction Action)
{
	if (UserBindings.Num() == 0)
	{
		UserBindings = ActiveBindings();
	}
	UserBindings.RemoveAll([Action](const FRTSKeyActionBinding& B) { return B.Action == Action; });
	ActivePreset = ERTSKeyLayoutPreset::UserDefined;
	bLayoutChosen = true;
	SaveToDisk();
	Broadcast();
}

void URTSKeyBindingSubsystem::ResetUserLayout(ERTSKeyLayoutPreset FromPreset)
{
	UserBindings = GetPresetBindings(FromPreset == ERTSKeyLayoutPreset::UserDefined ? ERTSKeyLayoutPreset::Modern : FromPreset);
	SaveToDisk();
	Broadcast();
}

bool URTSKeyBindingSubsystem::CanBindKey(const FKey& Key)
{
	if (!Key.IsValid() || Key.IsModifierKey() || Key.IsGamepadKey() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsTouch())
	{
		return false;
	}
	if (Key == EKeys::Escape || Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton
		|| Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown || Key == EKeys::AnyKey)
	{
		return false;
	}
	return true;
}

bool URTSKeyBindingSubsystem::IsManagedKey(const FKey& Key)
{
	static TSet<FKey> Managed;
	if (Managed.Num() == 0)
	{
		for (const ERTSKeyLayoutPreset Preset : { ERTSKeyLayoutPreset::Modern, ERTSKeyLayoutPreset::Classic })
		{
			for (const FRTSKeyActionBinding& B : GetPresetBindings(Preset))
			{
				Managed.Add(B.Chord.Key);
			}
		}
	}
	return Managed.Contains(Key);
}

bool URTSKeyBindingSubsystem::IsModifierTolerant(ERTSKeyAction Action)
{
	switch (Action)
	{
	case ERTSKeyAction::CameraForward:
	case ERTSKeyAction::CameraBack:
	case ERTSKeyAction::CameraLeft:
	case ERTSKeyAction::CameraRight:
	case ERTSKeyAction::CameraRotateLeft:
	case ERTSKeyAction::CameraRotateRight:
	case ERTSKeyAction::CameraZoomIn:
	case ERTSKeyAction::CameraZoomOut:
	case ERTSKeyAction::CameraJump:
	case ERTSKeyAction::FKeyGroup1:
	case ERTSKeyAction::FKeyGroup2:
	case ERTSKeyAction::FKeyGroup3:
	case ERTSKeyAction::FKeyGroup4:
	case ERTSKeyAction::FKeyGroup5:
	case ERTSKeyAction::FKeyGroup6:
		return true;
	default:
		return false;
	}
}

FText URTSKeyBindingSubsystem::GetKeyText(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return FText::GetEmpty();
	}
	static const TMap<FKey, FString> Overrides = {
		{ EKeys::SpaceBar, TEXT("SPACE") }, { EKeys::Up, TEXT("ARROW UP") }, { EKeys::Down, TEXT("ARROW DOWN") },
		{ EKeys::Left, TEXT("ARROW LEFT") }, { EKeys::Right, TEXT("ARROW RIGHT") }, { EKeys::Escape, TEXT("ESC") },
		{ EKeys::BackSpace, TEXT("BACKSPACE") }, { EKeys::CapsLock, TEXT("CAPS") },
		{ EKeys::ThumbMouseButton, TEXT("MOUSE 4") }, { EKeys::ThumbMouseButton2, TEXT("MOUSE 5") },
		{ EKeys::MiddleMouseButton, TEXT("MIDDLE MOUSE") },
	};
	if (const FString* Found = Overrides.Find(Key))
	{
		return FText::FromString(*Found);
	}
	return FText::FromString(Key.GetDisplayName(false).ToString().ToUpper());
}

FText URTSKeyBindingSubsystem::GetActionDisplayName(ERTSKeyAction Action)
{
	const int32 Index = static_cast<int32>(Action);
	switch (Action)
	{
	case ERTSKeyAction::CameraForward: return LOCTEXT("CameraForward", "Pan camera forward");
	case ERTSKeyAction::CameraBack: return LOCTEXT("CameraBack", "Pan camera back");
	case ERTSKeyAction::CameraLeft: return LOCTEXT("CameraLeft", "Pan camera left");
	case ERTSKeyAction::CameraRight: return LOCTEXT("CameraRight", "Pan camera right");
	case ERTSKeyAction::CameraRotateLeft: return LOCTEXT("CameraRotateLeft", "Rotate camera left");
	case ERTSKeyAction::CameraRotateRight: return LOCTEXT("CameraRotateRight", "Rotate camera right");
	case ERTSKeyAction::CameraZoomIn: return LOCTEXT("CameraZoomIn", "Zoom camera in");
	case ERTSKeyAction::CameraZoomOut: return LOCTEXT("CameraZoomOut", "Zoom camera out");
	case ERTSKeyAction::CameraJump: return LOCTEXT("CameraJump", "Jump camera (hold + left click)");
	case ERTSKeyAction::CameraLockUnit: return LOCTEXT("CameraLockUnit", "Lock camera on the selected unit");
	case ERTSKeyAction::CameraOrbit: return LOCTEXT("CameraOrbit", "Orbit camera");
	case ERTSKeyAction::SpawnEffects: return LOCTEXT("SpawnEffects", "Spawn effects");
	case ERTSKeyAction::HoldPosition: return LOCTEXT("HoldPosition", "Hold position");
	case ERTSKeyAction::DetectEnemies: return LOCTEXT("DetectEnemies", "Detect enemies (+ left click: run there)");
	case ERTSKeyAction::CycleFormation: return LOCTEXT("CycleFormation", "Cycle unit formation");
	case ERTSKeyAction::ShowHealthbars: return LOCTEXT("ShowHealthbars", "Show friendly health bars");
	case ERTSKeyAction::ToggleOverview: return LOCTEXT("ToggleOverview", "Next overview screen");
	case ERTSKeyAction::OpenMenu: return LOCTEXT("OpenMenu", "Open the menu");
	case ERTSKeyAction::SelectGroupQ: return LOCTEXT("SelectGroupQ", "Select group Q");
	case ERTSKeyAction::SelectGroupW: return LOCTEXT("SelectGroupW", "Select group W");
	case ERTSKeyAction::SelectGroupE: return LOCTEXT("SelectGroupE", "Select group E");
	case ERTSKeyAction::SelectGroupR: return LOCTEXT("SelectGroupR", "Select group R");
	default: break;
	}
	if (Action >= ERTSKeyAction::Ability1 && Action <= ERTSKeyAction::Ability6)
	{
		return FText::Format(LOCTEXT("Ability", "Use ability {0}"), Index - static_cast<int32>(ERTSKeyAction::Ability1) + 1);
	}
	if (Action >= ERTSKeyAction::SelectGroup1 && Action <= ERTSKeyAction::SelectGroup6)
	{
		const int32 N = Index - static_cast<int32>(ERTSKeyAction::SelectGroup1) + 1;
		return N <= 4
			? FText::Format(LOCTEXT("SelectTier", "Select T{0} units"), N)
			: FText::Format(LOCTEXT("SelectGroup", "Select group {0}"), N);
	}
	if (Action >= ERTSKeyAction::SelectAltGroup1 && Action <= ERTSKeyAction::SelectAltGroup6)
	{
		return FText::Format(LOCTEXT("SelectAltGroup", "Select alt group {0}"), Index - static_cast<int32>(ERTSKeyAction::SelectAltGroup1) + 1);
	}
	if (Action >= ERTSKeyAction::FKeyGroup1 && Action <= ERTSKeyAction::FKeyGroup6)
	{
		return FText::Format(LOCTEXT("FKeyGroup", "F-group {0} (tap: select, hold: assign)"), Index - static_cast<int32>(ERTSKeyAction::FKeyGroup1) + 1);
	}
	return FText::GetEmpty();
}

FText URTSKeyBindingSubsystem::GetActionShortName(ERTSKeyAction Action)
{
	const int32 Index = static_cast<int32>(Action);
	switch (Action)
	{
	case ERTSKeyAction::CameraForward: return LOCTEXT("ShortForward", "PAN UP");
	case ERTSKeyAction::CameraBack: return LOCTEXT("ShortBack", "PAN DN");
	case ERTSKeyAction::CameraLeft: return LOCTEXT("ShortLeft", "PAN L");
	case ERTSKeyAction::CameraRight: return LOCTEXT("ShortRight", "PAN R");
	case ERTSKeyAction::CameraRotateLeft: return LOCTEXT("ShortRotL", "ROT L");
	case ERTSKeyAction::CameraRotateRight: return LOCTEXT("ShortRotR", "ROT R");
	case ERTSKeyAction::CameraZoomIn: return LOCTEXT("ShortZoomIn", "ZOOM +");
	case ERTSKeyAction::CameraZoomOut: return LOCTEXT("ShortZoomOut", "ZOOM -");
	case ERTSKeyAction::CameraJump: return LOCTEXT("ShortJump", "JUMP");
	case ERTSKeyAction::CameraLockUnit: return LOCTEXT("ShortLock", "LOCK");
	case ERTSKeyAction::CameraOrbit: return LOCTEXT("ShortOrbit", "ORBIT");
	case ERTSKeyAction::SpawnEffects: return LOCTEXT("ShortFx", "FX");
	case ERTSKeyAction::HoldPosition: return LOCTEXT("ShortHold", "HOLD");
	case ERTSKeyAction::DetectEnemies: return LOCTEXT("ShortDetect", "DETECT");
	case ERTSKeyAction::CycleFormation: return LOCTEXT("ShortForm", "FORM");
	case ERTSKeyAction::ShowHealthbars: return LOCTEXT("ShortHp", "HP");
	case ERTSKeyAction::ToggleOverview: return LOCTEXT("ShortTab", "VIEW");
	case ERTSKeyAction::OpenMenu: return LOCTEXT("ShortMenu", "MENU");
	case ERTSKeyAction::SelectGroupQ: return LOCTEXT("ShortGQ", "GRP Q");
	case ERTSKeyAction::SelectGroupW: return LOCTEXT("ShortGW", "GRP W");
	case ERTSKeyAction::SelectGroupE: return LOCTEXT("ShortGE", "GRP E");
	case ERTSKeyAction::SelectGroupR: return LOCTEXT("ShortGR", "GRP R");
	default: break;
	}
	if (Action >= ERTSKeyAction::Ability1 && Action <= ERTSKeyAction::Ability6)
	{
		return FText::Format(LOCTEXT("ShortAbility", "ABIL {0}"), Index - static_cast<int32>(ERTSKeyAction::Ability1) + 1);
	}
	if (Action >= ERTSKeyAction::SelectGroup1 && Action <= ERTSKeyAction::SelectGroup6)
	{
		const int32 N = Index - static_cast<int32>(ERTSKeyAction::SelectGroup1) + 1;
		return N <= 4 ? FText::Format(LOCTEXT("ShortTier", "T{0}"), N) : FText::Format(LOCTEXT("ShortGroup", "GRP {0}"), N);
	}
	if (Action >= ERTSKeyAction::SelectAltGroup1 && Action <= ERTSKeyAction::SelectAltGroup6)
	{
		return FText::Format(LOCTEXT("ShortAlt", "ALT {0}"), Index - static_cast<int32>(ERTSKeyAction::SelectAltGroup1) + 1);
	}
	if (Action >= ERTSKeyAction::FKeyGroup1 && Action <= ERTSKeyAction::FKeyGroup6)
	{
		return FText::Format(LOCTEXT("ShortF", "GRP F{0}"), Index - static_cast<int32>(ERTSKeyAction::FKeyGroup1) + 1);
	}
	return FText::GetEmpty();
}

FText URTSKeyBindingSubsystem::GetPresetDisplayName(ERTSKeyLayoutPreset Preset)
{
	switch (Preset)
	{
	case ERTSKeyLayoutPreset::Classic: return LOCTEXT("PresetClassic", "Classic");
	case ERTSKeyLayoutPreset::UserDefined: return LOCTEXT("PresetUser", "User defined");
	default: return LOCTEXT("PresetModern", "Modern");
	}
}

FText URTSKeyBindingSubsystem::GetPresetDescription(ERTSKeyLayoutPreset Preset)
{
	switch (Preset)
	{
	case ERTSKeyLayoutPreset::Classic:
		return LOCTEXT("PresetClassicDesc", "Abilities on Q W E A S D.\nUnit tiers T1-T4 on 1-4.\nCamera on the arrow keys,\nrotate with DELETE / PAGE DOWN.");
	case ERTSKeyLayoutPreset::UserDefined:
		return LOCTEXT("PresetUserDesc", "Put every function on the key you want.\nCTRL and ALT combos are possible.\nOpens the key list to edit your layout.");
	default:
		return LOCTEXT("PresetModernDesc", "Abilities on 1-6.\nUnit tiers T1-T4 on CTRL + 1-4.\nCamera on W A S D,\nrotate with Q / E.");
	}
}

FRTSKeyChord URTSKeyBindingSubsystem::TranslateModernChord(const FRTSKeyChord& Chord) const
{
	ERTSKeyAction Action = ERTSKeyAction::None;
	for (const FRTSKeyActionBinding& B : GetPresetBindings(ERTSKeyLayoutPreset::Modern))
	{
		if (B.Chord == Chord)
		{
			Action = B.Action;
			break;
		}
	}
	if (Action == ERTSKeyAction::None)
	{
		// Nicht Teil der Belegung (Maus, Escape ...) - bleibt, wie es ist.
		return Chord;
	}
	return GetChord(Action);
}

FText URTSKeyBindingSubsystem::LocalizeKeyHints(const FText& Text) const
{
	FString Source = Text.ToString();
	if (Source.IsEmpty())
	{
		return Text;
	}

	// 1) {Key:Ability1} - der Name eines ERTSKeyAction-Werts.
	if (Source.Contains(TEXT("{Key:")))
	{
		const UEnum* Enum = StaticEnum<ERTSKeyAction>();
		const FRegexPattern Placeholder(TEXT("\\{Key:([A-Za-z0-9]+)\\}"));
		FRegexMatcher Matcher(Placeholder, Source);
		FString Out;
		int32 Last = 0;
		while (Matcher.FindNext())
		{
			const int64 Value = Enum ? Enum->GetValueByNameString(Matcher.GetCaptureGroup(1)) : INDEX_NONE;
			const FText KeyText = Value != INDEX_NONE ? GetChordText(static_cast<ERTSKeyAction>(Value)) : FText::GetEmpty();
			Out += Source.Mid(Last, Matcher.GetMatchBeginning() - Last);
			Out += KeyText.IsEmpty() ? TEXT("(unassigned)") : KeyText.ToString();
			Last = Matcher.GetMatchEnding();
		}
		Out += Source.Mid(Last);
		Source = Out;
	}

	// 2) Tasten aus der Modern-Belegung im Fliesstext: "Ctrl+1", "Alt + 2", "press 1", "or 1".
	// Nur das Vorwort ohne Gross/Klein, die Taste selbst gross: sonst wuerde aus "or a worker" eine Taste A.
	const FRegexPattern Hint(TEXT("((?i:\\b(?:ctrl|strg|alt)\\s*\\+\\s*)|(?i:\\b(?:press|or|and|hold|tap)\\s+))([0-9]|[A-Z]|F[1-9]|F1[0-2]|Tab|TAB|Space|SPACE)\\b"));
	FRegexMatcher Matcher(Hint, Source);
	FString Out;
	int32 Last = 0;
	bool bChanged = false;
	while (Matcher.FindNext())
	{
		const FString Prefix = Matcher.GetCaptureGroup(1);
		const FString Token = Matcher.GetCaptureGroup(2);
		const FKey Key = RTSKeyBindingsLocal::KeyFromHintToken(Token);
		if (!Key.IsValid())
		{
			continue;
		}
		const FString PrefixLower = Prefix.ToLower();
		const bool bCtrl = PrefixLower.StartsWith(TEXT("ctrl")) || PrefixLower.StartsWith(TEXT("strg"));
		const bool bAlt = PrefixLower.StartsWith(TEXT("alt"));
		const FRTSKeyChord Modern(Key, bCtrl, bAlt);
		const FRTSKeyChord Now = TranslateModernChord(Modern);
		if (Now == Modern)
		{
			continue;
		}
		// Bei "press 1" bleibt "press " stehen, bei "Ctrl+1" wird der ganze Ausdruck ersetzt.
		const bool bModifierPrefix = bCtrl || bAlt;
		Out += Source.Mid(Last, Matcher.GetMatchBeginning() - Last);
		if (!bModifierPrefix)
		{
			Out += Prefix;
		}
		Out += Now.IsValid() ? Now.ToText().ToString() : TEXT("(unassigned)");
		Last = Matcher.GetMatchEnding();
		bChanged = true;
	}
	if (!bChanged && Source == Text.ToString())
	{
		return Text;
	}
	Out += Source.Mid(Last);
	return FText::FromString(Out);
}

void URTSKeyBindingSubsystem::ResetForTests()
{
	ActivePreset = ERTSKeyLayoutPreset::Modern;
	bLayoutChosen = false;
	UserBindings.Reset();
	Broadcast();
}

void URTSKeyBindingSubsystem::SaveToDisk()
{
	URTSKeyBindingsSave* Save = Cast<URTSKeyBindingsSave>(UGameplayStatics::CreateSaveGameObject(URTSKeyBindingsSave::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->Preset = ActivePreset;
	Save->bLayoutChosen = bLayoutChosen;
	Save->UserBindings = UserBindings;
	Save->CursorScale = CursorScale;
	UGameplayStatics::SaveGameToSlot(Save, URTSKeyBindingsSave::SlotName, 0);
}

void URTSKeyBindingSubsystem::SetCursorScale(float NewScale)
{
	NewScale = FMath::Clamp(NewScale, 0.75f, 3.f);
	if (FMath::IsNearlyEqual(NewScale, CursorScale))
	{
		return;
	}
	CursorScale = NewScale;
	SaveToDisk();
	if (const UGameInstance* GI = GetGameInstance())
	{
		for (ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (AControllerBase* PC = LP ? Cast<AControllerBase>(LP->GetPlayerController(GI->GetWorld())) : nullptr)
			{
				PC->ApplyCustomMouseCursor();
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Cursor] Cursor size factor %.2f."), CursorScale);
	Broadcast();
}

void URTSKeyBindingSubsystem::Broadcast()
{
	OnBindingsChanged.Broadcast();
}

FText URTSKeyBindingLibrary::GetKeyBindingText(const UObject* WorldContextObject, ERTSKeyAction Action)
{
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(WorldContextObject);
	return Bindings ? Bindings->GetChordText(Action) : FText::GetEmpty();
}

FText URTSKeyBindingLibrary::GetAbilityKeyText(const UObject* WorldContextObject, int32 Index)
{
	if (Index < 0 || Index > 5)
	{
		return FText::GetEmpty();
	}
	return GetKeyBindingText(WorldContextObject, static_cast<ERTSKeyAction>(static_cast<int32>(ERTSKeyAction::Ability1) + Index));
}

FText URTSKeyBindingLibrary::AppendAbilityKey(const UObject* WorldContextObject, FText Text, int32 Index)
{
	const FText KeyText = GetAbilityKeyText(WorldContextObject, Index);
	if (KeyText.IsEmpty())
	{
		return Text;
	}
	if (Text.IsEmpty())
	{
		return FText::Format(LOCTEXT("AbilityKeyOnly", "[{0}]"), KeyText);
	}
	return FText::Format(LOCTEXT("AbilityKeyAppended", "{0}   [{1}]"), Text, KeyText);
}

// ---------------------------------------------------------------------------------------------------------------
// Konsolenbefehle zum Testen (und fuer Spieler, die sich verkonfiguriert haben)
// ---------------------------------------------------------------------------------------------------------------

namespace RTSKeyBindingsConsole
{
	AExtendedCameraBase* LocalCamera(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AExtendedCameraBase>(PC->GetPawn()) : nullptr;
	}

	ERTSKeyAction ParseAction(const FString& Name)
	{
		const UEnum* Enum = StaticEnum<ERTSKeyAction>();
		const int64 Value = Enum ? Enum->GetValueByNameString(Name) : INDEX_NONE;
		return Value == INDEX_NONE ? ERTSKeyAction::None : static_cast<ERTSKeyAction>(Value);
	}

	static FAutoConsoleCommandWithWorldAndArgs Preset(TEXT("RTS.Keys.Preset"),
		TEXT("Key layout: 0 = Modern, 1 = Classic, 2 = User defined."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(World))
			{
				Bindings->SetActivePreset(static_cast<ERTSKeyLayoutPreset>(FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, 0, 2)));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs Dump(TEXT("RTS.Keys.Dump"),
		TEXT("Logs the active key layout."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(World);
			if (!Bindings)
			{
				return;
			}
			const UEnum* Enum = StaticEnum<ERTSKeyAction>();
			UE_LOG(LogTemp, Log, TEXT("[Tastatur] Layout %s, gewaehlt=%d"), *URTSKeyBindingSubsystem::GetPresetDisplayName(Bindings->GetActivePreset()).ToString(), Bindings->HasChosenLayout() ? 1 : 0);
			for (uint8 i = 1; i < static_cast<uint8>(ERTSKeyAction::Count); ++i)
			{
				const ERTSKeyAction Action = static_cast<ERTSKeyAction>(i);
				UE_LOG(LogTemp, Log, TEXT("[Tastatur]   %-18s %s"), *Enum->GetNameStringByValue(i), *Bindings->GetChordText(Action).ToString());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs Bind(TEXT("RTS.Keys.Bind"),
		TEXT("RTS.Keys.Bind <Action> <Key> [ctrl] [alt] - puts an action on a key in the user layout (e.g. RTS.Keys.Bind Ability1 F ctrl)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(World);
			if (!Bindings || Args.Num() < 2)
			{
				return;
			}
			const FRTSKeyChord Chord(FKey(*Args[1]), Args.Contains(TEXT("ctrl")), Args.Contains(TEXT("alt")));
			Bindings->SetUserBinding(ParseAction(Args[0]), Chord);
		}));

	static FAutoConsoleCommandWithWorldAndArgs ResetSave(TEXT("RTS.Keys.ResetSave"),
		TEXT("Deletes the saved key layout - the next start asks again (Modern until then)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameplayStatics::DeleteGameInSlot(URTSKeyBindingsSave::SlotName, 0);
			if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(World))
			{
				Bindings->ResetForTests();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs Simulate(TEXT("RTS.Keys.Simulate"),
		TEXT("RTS.Keys.Simulate <Key> [hold seconds] - presses a key through the key layout of the local camera."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AExtendedCameraBase* Camera = LocalCamera(World);
			if (!Camera || Args.Num() < 1)
			{
				return;
			}
			const FKey Key(*Args[0]);
			const bool bHandled = Camera->HandleBoundKey(Key, IE_Pressed);
			UE_LOG(LogTemp, Log, TEXT("[Tastatur] Simuliert %s -> %s"), *Key.ToString(), bHandled ? TEXT("Belegung") : TEXT("keine Funktion"));
			const float Hold = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
			TWeakObjectPtr<AExtendedCameraBase> WeakCamera(Camera);
			if (Hold <= 0.f)
			{
				Camera->HandleBoundKey(Key, IE_Released);
				return;
			}
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, [WeakCamera, Key]()
			{
				if (AExtendedCameraBase* C = WeakCamera.Get())
				{
					C->HandleBoundKey(Key, IE_Released);
				}
			}, Hold, false);
		}));

	static FAutoConsoleCommandWithWorldAndArgs Localize(TEXT("RTS.Keys.Localize"),
		TEXT("RTS.Keys.Localize <text> - logs a hint text with the keys of the active layout (as the tutorial shows it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(World);
			if (Bindings)
			{
				const FString Text = FString::Join(Args, TEXT(" "));
				UE_LOG(LogTemp, Log, TEXT("[Tastatur] '%s' -> '%s'"), *Text, *Bindings->LocalizeKeyHints(FText::FromString(Text)).ToString());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs OpenChoice(TEXT("RTS.Keys.OpenChoice"),
		TEXT("Opens the key layout choice (Modern / Classic / User defined)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AExtendedCameraBase* Camera = LocalCamera(World))
			{
				Camera->OpenKeyLayoutChoice();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs OpenKeyboard(TEXT("RTS.Keys.OpenKeyboard"),
		TEXT("Opens the keyboard window; with a key name also its rebind dialog (RTS.Keys.OpenKeyboard W)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AExtendedCameraBase* Camera = LocalCamera(World);
			if (!Camera)
			{
				return;
			}
			UControlWidget* Window = UControlWidget::OpenAsWindow(Cast<APlayerController>(Camera->GetController()), Camera->GetKeyboardWidgetClass(), nullptr);
			if (Window && Args.Num() > 0)
			{
				Window->OnKeyCapClicked(FKey(*Args[0]));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs OpenList(TEXT("RTS.Keys.OpenList"),
		TEXT("Opens the key list (user-defined layout); with a key name its rows are marked (RTS.Keys.OpenList Q)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AExtendedCameraBase* Camera = LocalCamera(World))
			{
				Camera->OpenKeyboardEditor(Args.Num() > 0 ? FKey(*Args[0]) : FKey());
			}
		}));
}

#undef LOCTEXT_NAMESPACE
