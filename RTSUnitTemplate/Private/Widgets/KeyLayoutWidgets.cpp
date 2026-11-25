// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/KeyLayoutWidgets.h"
#include "Widgets/ControlWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "RTSKeyLayout"

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

namespace RTSKeyUI
{
	FSlateBrush MakeBrush(UObject* Resource, const FLinearColor& FallbackColor, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		if (Resource)
		{
			Brush.SetResourceObject(Resource);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = FVector2D(32.f, 32.f);
			Brush.TintColor = FSlateColor(Tint);
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = FVector2D(32.f, 32.f);
			Brush.TintColor = FSlateColor(FallbackColor * Tint);
		}
		return Brush;
	}

	FSlateFontInfo MakeFont(const FSoftObjectPath& Path, int32 Size, const ANSICHAR* FallbackStyle)
	{
		if (UObject* FontObject = Path.IsNull() ? nullptr : Path.TryLoad())
		{
			return FSlateFontInfo(FontObject, Size);
		}
		return FCoreStyle::GetDefaultFontStyle(FallbackStyle, Size);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FRTSKeyboardStyle& Style, const FText& Text, int32 Size, const FLinearColor& Color,
		bool bHeading, int32 LetterSpacing)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		FSlateFontInfo Font = MakeFont(bHeading ? Style.HeadingFontPath : Style.BodyFontPath, Size, bHeading ? "Bold" : "Regular");
		Font.LetterSpacing = LetterSpacing;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetJustification(ETextJustify::Center);
		return Block;
	}

	void SetButtonTint(UButton* Button, const FRTSKeyboardStyle& Style, const FLinearColor& Tint)
	{
		if (!Button)
		{
			return;
		}
		FButtonStyle ButtonStyle = Button->GetStyle();
		ButtonStyle.Normal = MakeBrush(Style.ButtonNormal, FLinearColor(0.06f, 0.12f, 0.16f, 0.95f), Tint);
		ButtonStyle.Hovered = MakeBrush(Style.ButtonHover ? Style.ButtonHover.Get() : Style.ButtonNormal.Get(), FLinearColor(0.1f, 0.24f, 0.3f, 0.98f), Tint);
		ButtonStyle.Pressed = MakeBrush(Style.ButtonPressed ? Style.ButtonPressed.Get() : Style.ButtonNormal.Get(), FLinearColor(0.16f, 0.36f, 0.42f, 1.f), Tint);
		ButtonStyle.Disabled = MakeBrush(Style.ButtonNormal, FLinearColor(0.04f, 0.05f, 0.07f, 0.8f), FLinearColor(0.5f, 0.5f, 0.5f, 0.6f));
		ButtonStyle.NormalPadding = FMargin(0.f);
		ButtonStyle.PressedPadding = FMargin(0.f);
		Button->SetStyle(ButtonStyle);
	}

	UButton* MakeButton(UWidgetTree* Tree, const FRTSKeyboardStyle& Style, const FText& Label, int32 Size, UTextBlock** OutLabel, const FLinearColor& Tint)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		SetButtonTint(Button, Style, Tint);
		UTextBlock* Text = MakeText(Tree, Style, Label, Size, Style.TitleColor, true, 120);
		if (UButtonSlot* PanelSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			PanelSlot->SetPadding(FMargin(22.f, 10.f));
			PanelSlot->SetHorizontalAlignment(HAlign_Center);
			PanelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	}
}

namespace
{
	struct FKeyWindowEntry
	{
		TWeakObjectPtr<UUserWidget> Widget;
		bool bClosableWithEsc = true;
	};
	TArray<FKeyWindowEntry> OpenKeyWindows;

	void PruneKeyWindows()
	{
		OpenKeyWindows.RemoveAll([](const FKeyWindowEntry& E) { return !E.Widget.IsValid() || !E.Widget->IsInViewport(); });
	}
}

void URTSKeyCapProxy::HandleClicked()
{
	if (UControlWidget* Widget = Owner.Get())
	{
		Widget->OnKeyCapClicked(Key);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Key list (user-defined layout)
// ---------------------------------------------------------------------------------------------------------------

void URTSKeyRowProxy::HandleBind()
{
	if (URTSKeyBindingListWidget* List = Owner.Get())
	{
		List->BeginCapture(Action);
	}
}

void URTSKeyRowProxy::HandleClear()
{
	if (URTSKeyBindingListWidget* List = Owner.Get())
	{
		List->ClearAction(Action);
	}
}

namespace
{
	struct FKeyListSection
	{
		FText Title;
		TArray<ERTSKeyAction> Actions;
	};

	TArray<FKeyListSection> KeyListSections()
	{
		using A = ERTSKeyAction;
		return {
			{ LOCTEXT("SecCamera", "CAMERA"), { A::CameraForward, A::CameraBack, A::CameraLeft, A::CameraRight, A::CameraRotateLeft, A::CameraRotateRight,
				A::CameraZoomIn, A::CameraZoomOut, A::CameraJump, A::CameraLockUnit, A::CameraOrbit } },
			{ LOCTEXT("SecAbilities", "ABILITIES"), { A::Ability1, A::Ability2, A::Ability3, A::Ability4, A::Ability5, A::Ability6 } },
			{ LOCTEXT("SecSelection", "SELECT UNITS"), { A::SelectGroup1, A::SelectGroup2, A::SelectGroup3, A::SelectGroup4, A::SelectGroup5, A::SelectGroup6,
				A::SelectAltGroup1, A::SelectAltGroup2, A::SelectAltGroup3, A::SelectAltGroup4, A::SelectAltGroup5, A::SelectAltGroup6,
				A::SelectGroupQ, A::SelectGroupW, A::SelectGroupE, A::SelectGroupR,
				A::FKeyGroup1, A::FKeyGroup2, A::FKeyGroup3, A::FKeyGroup4, A::FKeyGroup5, A::FKeyGroup6 } },
			{ LOCTEXT("SecCommands", "COMMANDS"), { A::HoldPosition, A::DetectEnemies, A::CycleFormation, A::SpawnEffects } },
			{ LOCTEXT("SecInterface", "INTERFACE"), { A::ShowHealthbars, A::ToggleOverview, A::OpenMenu } },
		};
	}
}

URTSKeyBindingListWidget* URTSKeyBindingListWidget::Open(APlayerController* PC, const FRTSKeyboardStyle& InStyle, const FKey& InFocusKey, TFunction<void()> OnClosed)
{
	if (!PC || !PC->IsLocalController())
	{
		return nullptr;
	}
	URTSKeyBindingListWidget* Widget = CreateWidget<URTSKeyBindingListWidget>(PC, URTSKeyBindingListWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->Style = InStyle;
	Widget->FocusKey = InFocusKey;
	Widget->OnClosedCallback = MoveTemp(OnClosed);
	Widget->SetIsFocusable(true);
	Widget->Build();
	Widget->AddToViewport(96);
	URTSKeyLayoutChoiceWidget::RegisterWindow(Widget, true);
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(PC))
	{
		Bindings->OnBindingsChanged.AddUniqueDynamic(Widget, &URTSKeyBindingListWidget::OnBindingsChanged);
	}
	PC->SetShowMouseCursor(true);
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] Tastenliste offen."));
	return Widget;
}

void URTSKeyBindingListWidget::Build()
{
	if (!WidgetTree)
	{
		return;
	}
	Rows.Reset();
	RowProxies.Reset();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrush(RTSKeyUI::MakeBrush(nullptr, Style.BackdropColor));
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Dim))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(860.f);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Size))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrush(RTSKeyUI::MakeBrush(Style.Panel, FLinearColor(0.03f, 0.06f, 0.09f, 0.97f)));
	Panel->SetPadding(FMargin(56.f, 64.f, 56.f, 44.f));
	Size->AddChild(Panel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);
	auto AddRow = [Column](UWidget* W, float Bottom, EHorizontalAlignment H = HAlign_Center)
	{
		if (UVerticalBoxSlot* PanelSlot = Column->AddChildToVerticalBox(W))
		{
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			PanelSlot->SetHorizontalAlignment(H);
		}
	};

	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("ListTitle", "CONTROLS"), 30, Style.TitleColor, true, 400), 10.f);

	// Layout-Wahl wie auf der Tastatur
	UHorizontalBox* Presets = WidgetTree->ConstructWidget<UHorizontalBox>();
	{
		UTextBlock* Labels[3] = { nullptr, nullptr, nullptr };
		ModernButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListModern", "MODERN"), 13, &Labels[0]);
		ClassicButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListClassic", "CLASSIC"), 13, &Labels[1]);
		UserButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListUser", "USER DEFINED"), 13, &Labels[2]);
		ModernLabel = Labels[0];
		ClassicLabel = Labels[1];
		UserLabel = Labels[2];
		ResetModernButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListResetModern", "RESET TO MODERN"), 11);
		ResetClassicButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListResetClassic", "RESET TO CLASSIC"), 11);
		ModernButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::OnModernClicked);
		ClassicButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::OnClassicClicked);
		UserButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::OnUserClicked);
		ResetModernButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::OnResetModernClicked);
		ResetClassicButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::OnResetClassicClicked);
		for (UButton* B : { ModernButton.Get(), ClassicButton.Get(), UserButton.Get(), ResetModernButton.Get(), ResetClassicButton.Get() })
		{
			if (UHorizontalBoxSlot* PanelSlot = Presets->AddChildToHorizontalBox(B))
			{
				PanelSlot->SetPadding(FMargin(4.f, 0.f));
				PanelSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	AddRow(Presets, 8.f);

	HintText = RTSKeyUI::MakeText(WidgetTree, Style, FText::GetEmpty(), 14, Style.AccentColor);
	HintText->SetAutoWrapText(true);
	AddRow(HintText, 14.f, HAlign_Fill);

	// Die Liste
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>();
	ListBox->SetHeightOverride(470.f);
	List = WidgetTree->ConstructWidget<UScrollBox>();
	List->SetScrollBarVisibility(ESlateVisibility::Visible);
	ListBox->AddChild(List);
	AddRow(ListBox, 12.f, HAlign_Fill);

	for (const FKeyListSection& Section : KeyListSections())
	{
		UTextBlock* Header = RTSKeyUI::MakeText(WidgetTree, Style, Section.Title, 13, Style.AccentColor, true, 300);
		Header->SetJustification(ETextJustify::Left);
		if (UScrollBoxSlot* PanelSlot = Cast<UScrollBoxSlot>(List->AddChild(Header)))
		{
			PanelSlot->SetPadding(FMargin(4.f, 14.f, 0.f, 4.f));
		}
		USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>();
		LineBox->SetHeightOverride(1.f);
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
		Line->SetBrush(RTSKeyUI::MakeBrush(nullptr, Style.AccentColor * FLinearColor(1.f, 1.f, 1.f, 0.35f)));
		LineBox->AddChild(Line);
		if (UScrollBoxSlot* PanelSlot = Cast<UScrollBoxSlot>(List->AddChild(LineBox)))
		{
			PanelSlot->SetPadding(FMargin(4.f, 0.f, 18.f, 6.f));
		}

		for (const ERTSKeyAction Action : Section.Actions)
		{
			UHorizontalBox* Line2 = WidgetTree->ConstructWidget<UHorizontalBox>();
			UTextBlock* Name = RTSKeyUI::MakeText(WidgetTree, Style, URTSKeyBindingSubsystem::GetActionDisplayName(Action), 15, Style.TextColor);
			Name->SetJustification(ETextJustify::Left);
			if (UHorizontalBoxSlot* PanelSlot = Line2->AddChildToHorizontalBox(Name))
			{
				PanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				PanelSlot->SetVerticalAlignment(VAlign_Center);
				PanelSlot->SetPadding(FMargin(8.f, 0.f, 12.f, 0.f));
			}

			USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
			KeyBox->SetWidthOverride(230.f);
			KeyBox->SetHeightOverride(38.f);
			UTextBlock* KeyLabel = nullptr;
			UButton* KeyButton = RTSKeyUI::MakeButton(WidgetTree, Style, FText::GetEmpty(), 14, &KeyLabel);
			KeyBox->AddChild(KeyButton);
			if (UHorizontalBoxSlot* PanelSlot = Line2->AddChildToHorizontalBox(KeyBox))
			{
				PanelSlot->SetVerticalAlignment(VAlign_Center);
			}

			USizeBox* ClearBox = WidgetTree->ConstructWidget<USizeBox>();
			ClearBox->SetWidthOverride(38.f);
			ClearBox->SetHeightOverride(38.f);
			UButton* ClearButton = RTSKeyUI::MakeButton(WidgetTree, Style, FText::FromString(TEXT("X")), 12);
			ClearButton->SetToolTipText(LOCTEXT("ClearTip", "Remove the key of this function"));
			ClearBox->AddChild(ClearButton);
			if (UHorizontalBoxSlot* PanelSlot = Line2->AddChildToHorizontalBox(ClearBox))
			{
				PanelSlot->SetVerticalAlignment(VAlign_Center);
				PanelSlot->SetPadding(FMargin(6.f, 0.f, 18.f, 0.f));
			}

			URTSKeyRowProxy* Proxy = NewObject<URTSKeyRowProxy>(this);
			Proxy->Action = Action;
			Proxy->Owner = this;
			KeyButton->OnClicked.AddDynamic(Proxy, &URTSKeyRowProxy::HandleBind);
			ClearButton->OnClicked.AddDynamic(Proxy, &URTSKeyRowProxy::HandleClear);
			RowProxies.Add(Proxy);

			if (UScrollBoxSlot* PanelSlot = Cast<UScrollBoxSlot>(List->AddChild(Line2)))
			{
				PanelSlot->SetPadding(FMargin(0.f, 2.f));
			}

			FRow Row;
			Row.Action = Action;
			Row.KeyButton = KeyButton;
			Row.KeyLabel = KeyLabel;
			Row.ClearButton = ClearButton;
			Row.NameLabel = Name;
			Rows.Add(Row);
		}
	}

	StatusText = RTSKeyUI::MakeText(WidgetTree, Style, FText::GetEmpty(), 14, Style.DimColor);
	StatusText->SetAutoWrapText(true);
	AddRow(StatusText, 10.f, HAlign_Fill);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	ReplaceButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("Replace", "REPLACE"), 15, nullptr, Style.WarningColor);
	CancelButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListCancel", "CANCEL"), 15);
	UButton* DoneButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ListDone", "DONE"), 16);
	ReplaceButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::ConfirmReplace);
	CancelButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::CancelCapture);
	DoneButton->OnClicked.AddDynamic(this, &URTSKeyBindingListWidget::CloseList);
	for (UButton* B : { ReplaceButton.Get(), CancelButton.Get(), DoneButton })
	{
		if (UHorizontalBoxSlot* PanelSlot = Buttons->AddChildToHorizontalBox(B))
		{
			PanelSlot->SetPadding(FMargin(8.f, 0.f));
		}
	}
	AddRow(Buttons, 0.f);

	Refresh();
}

void URTSKeyBindingListWidget::SetStatus(const FText& Text, const FLinearColor& Color)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void URTSKeyBindingListWidget::Refresh()
{
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	if (!Bindings)
	{
		return;
	}
	const ERTSKeyLayoutPreset Active = Bindings->GetActivePreset();
	const bool bUser = Active == ERTSKeyLayoutPreset::UserDefined;

	UWidget* FirstFocused = nullptr;
	for (const FRow& Row : Rows)
	{
		const FRTSKeyChord Chord = Bindings->GetChord(Row.Action);
		const bool bCapturing = Row.Action == CaptureAction;
		const bool bFocused = FocusKey.IsValid() && Chord.Key == FocusKey;
		if (UTextBlock* Label = Row.KeyLabel.Get())
		{
			Label->SetText(bCapturing ? LOCTEXT("PressKey", "PRESS A KEY...") : (Chord.IsValid() ? Chord.ToText() : FText::FromString(TEXT("-"))));
			Label->SetColorAndOpacity(FSlateColor(bCapturing ? Style.WarningColor : (Chord.IsValid() ? Style.TitleColor : Style.DimColor)));
		}
		RTSKeyUI::SetButtonTint(Row.KeyButton.Get(), Style, bCapturing || bFocused ? FLinearColor(1.7f, 1.7f, 1.7f, 1.f) : FLinearColor::White);
		if (UTextBlock* Name = Row.NameLabel.Get())
		{
			Name->SetColorAndOpacity(FSlateColor(bCapturing || bFocused ? Style.AccentColor * FLinearColor(1.5f, 1.5f, 1.5f, 1.f) : Style.TextColor));
		}
		if (UButton* Clear = Row.ClearButton.Get())
		{
			Clear->SetVisibility(bUser && Chord.IsValid() && CaptureAction == ERTSKeyAction::None ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}
		if (bFocused && !FirstFocused)
		{
			FirstFocused = Row.KeyButton.Get() ? Row.KeyButton->GetParent() : nullptr;
		}
	}
	if (FirstFocused && List)
	{
		List->ScrollWidgetIntoView(FirstFocused, false, EDescendantScrollDestination::Center);
	}

	const ERTSKeyLayoutPreset Presets[3] = { ERTSKeyLayoutPreset::Modern, ERTSKeyLayoutPreset::Classic, ERTSKeyLayoutPreset::UserDefined };
	const TPair<UButton*, UTextBlock*> PresetButtons[3] = { { ModernButton, ModernLabel }, { ClassicButton, ClassicLabel }, { UserButton, UserLabel } };
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bActive = Active == Presets[i];
		RTSKeyUI::SetButtonTint(PresetButtons[i].Key, Style, bActive ? FLinearColor(1.6f, 1.6f, 1.6f, 1.f) : FLinearColor(0.65f, 0.65f, 0.65f, 0.9f));
		if (UTextBlock* Label = PresetButtons[i].Value)
		{
			Label->SetColorAndOpacity(FSlateColor(bActive ? Style.AccentColor * FLinearColor(1.5f, 1.5f, 1.5f, 1.f) : Style.DimColor));
		}
	}
	const ESlateVisibility ResetVisibility = bUser ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	ResetModernButton->SetVisibility(ResetVisibility);
	ResetClassicButton->SetVisibility(ResetVisibility);

	HintText->SetText(bUser
		? LOCTEXT("ListHintUser", "Your own layout: click the key of a function and press the new key. CTRL and ALT combos are possible.")
		: FText::Format(LOCTEXT("ListHintPreset", "{0} layout. Clicking a key copies it into your own layout (USER DEFINED), where every key can be changed - {0} stays as it is."),
			URTSKeyBindingSubsystem::GetPresetDisplayName(Active)));

	const bool bConfirm = PendingConflict != ERTSKeyAction::None;
	ReplaceButton->SetVisibility(bConfirm ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CancelButton->SetVisibility(CaptureAction != ERTSKeyAction::None ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (CaptureAction == ERTSKeyAction::None)
	{
		const TArray<ERTSKeyAction> Unbound = Bindings->GetUnboundActions();
		SetStatus(Unbound.Num() > 0
			? FText::Format(LOCTEXT("ListUnbound", "{0} function(s) without a key (shown as -)."), FText::AsNumber(Unbound.Num()))
			: FText::GetEmpty(), Style.DimColor);
	}
}

void URTSKeyBindingListWidget::OnBindingsChanged()
{
	Refresh();
}

void URTSKeyBindingListWidget::BeginCapture(ERTSKeyAction Action)
{
	URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	if (!Bindings || Action == ERTSKeyAction::None)
	{
		return;
	}
	if (Bindings->GetActivePreset() != ERTSKeyLayoutPreset::UserDefined)
	{
		// Das Preset bleibt, wie es ist; die eigene Belegung startet (falls neu) als Kopie davon.
		Bindings->SetActivePreset(ERTSKeyLayoutPreset::UserDefined);
	}
	CaptureAction = Action;
	PendingConflict = ERTSKeyAction::None;
	Refresh();
	for (const FRow& Row : Rows)
	{
		if (Row.Action == Action && Row.KeyButton.IsValid() && List)
		{
			List->ScrollWidgetIntoView(Row.KeyButton->GetParent(), true, EDescendantScrollDestination::Center);
			break;
		}
	}
	SetStatus(FText::Format(LOCTEXT("ListCapturing", "Press the new key for '{0}' (hold CTRL or ALT for a combo).  ESC cancels."),
		URTSKeyBindingSubsystem::GetActionDisplayName(Action)), Style.AccentColor);
	SetKeyboardFocus();
}

void URTSKeyBindingListWidget::SubmitKey(FKey Key, bool bCtrl, bool bAlt)
{
	HandleKey(Key, bCtrl, bAlt);
}

FReply URTSKeyBindingListWidget::HandleKey(const FKey& Key, bool bCtrl, bool bAlt)
{
	if (CaptureAction == ERTSKeyAction::None || PendingConflict != ERTSKeyAction::None)
	{
		return FReply::Unhandled();
	}
	if (Key == EKeys::Escape)
	{
		CancelCapture();
		return FReply::Handled();
	}
	if (Key.IsModifierKey())
	{
		return FReply::Handled();
	}
	if (!URTSKeyBindingSubsystem::CanBindKey(Key))
	{
		SetStatus(FText::Format(LOCTEXT("ListCannotBind", "{0} cannot be used - try another key.  ESC cancels."), URTSKeyBindingSubsystem::GetKeyText(Key)), Style.WarningColor);
		return FReply::Handled();
	}

	PendingChord = FRTSKeyChord(Key, bCtrl, bAlt);
	URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	const ERTSKeyAction Other = Bindings ? Bindings->FindUserConflict(CaptureAction, PendingChord) : ERTSKeyAction::None;
	if (Other != ERTSKeyAction::None)
	{
		PendingConflict = Other;
		Refresh();
		SetStatus(FText::Format(LOCTEXT("ListConflict", "{0} is already used by '{1}'. REPLACE removes it there - '{1}' then has no key."),
			PendingChord.ToText(), URTSKeyBindingSubsystem::GetActionDisplayName(Other)), Style.WarningColor);
		return FReply::Handled();
	}
	const ERTSKeyAction Action = CaptureAction;
	CaptureAction = ERTSKeyAction::None;
	if (Bindings)
	{
		Bindings->SetUserBinding(Action, PendingChord);
	}
	Refresh();
	return FReply::Handled();
}

void URTSKeyBindingListWidget::ConfirmReplace()
{
	const ERTSKeyAction Action = CaptureAction;
	CaptureAction = ERTSKeyAction::None;
	PendingConflict = ERTSKeyAction::None;
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
	{
		if (Action != ERTSKeyAction::None)
		{
			Bindings->SetUserBinding(Action, PendingChord);
		}
	}
	Refresh();
}

void URTSKeyBindingListWidget::CancelCapture()
{
	CaptureAction = ERTSKeyAction::None;
	PendingConflict = ERTSKeyAction::None;
	Refresh();
}

void URTSKeyBindingListWidget::ClearAction(ERTSKeyAction Action)
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
	{
		Bindings->ClearUserBinding(Action);
	}
}

void URTSKeyBindingListWidget::CloseList()
{
	URTSKeyLayoutChoiceWidget::UnregisterWindow(this);
	RemoveFromParent();
	if (OnClosedCallback)
	{
		TFunction<void()> Callback = MoveTemp(OnClosedCallback);
		OnClosedCallback = nullptr;
		Callback();
	}
}

void URTSKeyBindingListWidget::NativeDestruct()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
	{
		Bindings->OnBindingsChanged.RemoveDynamic(this, &URTSKeyBindingListWidget::OnBindingsChanged);
	}
	URTSKeyLayoutChoiceWidget::UnregisterWindow(this);
	Super::NativeDestruct();
}

FReply URTSKeyBindingListWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CaptureAction != ERTSKeyAction::None)
	{
		if (PendingConflict != ERTSKeyAction::None && InKeyEvent.GetKey() == EKeys::Escape)
		{
			CancelCapture();
			return FReply::Handled();
		}
		const FReply Reply = HandleKey(InKeyEvent.GetKey(), InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown());
		return Reply.IsEventHandled() ? Reply : FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseList();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply URTSKeyBindingListWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CaptureAction != ERTSKeyAction::None)
	{
		return HandleKey(InKeyEvent.GetKey(), InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown());
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URTSKeyBindingListWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CaptureAction != ERTSKeyAction::None && PendingConflict == ERTSKeyAction::None)
	{
		const FKey Button = InMouseEvent.GetEffectingButton();
		if (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton)
		{
			return HandleKey(Button, InMouseEvent.IsControlDown(), InMouseEvent.IsAltDown());
		}
	}
	// Klicks auf den abgedunkelten Rand gehen nicht ins Spiel.
	return FReply::Handled();
}

void URTSKeyBindingListWidget::OnModernClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::Modern); }
}

void URTSKeyBindingListWidget::OnClassicClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::Classic); }
}

void URTSKeyBindingListWidget::OnUserClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::UserDefined); }
}

void URTSKeyBindingListWidget::OnResetModernClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->ResetUserLayout(ERTSKeyLayoutPreset::Modern); }
}

void URTSKeyBindingListWidget::OnResetClassicClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->ResetUserLayout(ERTSKeyLayoutPreset::Classic); }
}

// ---------------------------------------------------------------------------------------------------------------
// Layout choice
// ---------------------------------------------------------------------------------------------------------------

void URTSKeyLayoutChoiceWidget::RegisterWindow(UUserWidget* Window, bool bClosableWithEsc)
{
	PruneKeyWindows();
	FKeyWindowEntry Entry;
	Entry.Widget = Window;
	Entry.bClosableWithEsc = bClosableWithEsc;
	OpenKeyWindows.Add(Entry);
}

void URTSKeyLayoutChoiceWidget::UnregisterWindow(UUserWidget* Window)
{
	OpenKeyWindows.RemoveAll([Window](const FKeyWindowEntry& E) { return !E.Widget.IsValid() || E.Widget.Get() == Window; });
}

bool URTSKeyLayoutChoiceWidget::IsAnyWindowOpen(const UWorld* World)
{
	PruneKeyWindows();
	return OpenKeyWindows.ContainsByPredicate([World](const FKeyWindowEntry& E) { return E.Widget->GetWorld() == World; });
}

bool URTSKeyLayoutChoiceWidget::CloseTopmostWindow(const UWorld* World)
{
	PruneKeyWindows();
	for (int32 i = OpenKeyWindows.Num() - 1; i >= 0; --i)
	{
		UUserWidget* Window = OpenKeyWindows[i].Widget.Get();
		if (!Window || Window->GetWorld() != World || !Window->IsVisible())
		{
			continue;
		}
		if (!OpenKeyWindows[i].bClosableWithEsc)
		{
			// Erste Wahl beim Start: Esc schliesst sie nicht, oeffnet aber auch nicht das Menue darunter.
			return true;
		}
		if (URTSKeyBindingListWidget* KeyList = Cast<URTSKeyBindingListWidget>(Window))
		{
			if (KeyList->IsCapturing())
			{
				KeyList->CancelCapture();
			}
			else
			{
				KeyList->CloseList();
			}
		}
		else if (UControlWidget* Keyboard = Cast<UControlWidget>(Window))
		{
			Keyboard->CloseWindow();
		}
		else if (URTSKeyLayoutChoiceWidget* Choice = Cast<URTSKeyLayoutChoiceWidget>(Window))
		{
			Choice->Finish();
		}
		else
		{
			Window->RemoveFromParent();
		}
		return true;
	}
	return false;
}

URTSKeyLayoutChoiceWidget* URTSKeyLayoutChoiceWidget::Open(APlayerController* PC, TSubclassOf<UControlWidget> InKeyboardClass, UMaterialInterface* CardMaterial,
	bool bInFirstRun, TFunction<void()> OnFinished)
{
	if (!PC || !PC->IsLocalController())
	{
		return nullptr;
	}
	URTSKeyLayoutChoiceWidget* Widget = CreateWidget<URTSKeyLayoutChoiceWidget>(PC, URTSKeyLayoutChoiceWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->KeyboardClass = UControlWidget::ResolveKeyboardClass(InKeyboardClass);
	// Same look as the Esc menu keyboard (the HUD instance); the class default only without a HUD.
	if (const UControlWidget* GameKeyboard = UControlWidget::FindGameKeyboard(PC))
	{
		Widget->Style = GameKeyboard->Style;
	}
	else if (const UControlWidget* CDO = Widget->KeyboardClass ? Widget->KeyboardClass->GetDefaultObject<UControlWidget>() : nullptr)
	{
		Widget->Style = CDO->Style;
	}
	Widget->bFirstRun = bInFirstRun;
	Widget->OnFinishedCallback = MoveTemp(OnFinished);
	Widget->Build(CardMaterial);
	Widget->AddToViewport(90);
	RegisterWindow(Widget, !bInFirstRun);
	PC->SetShowMouseCursor(true);
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] Layoutwahl offen (erste Wahl=%d)."), bInFirstRun ? 1 : 0);
	return Widget;
}

void URTSKeyLayoutChoiceWidget::Build(UMaterialInterface* CardMaterial)
{
	if (!WidgetTree)
	{
		return;
	}
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	const ERTSKeyLayoutPreset Active = Bindings ? Bindings->GetActivePreset() : ERTSKeyLayoutPreset::Modern;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrush(RTSKeyUI::MakeBrush(nullptr, Style.BackdropColor));
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Dim))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Column))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}
	auto AddRow = [Column](UWidget* W, float Bottom)
	{
		if (UVerticalBoxSlot* PanelSlot = Column->AddChildToVerticalBox(W))
		{
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			PanelSlot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, bFirstRun ? LOCTEXT("ChoiceTitleFirst", "CHOOSE YOUR CONTROLS") : LOCTEXT("ChoiceTitle", "KEYBOARD LAYOUT"),
		36, Style.TitleColor, true, 160), 6.f);
	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("ChoiceSub", "You can change this any time: ESC  >  KEYBOARD LAYOUT."), 15, Style.DimColor), 30.f);

	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>();
	const ERTSKeyLayoutPreset Presets[3] = { ERTSKeyLayoutPreset::Modern, ERTSKeyLayoutPreset::Classic, ERTSKeyLayoutPreset::UserDefined };
	for (int32 i = 0; i < 3; ++i)
	{
		const ERTSKeyLayoutPreset Preset = Presets[i];
		const bool bCurrent = !bFirstRun && Preset == Active;

		USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();
		CardBox->SetWidthOverride(330.f);
		CardBox->SetMinDesiredHeight(330.f);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrush(RTSKeyUI::MakeBrush(nullptr, bCurrent ? Style.AccentColor : FLinearColor(0.2f, 0.3f, 0.38f, 0.6f)));
		Frame->SetPadding(FMargin(bCurrent ? 3.f : 1.f));
		CardBox->AddChild(Frame);

		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UObject* CardResource = CardMaterial ? static_cast<UObject*>(CardMaterial) : Style.Panel.Get();
		FButtonStyle ButtonStyle = Button->GetStyle();
		ButtonStyle.Normal = RTSKeyUI::MakeBrush(CardResource, FLinearColor(0.04f, 0.07f, 0.1f, 0.95f), FLinearColor(0.8f, 0.8f, 0.8f, 1.f));
		ButtonStyle.Hovered = RTSKeyUI::MakeBrush(CardResource, FLinearColor(0.07f, 0.13f, 0.17f, 0.97f), FLinearColor(1.25f, 1.25f, 1.25f, 1.f));
		ButtonStyle.Pressed = RTSKeyUI::MakeBrush(CardResource, FLinearColor(0.03f, 0.05f, 0.07f, 1.f), FLinearColor(0.6f, 0.6f, 0.6f, 1.f));
		ButtonStyle.NormalPadding = FMargin(0.f);
		ButtonStyle.PressedPadding = FMargin(0.f);
		Button->SetStyle(ButtonStyle);
		Frame->SetContent(Button);
		switch (Preset)
		{
		case ERTSKeyLayoutPreset::Classic: Button->OnClicked.AddDynamic(this, &URTSKeyLayoutChoiceWidget::OnClassicClicked); break;
		case ERTSKeyLayoutPreset::UserDefined: Button->OnClicked.AddDynamic(this, &URTSKeyLayoutChoiceWidget::OnUserClicked); break;
		default: Button->OnClicked.AddDynamic(this, &URTSKeyLayoutChoiceWidget::OnModernClicked); break;
		}

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UButtonSlot* PanelSlot = Cast<UButtonSlot>(Button->AddChild(Content)))
		{
			PanelSlot->SetPadding(FMargin(30.f, 34.f, 30.f, 30.f));
			PanelSlot->SetHorizontalAlignment(HAlign_Fill);
			PanelSlot->SetVerticalAlignment(VAlign_Fill);
		}
		auto Add = [Content](UWidget* W, float Bottom)
		{
			if (UVerticalBoxSlot* PanelSlot = Content->AddChildToVerticalBox(W))
			{
				PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
				PanelSlot->SetHorizontalAlignment(HAlign_Center);
			}
		};
		Add(RTSKeyUI::MakeText(WidgetTree, Style, FText::Format(LOCTEXT("LayoutN", "LAYOUT {0}"), FText::AsNumber(i + 1)), 12, Style.DimColor, false, 300), 12.f);
		Add(RTSKeyUI::MakeText(WidgetTree, Style, URTSKeyBindingSubsystem::GetPresetDisplayName(Preset).ToUpper(), 28, Style.AccentColor, true, 90), 14.f);

		USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>();
		LineBox->SetWidthOverride(200.f);
		LineBox->SetHeightOverride(2.f);
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
		Line->SetBrush(RTSKeyUI::MakeBrush(nullptr, Style.AccentColor * FLinearColor(1.f, 1.f, 1.f, 0.6f)));
		LineBox->AddChild(Line);
		Add(LineBox, 16.f);

		USizeBox* DescBox = WidgetTree->ConstructWidget<USizeBox>();
		DescBox->SetWidthOverride(270.f);
		UTextBlock* Desc = RTSKeyUI::MakeText(WidgetTree, Style, URTSKeyBindingSubsystem::GetPresetDescription(Preset), 15, Style.TextColor);
		Desc->SetAutoWrapText(true);
		DescBox->AddChild(Desc);
		Add(DescBox, 16.f);

		if (bCurrent)
		{
			Add(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("Current", "CURRENT"), 12, Style.AccentColor, true, 300), 0.f);
		}

		if (UHorizontalBoxSlot* PanelSlot = Cards->AddChildToHorizontalBox(CardBox))
		{
			PanelSlot->SetPadding(FMargin(14.f, 0.f));
		}
	}
	AddRow(Cards, 26.f);

	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("KeepUser", "Your own layout stays saved when you pick Modern or Classic."), 14, Style.DimColor), 18.f);

	if (!bFirstRun)
	{
		UButton* CloseButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("Close", "CLOSE"), 15);
		CloseButton->OnClicked.AddDynamic(this, &URTSKeyLayoutChoiceWidget::OnCloseClicked);
		AddRow(CloseButton, 0.f);
	}
}

void URTSKeyLayoutChoiceWidget::OnModernClicked() { Choose(ERTSKeyLayoutPreset::Modern); }
void URTSKeyLayoutChoiceWidget::OnClassicClicked() { Choose(ERTSKeyLayoutPreset::Classic); }
void URTSKeyLayoutChoiceWidget::OnUserClicked() { Choose(ERTSKeyLayoutPreset::UserDefined); }
void URTSKeyLayoutChoiceWidget::OnCloseClicked() { Finish(); }

void URTSKeyLayoutChoiceWidget::Choose(ERTSKeyLayoutPreset Preset)
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
	{
		Bindings->SetActivePreset(Preset);
	}
	if (Preset != ERTSKeyLayoutPreset::UserDefined)
	{
		Finish();
		return;
	}

	// Eigene Belegung: die Tastenliste oeffnen; diese Wahl ist erst fertig, wenn sie zu ist.
	SetVisibility(ESlateVisibility::Collapsed);
	TWeakObjectPtr<URTSKeyLayoutChoiceWidget> WeakThis(this);
	if (!URTSKeyBindingListWidget::Open(GetOwningPlayer(), Style, FKey(), [WeakThis]()
		{
			if (URTSKeyLayoutChoiceWidget* Self = WeakThis.Get())
			{
				Self->Finish();
			}
		}))
	{
		Finish();
	}
}

void URTSKeyLayoutChoiceWidget::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	UnregisterWindow(this);
	RemoveFromParent();
	UE_LOG(LogTemp, Log, TEXT("[Tastatur] Layoutwahl geschlossen."));
	if (OnFinishedCallback)
	{
		TFunction<void()> Callback = MoveTemp(OnFinishedCallback);
		OnFinishedCallback = nullptr;
		Callback();
	}
}

void URTSKeyLayoutChoiceWidget::NativeDestruct()
{
	UnregisterWindow(this);
	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
