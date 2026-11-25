// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/ControlWidget.h"
#include "Components/CheckBox.h"
#include "Characters/Camera/CameraBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "RTSControlWidget"

void UControlWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Every other keyboard (loading screen, windows) looks like the one in the Esc menu: take over the
	// style of the HUD's control widget, before the keyboard is built.
	if (const UControlWidget* GameKeyboard = FindGameKeyboard(GetOwningPlayer()))
	{
		if (GameKeyboard != this)
		{
			Style = GameKeyboard->Style;
			KeySize = GameKeyboard->KeySize;
			KeyTooltipClass = GameKeyboard->KeyTooltipClass;
		}
	}
	// Ein Blueprint mit eigenem Designer-Layout behaelt es (Lux, aeltere Projekte).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildKeyboard();
	}
}

void UControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SwapScrollCheckBox)
	{
		SwapScrollCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UControlWidget::OnSwapScrollChanged);

		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
			if (Camera)
			{
				SwapScrollCheckBox->SetIsChecked(Camera->SwapScroll);
			}
		}
	}

	if (SwapAttackMoveCheckBox)
	{
		SwapAttackMoveCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UControlWidget::OnSwapAttackMoveChanged);

		ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer());
		if (PC)
		{
			SwapAttackMoveCheckBox->SetIsChecked(PC->SwapAttackMove);
		}
	}

	if (InvertCameraAngleCheckBox)
	{
		InvertCameraAngleCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UControlWidget::OnInvertCameraAngleChanged);

		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
			if (Camera)
			{
				InvertCameraAngleCheckBox->SetIsChecked(Camera->bInvertCameraAnglePitch);
			}
		}
	}

	if (bBuiltKeyboard)
	{
		if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
		{
			Bindings->OnBindingsChanged.AddUniqueDynamic(this, &UControlWidget::OnBindingsChanged);
		}
		RefreshKeys();
	}
}

void UControlWidget::NativeDestruct()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this))
	{
		Bindings->OnBindingsChanged.RemoveDynamic(this, &UControlWidget::OnBindingsChanged);
	}
	if (bShowAsWindow)
	{
		URTSKeyLayoutChoiceWidget::UnregisterWindow(this);
	}
	Super::NativeDestruct();
}

void UControlWidget::OnSwapScrollChanged(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
		if (Camera)
		{
			Camera->SwapScroll = bIsChecked;
		}
	}
}

void UControlWidget::OnSwapAttackMoveChanged(bool bIsChecked)
{
	ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->SwapAttackMove = bIsChecked;
	}
}

void UControlWidget::OnInvertCameraAngleChanged(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
		if (Camera)
		{
			Camera->bInvertCameraAnglePitch = bIsChecked;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard built in code
// ---------------------------------------------------------------------------------------------------------------

TSubclassOf<UControlWidget> UControlWidget::ResolveKeyboardClass(UClass* HUDClass)
{
	if (HUDClass && HUDClass->IsChildOf(UControlWidget::StaticClass()))
	{
		// Nur eine Klasse ohne eigenes Designer-Layout baut die Tastatur - sonst die nackte C++-Klasse.
		const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(HUDClass);
		const UWidgetTree* Archetype = WidgetClass ? WidgetClass->GetWidgetTreeArchetype() : nullptr;
		if (!Archetype || !Archetype->RootWidget)
		{
			return HUDClass;
		}
	}
	return UControlWidget::StaticClass();
}

UControlWidget* UControlWidget::OpenAsWindow(APlayerController* PC, TSubclassOf<UControlWidget> WidgetClass, TFunction<void()> OnClosed)
{
	if (!PC || !PC->IsLocalController())
	{
		return nullptr;
	}
	TSubclassOf<UControlWidget> Class = ResolveKeyboardClass(WidgetClass);
	UControlWidget* Widget = CreateWidget<UControlWidget>(PC, Class, NAME_None);
	if (!Widget)
	{
		return nullptr;
	}
	// CreateWidget hat die Tastatur schon ohne Fensterteile gebaut - mit DONE-Knopf und Hintergrund
	// neu aufbauen (noch kein Slate-Widget, also ohne Folgen).
	Widget->bShowAsWindow = true;
	if (Widget->bBuiltKeyboard && Widget->WidgetTree)
	{
		Widget->WidgetTree->RootWidget = nullptr;
		Widget->BuildKeyboard();
	}
	Widget->OnClosedCallback = MoveTemp(OnClosed);
	Widget->AddToViewport(95);
	URTSKeyLayoutChoiceWidget::RegisterWindow(Widget, true);
	PC->SetShowMouseCursor(true);
	return Widget;
}

void UControlWidget::CloseWindow()
{
	OnDone.Broadcast();
	if (!bShowAsWindow)
	{
		return;
	}
	URTSKeyLayoutChoiceWidget::UnregisterWindow(this);
	RemoveFromParent();
	if (OnClosedCallback)
	{
		TFunction<void()> Callback = MoveTemp(OnClosedCallback);
		OnClosedCallback = nullptr;
		Callback();
	}
}

void UControlWidget::OnDoneClicked()
{
	CloseWindow();
}

namespace
{
	FText KeyCapLabel(const FKey& Key)
	{
		static const TMap<FKey, FString> Labels = {
			{ EKeys::Escape, TEXT("ESC") }, { EKeys::Tab, TEXT("TAB") }, { EKeys::CapsLock, TEXT("CAPS") },
			{ EKeys::LeftShift, TEXT("SHIFT") }, { EKeys::LeftControl, TEXT("CTRL") }, { EKeys::LeftAlt, TEXT("ALT") },
			{ EKeys::SpaceBar, TEXT("SPACE") }, { EKeys::BackSpace, TEXT("BACK") }, { EKeys::Enter, TEXT("ENTER") },
			{ EKeys::Insert, TEXT("INS") }, { EKeys::Home, TEXT("HOME") }, { EKeys::PageUp, TEXT("PG UP") },
			{ EKeys::Delete, TEXT("DEL") }, { EKeys::End, TEXT("END") }, { EKeys::PageDown, TEXT("PG DN") },
			{ EKeys::Up, TEXT("UP") }, { EKeys::Down, TEXT("DOWN") }, { EKeys::Left, TEXT("LEFT") }, { EKeys::Right, TEXT("RIGHT") },
			{ EKeys::Comma, TEXT(",") }, { EKeys::Period, TEXT(".") },
		};
		if (const FString* Found = Labels.Find(Key))
		{
			return FText::FromString(*Found);
		}
		return URTSKeyBindingSubsystem::GetKeyText(Key);
	}
}

UWidget* UControlWidget::MakeGap(float Units)
{
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	Spacer->SetSize(FVector2D(KeySize * Units + 6.f * (Units - 1.f) + 6.f, KeySize + 6.f));
	return Spacer;
}

UWidget* UControlWidget::MakeKey(const FKey& Key, const FText& Label, float Units, bool bFixed, const FText& FixedTooltip)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(KeySize * Units + 6.f * (Units - 1.f));
	Box->SetHeightOverride(KeySize);

	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Box->AddChild(Button);

	UVerticalBox* Caps = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UButtonSlot* PanelSlot = Cast<UButtonSlot>(Button->AddChild(Caps)))
	{
		PanelSlot->SetPadding(FMargin(2.f));
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}
	const FText Shown = Label.IsEmpty() ? KeyCapLabel(Key) : Label;
	const int32 Len = Shown.ToString().Len();
	UTextBlock* KeyText = RTSKeyUI::MakeText(WidgetTree, Style, Shown, Len <= 2 ? 18 : (Len <= 4 ? 13 : 11), Style.AccentColor, true);
	Caps->AddChildToVerticalBox(KeyText)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* ActionText = RTSKeyUI::MakeText(WidgetTree, Style, FText::GetEmpty(), 9, Style.TitleColor, false, 10);
	Caps->AddChildToVerticalBox(ActionText)->SetHorizontalAlignment(HAlign_Center);

	FKeyCap Cap;
	Cap.Key = Key;
	Cap.Button = Button;
	Cap.ActionLabel = ActionText;
	Cap.bFixed = bFixed;
	Cap.FixedTooltip = FixedTooltip;
	KeyCaps.Add(Cap);

	if (!bFixed)
	{
		URTSKeyCapProxy* Proxy = NewObject<URTSKeyCapProxy>(this);
		Proxy->Key = Key;
		Proxy->Owner = this;
		Button->OnClicked.AddDynamic(Proxy, &URTSKeyCapProxy::HandleClicked);
		KeyProxies.Add(Proxy);
	}

	return Box;
}

UWidget* UControlWidget::MakeMouseEntry(const FText& Label, const FText& Tooltip)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(190.f);
	Box->SetHeightOverride(KeySize);
	UButton* Button = RTSKeyUI::MakeButton(WidgetTree, Style, Label, 13);
	ApplyTooltip(Button, Label, LOCTEXT("MouseType", "Mouse"), Tooltip);
	Box->AddChild(Button);
	if (Label.EqualTo(LOCTEXT("LeftClick", "LEFT CLICK")))
	{
		LeftClickMouseButton = Button;
	}
	return Box;
}

void UControlWidget::BuildKeyboard()
{
	bBuiltKeyboard = true;
	KeyCaps.Reset();
	KeyProxies.Reset();

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = RootOverlay;

	if (bShowAsWindow)
	{
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
		Dim->SetBrush(RTSKeyUI::MakeBrush(nullptr, Style.BackdropColor));
		if (UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(Dim))
		{
			PanelSlot->SetHorizontalAlignment(HAlign_Fill);
			PanelSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrush(RTSKeyUI::MakeBrush(Style.Panel, FLinearColor(0.03f, 0.06f, 0.09f, 0.95f)));
	Panel->SetPadding(FMargin(60.f, 70.f, 60.f, 50.f));
	if (UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(Panel))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

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

	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("Title", "CONTROLS"), 30, Style.TitleColor, true, 400), 10.f);

	// Cursor size (on top of the automatic resolution scale - a 4K screen already gets twice the size).
	UHorizontalBox* CursorRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	{
		UTextBlock* CursorLabel = RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("CursorSize", "CURSOR SIZE"), 13, Style.DimColor, true, 300);
		if (UHorizontalBoxSlot* PanelSlot = CursorRow->AddChildToHorizontalBox(CursorLabel))
		{
			PanelSlot->SetVerticalAlignment(VAlign_Center);
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		CursorSizeButtons.Reset();
		CursorSizeLabels.Reset();
		const FText Names[4] = { LOCTEXT("Cursor100", "100%"), LOCTEXT("Cursor125", "125%"), LOCTEXT("Cursor150", "150%"), LOCTEXT("Cursor200", "200%") };
		for (int32 i = 0; i < 4; ++i)
		{
			UTextBlock* Label = nullptr;
			UButton* B = RTSKeyUI::MakeButton(WidgetTree, Style, Names[i], 13, &Label);
			B->SetToolTipText(LOCTEXT("CursorSizeTip", "Mouse cursor size. It already grows with the screen resolution; this makes it larger still."));
			CursorSizeButtons.Add(B);
			CursorSizeLabels.Add(Label);
			if (UHorizontalBoxSlot* PanelSlot = CursorRow->AddChildToHorizontalBox(B))
			{
				PanelSlot->SetPadding(FMargin(4.f, 0.f));
				PanelSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		CursorSizeButtons[0]->OnClicked.AddDynamic(this, &UControlWidget::OnCursorSize100Clicked);
		CursorSizeButtons[1]->OnClicked.AddDynamic(this, &UControlWidget::OnCursorSize125Clicked);
		CursorSizeButtons[2]->OnClicked.AddDynamic(this, &UControlWidget::OnCursorSize150Clicked);
		CursorSizeButtons[3]->OnClicked.AddDynamic(this, &UControlWidget::OnCursorSize200Clicked);
	}
	AddRow(CursorRow, 12.f);

	// Layout-Wahl
	UHorizontalBox* Presets = WidgetTree->ConstructWidget<UHorizontalBox>();
	{
		UTextBlock* LayoutLabel = RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("Layout", "LAYOUT"), 13, Style.DimColor, true, 300);
		if (UHorizontalBoxSlot* PanelSlot = Presets->AddChildToHorizontalBox(LayoutLabel))
		{
			PanelSlot->SetVerticalAlignment(VAlign_Center);
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		UTextBlock* Labels[3] = { nullptr, nullptr, nullptr };
		ModernButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("Modern", "MODERN"), 13, &Labels[0]);
		ClassicButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("Classic", "CLASSIC"), 13, &Labels[1]);
		UserButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("User", "USER DEFINED"), 13, &Labels[2]);
		ModernLabel = Labels[0];
		ClassicLabel = Labels[1];
		UserLabel = Labels[2];
		ResetModernButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ResetModern", "RESET TO MODERN"), 11);
		ResetClassicButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("ResetClassic", "RESET TO CLASSIC"), 11);
		ModernButton->OnClicked.AddDynamic(this, &UControlWidget::OnModernClicked);
		ClassicButton->OnClicked.AddDynamic(this, &UControlWidget::OnClassicClicked);
		UserButton->OnClicked.AddDynamic(this, &UControlWidget::OnUserClicked);
		ResetModernButton->OnClicked.AddDynamic(this, &UControlWidget::OnResetModernClicked);
		ResetClassicButton->OnClicked.AddDynamic(this, &UControlWidget::OnResetClassicClicked);
		ResetModernButton->SetToolTipText(LOCTEXT("ResetModernTip", "Replaces your own layout with a copy of Modern."));
		ResetClassicButton->SetToolTipText(LOCTEXT("ResetClassicTip", "Replaces your own layout with a copy of Classic."));
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

	LayoutHintText = RTSKeyUI::MakeText(WidgetTree, Style, FText::GetEmpty(), 15, Style.AccentColor);
	AddRow(LayoutHintText, 16.f);

	// Tastatur, Navigationsblock, Maus
	UHorizontalBox* Main = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* Keys = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
	auto NewRow = [this](UVerticalBox* Parent) -> UHorizontalBox*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* PanelSlot = Parent->AddChildToVerticalBox(Row))
		{
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		}
		return Row;
	};
	auto Put = [](UHorizontalBox* Row, UWidget* W)
	{
		if (UHorizontalBoxSlot* PanelSlot = Row->AddChildToHorizontalBox(W))
		{
			PanelSlot->SetPadding(FMargin(3.f));
		}
	};
	auto PutGap = [this](UHorizontalBox* Row, float Units)
	{
		Row->AddChildToHorizontalBox(MakeGap(Units));
	};

	const FText ShiftTip = LOCTEXT("ShiftTip", "Fixed modifier key:\nSHIFT + Right click  -  Move units along waypoints\nSHIFT + ability  -  Queue it\n(SHIFT is never part of a key combo.)");
	const FText CtrlTip = LOCTEXT("CtrlTip", "Fixed modifier key for CTRL combos (e.g. CTRL + 1).\nCTRL + Scroll  -  Zoom camera");
	const FText AltTip = LOCTEXT("AltTip", "Fixed modifier key for ALT combos (e.g. ALT + 1).\nALT + Scroll  -  Zoom camera");
	const FText EscTip = LOCTEXT("EscTip", "Fixed:\nOpen / close the menu\nCancel the running ability, then deselect");

	{
		UHorizontalBox* Row = NewRow(Keys);
		Put(Row, MakeKey(EKeys::Escape, FText(), 1.f, true, EscTip));
		PutGap(Row, 0.2f);
		const FKey FKeys[12] = { EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 };
		for (int32 i = 0; i < 12; ++i)
		{
			if (i == 4 || i == 8)
			{
				PutGap(Row, 0.15f);
			}
			Put(Row, MakeKey(FKeys[i], FText(), 0.85f));
		}
	}
	{
		UHorizontalBox* Row = NewRow(Keys);
		const FKey Digits[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
		for (const FKey& K : Digits)
		{
			Put(Row, MakeKey(K, FText(), 1.f));
		}
		Put(Row, MakeKey(EKeys::BackSpace, FText(), 1.6f));
	}
	{
		UHorizontalBox* Row = NewRow(Keys);
		Put(Row, MakeKey(EKeys::Tab, FText(), 1.5f));
		for (const FKey& K : { EKeys::Q, EKeys::W, EKeys::E, EKeys::R, EKeys::T, EKeys::Z, EKeys::U, EKeys::I, EKeys::O, EKeys::P })
		{
			Put(Row, MakeKey(K, FText(), 1.f));
		}
	}
	{
		UHorizontalBox* Row = NewRow(Keys);
		Put(Row, MakeKey(EKeys::CapsLock, FText(), 1.75f));
		for (const FKey& K : { EKeys::A, EKeys::S, EKeys::D, EKeys::F, EKeys::G, EKeys::H, EKeys::J, EKeys::K, EKeys::L })
		{
			Put(Row, MakeKey(K, FText(), 1.f));
		}
		Put(Row, MakeKey(EKeys::Enter, FText(), 1.35f));
	}
	{
		UHorizontalBox* Row = NewRow(Keys);
		Put(Row, MakeKey(EKeys::LeftShift, FText(), 2.25f, true, ShiftTip));
		for (const FKey& K : { EKeys::Y, EKeys::X, EKeys::C, EKeys::V, EKeys::B, EKeys::N, EKeys::M, EKeys::Comma, EKeys::Period })
		{
			Put(Row, MakeKey(K, FText(), 1.f));
		}
	}
	{
		UHorizontalBox* Row = NewRow(Keys);
		Put(Row, MakeKey(EKeys::LeftControl, FText(), 1.5f, true, CtrlTip));
		Put(Row, MakeKey(EKeys::LeftAlt, FText(), 1.5f, true, AltTip));
		Put(Row, MakeKey(EKeys::SpaceBar, FText(), 6.f));
	}

	// Navigationsblock - Zeilen auf Hoehe der Tastaturzeilen
	{
		PutGap(NewRow(Nav), 3.f);
		UHorizontalBox* R1 = NewRow(Nav);
		Put(R1, MakeKey(EKeys::Insert, FText(), 1.f));
		Put(R1, MakeKey(EKeys::Home, FText(), 1.f));
		Put(R1, MakeKey(EKeys::PageUp, FText(), 1.f));
		UHorizontalBox* R2 = NewRow(Nav);
		Put(R2, MakeKey(EKeys::Delete, FText(), 1.f));
		Put(R2, MakeKey(EKeys::End, FText(), 1.f));
		Put(R2, MakeKey(EKeys::PageDown, FText(), 1.f));
		PutGap(NewRow(Nav), 3.f);
		UHorizontalBox* R4 = NewRow(Nav);
		PutGap(R4, 1.f);
		Put(R4, MakeKey(EKeys::Up, FText(), 1.f));
		UHorizontalBox* R5 = NewRow(Nav);
		Put(R5, MakeKey(EKeys::Left, FText(), 1.f));
		Put(R5, MakeKey(EKeys::Down, FText(), 1.f));
		Put(R5, MakeKey(EKeys::Right, FText(), 1.f));
	}

	UVerticalBox* Mouse = WidgetTree->ConstructWidget<UVerticalBox>();
	{
		auto AddMouse = [Mouse](UWidget* W)
		{
			if (UVerticalBoxSlot* PanelSlot = Mouse->AddChildToVerticalBox(W))
			{
				PanelSlot->SetPadding(FMargin(3.f));
			}
		};
		AddMouse(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("MouseTitle", "MOUSE"), 15, Style.TitleColor, true, 200));
		AddMouse(MakeMouseEntry(LOCTEXT("LeftClick", "LEFT CLICK"), FText::GetEmpty()));
		AddMouse(MakeMouseEntry(LOCTEXT("RightClick", "RIGHT CLICK"), LOCTEXT("RightClickTip", "With a unit selected:\nRight click  -  Move unit\nSHIFT + Right click  -  Move along waypoints")));
		AddMouse(MakeMouseEntry(LOCTEXT("MiddleMouse", "MIDDLE MOUSE"), LOCTEXT("MiddleTip", "Hold and move  -  Rotate the camera freely")));
		AddMouse(MakeMouseEntry(LOCTEXT("Scroll", "SCROLL"), LOCTEXT("ScrollTip", "Scroll  -  Go through the ability pages\nALT + Scroll  -  Zoom camera\nCTRL + Scroll  -  Zoom camera")));
		AddMouse(MakeMouseEntry(LOCTEXT("ScreenEdge", "SCREEN EDGE"), LOCTEXT("EdgeTip", "Move the mouse to the screen edges  -  Pan camera")));
	}

	if (UHorizontalBoxSlot* PanelSlot = Main->AddChildToHorizontalBox(Keys)) { PanelSlot->SetVerticalAlignment(VAlign_Top); }
	if (UHorizontalBoxSlot* PanelSlot = Main->AddChildToHorizontalBox(Nav)) { PanelSlot->SetPadding(FMargin(22.f, 0.f, 0.f, 0.f)); PanelSlot->SetVerticalAlignment(VAlign_Top); }
	if (UHorizontalBoxSlot* PanelSlot = Main->AddChildToHorizontalBox(Mouse)) { PanelSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f)); PanelSlot->SetVerticalAlignment(VAlign_Center); }
	AddRow(Main, 12.f);

	UnassignedText = RTSKeyUI::MakeText(WidgetTree, Style, FText::GetEmpty(), 13, Style.WarningColor);
	UnassignedText->SetAutoWrapText(true);
	AddRow(UnassignedText, 12.f, HAlign_Fill);

	// Optionen
	AddRow(RTSKeyUI::MakeText(WidgetTree, Style, LOCTEXT("Options", "OPTIONS"), 15, Style.TitleColor, true, 200), 6.f);
	UHorizontalBox* Options = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto AddOption = [this, Options](const FText& Label, UCheckBox*& Out)
	{
		UTextBlock* Text = RTSKeyUI::MakeText(WidgetTree, Style, Label, 14, Style.TextColor);
		if (UHorizontalBoxSlot* PanelSlot = Options->AddChildToHorizontalBox(Text))
		{
			PanelSlot->SetVerticalAlignment(VAlign_Center);
			PanelSlot->SetPadding(FMargin(18.f, 0.f, 6.f, 0.f));
		}
		Out = WidgetTree->ConstructWidget<UCheckBox>();
		Options->AddChildToHorizontalBox(Out)->SetVerticalAlignment(VAlign_Center);
	};
	AddOption(LOCTEXT("SwapScroll", "Swap Scroll"), SwapScrollCheckBox);
	AddOption(LOCTEXT("SwapAttackMove", "Swap AttackMove"), SwapAttackMoveCheckBox);
	AddOption(LOCTEXT("InvertCam", "Invert Cam Angle"), InvertCameraAngleCheckBox);
	AddRow(Options, bShowAsWindow ? 20.f : 0.f);

	if (bShowAsWindow)
	{
		UButton* DoneButton = RTSKeyUI::MakeButton(WidgetTree, Style, LOCTEXT("Done", "DONE"), 16);
		DoneButton->OnClicked.AddDynamic(this, &UControlWidget::OnDoneClicked);
		AddRow(DoneButton, 0.f);
	}
}

FText UControlWidget::BuildKeyTooltip(const FKey& Key) const
{
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	if (!Bindings)
	{
		return FText::GetEmpty();
	}
	FString Lines;
	for (const FRTSKeyActionBinding& B : Bindings->GetBindingsForKey(Key))
	{
		Lines += FString::Printf(TEXT("%s  -  %s\n"), *B.Chord.ToText().ToString(), *URTSKeyBindingSubsystem::GetActionDisplayName(B.Action).ToString());
	}
	if (Lines.IsEmpty())
	{
		Lines = LOCTEXT("NoFunctionTip", "No function").ToString() + TEXT("\n");
	}
	Lines += TEXT("\n");
	Lines += (Bindings->GetActivePreset() == ERTSKeyLayoutPreset::UserDefined
		? LOCTEXT("ClickToChange", "Click to change")
		: LOCTEXT("ClickForDetails", "Click to change it (copies this layout into USER DEFINED)")).ToString();
	return FText::FromString(Lines);
}

void UControlWidget::ApplyTooltip(UWidget* Target, const FText& Title, const FText& Type, const FText& Body)
{
	if (!Target)
	{
		return;
	}
	UUserWidget* Tip = KeyTooltipClass ? CreateWidget<UUserWidget>(this, KeyTooltipClass) : nullptr;
	if (!Tip)
	{
		Target->SetToolTipText(Body);
		return;
	}
	// Die Felder des Tooltip-Blueprints (wie BP_ToolTip_ControlKey_AH) per Namen fuellen.
	auto SetText = [Tip](const TCHAR* Name, const FText& Value)
	{
		if (FTextProperty* Property = FindFProperty<FTextProperty>(Tip->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Tip, Value);
		}
	};
	SetText(TEXT("Name"), Title);
	SetText(TEXT("TypeName"), Type);
	SetText(TEXT("Description"), Body);
	Target->SetToolTip(Tip);
}

void UControlWidget::RefreshKeys()
{
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	if (!bBuiltKeyboard || !Bindings)
	{
		return;
	}

	for (const FKeyCap& Cap : KeyCaps)
	{
		UButton* Button = Cap.Button.Get();
		if (!Button)
		{
			continue;
		}
		const TArray<FRTSKeyActionBinding> OnKey = Cap.bFixed ? TArray<FRTSKeyActionBinding>() : Bindings->GetBindingsForKey(Cap.Key);
		const bool bBound = OnKey.Num() > 0;

		FButtonStyle ButtonStyle = Button->GetStyle();
		ButtonStyle.Normal = RTSKeyUI::MakeBrush(bBound ? Style.KeyNormal.Get() : (Style.KeyUnbound ? Style.KeyUnbound.Get() : Style.KeyNormal.Get()),
			bBound ? FLinearColor(0.08f, 0.17f, 0.21f, 0.96f) : FLinearColor(0.035f, 0.045f, 0.06f, 0.9f),
			bBound || Style.KeyUnbound ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f, 0.8f));
		ButtonStyle.Hovered = RTSKeyUI::MakeBrush(Style.KeyHover ? Style.KeyHover.Get() : Style.KeyNormal.Get(), FLinearColor(0.12f, 0.28f, 0.33f, 0.98f));
		ButtonStyle.Pressed = RTSKeyUI::MakeBrush(Style.KeyPressed ? Style.KeyPressed.Get() : Style.KeyNormal.Get(), FLinearColor(0.2f, 0.4f, 0.45f, 1.f));
		ButtonStyle.NormalPadding = FMargin(0.f);
		ButtonStyle.PressedPadding = FMargin(0.f);
		Button->SetStyle(ButtonStyle);

		if (UTextBlock* ActionLabel = Cap.ActionLabel.Get())
		{
			FText Short;
			if (bBound)
			{
				// Die Funktion ohne Modifikator zuerst, sonst die erste Kombination ("^" = mit Strg/Alt).
				const FRTSKeyActionBinding& First = OnKey[0];
				Short = URTSKeyBindingSubsystem::GetActionShortName(First.Action);
				if (First.Chord.bCtrl || First.Chord.bAlt)
				{
					Short = FText::Format(LOCTEXT("ShortCombo", "{0}+ {1}"), FText::FromString(First.Chord.bCtrl ? TEXT("C") : TEXT("A")), Short);
				}
			}
			ActionLabel->SetText(Short);
		}

		if (UWidget* Box = Button->GetParent())
		{
			ApplyTooltip(Box, KeyCapLabel(Cap.Key), LOCTEXT("KeyboardType", "Keyboard"), Cap.bFixed ? Cap.FixedTooltip : BuildKeyTooltip(Cap.Key));
		}
	}

	if (LeftClickMouseButton)
	{
		const FText DetectKey = Bindings->GetChordText(ERTSKeyAction::DetectEnemies);
		ApplyTooltip(LeftClickMouseButton, LOCTEXT("LeftClick", "LEFT CLICK"), LOCTEXT("MouseType", "Mouse"), DetectKey.IsEmpty()
			? LOCTEXT("LeftClickTipNoT", "Click  -  Select unit\nPress and hold  -  Select units")
			: FText::Format(LOCTEXT("LeftClickTip", "Click  -  Select unit\nPress and hold  -  Select units\n{0} + Left click  -  Run to position and detect enemies"), DetectKey));
	}

	if (UnassignedText)
	{
		const TArray<ERTSKeyAction> Unbound = Bindings->GetUnboundActions();
		FString Names;
		for (const ERTSKeyAction Action : Unbound)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(",  ")) + URTSKeyBindingSubsystem::GetActionDisplayName(Action).ToString();
		}
		UnassignedText->SetText(Names.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("Unassigned", "Without a key:  {0}"), FText::FromString(Names)));
		UnassignedText->SetVisibility(Names.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (LayoutHintText)
	{
		LayoutHintText->SetText(Bindings->GetActivePreset() == ERTSKeyLayoutPreset::UserDefined
			? LOCTEXT("HintUser", "Your own layout  -  hover over a key to see what it does, click it to change it.")
			: FText::Format(LOCTEXT("HintPreset", "{0} layout  -  hover over a key to see what it does. To change keys, switch to USER DEFINED."),
				URTSKeyBindingSubsystem::GetPresetDisplayName(Bindings->GetActivePreset())));
	}
	UpdatePresetButtons();
}

void UControlWidget::UpdatePresetButtons()
{
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	const ERTSKeyLayoutPreset Active = Bindings ? Bindings->GetActivePreset() : ERTSKeyLayoutPreset::Modern;
	// Das aktive Layout: heller Knopf, Schrift in der Akzentfarbe; die anderen gedaempft.
	const FLinearColor ActiveTint(1.6f, 1.6f, 1.6f, 1.f);
	const FLinearColor IdleTint(0.65f, 0.65f, 0.65f, 0.9f);
	const TPair<UButton*, UTextBlock*> Buttons[3] = { { ModernButton, ModernLabel }, { ClassicButton, ClassicLabel }, { UserButton, UserLabel } };
	const ERTSKeyLayoutPreset Presets[3] = { ERTSKeyLayoutPreset::Modern, ERTSKeyLayoutPreset::Classic, ERTSKeyLayoutPreset::UserDefined };
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bActive = Active == Presets[i];
		RTSKeyUI::SetButtonTint(Buttons[i].Key, Style, bActive ? ActiveTint : IdleTint);
		if (UTextBlock* Label = Buttons[i].Value)
		{
			Label->SetColorAndOpacity(FSlateColor(bActive ? Style.AccentColor * FLinearColor(1.5f, 1.5f, 1.5f, 1.f) : Style.DimColor));
		}
	}
	static const float CursorFactors[4] = { 1.f, 1.25f, 1.5f, 2.f };
	const float CursorScale = Bindings ? Bindings->GetCursorScale() : 1.f;
	for (int32 i = 0; i < CursorSizeButtons.Num() && i < 4; ++i)
	{
		const bool bActive = FMath::IsNearlyEqual(CursorScale, CursorFactors[i], 0.01f);
		RTSKeyUI::SetButtonTint(CursorSizeButtons[i], Style, bActive ? ActiveTint : IdleTint);
		if (CursorSizeLabels.IsValidIndex(i) && CursorSizeLabels[i])
		{
			CursorSizeLabels[i]->SetColorAndOpacity(FSlateColor(bActive ? Style.AccentColor * FLinearColor(1.5f, 1.5f, 1.5f, 1.f) : Style.DimColor));
		}
	}
	const ESlateVisibility ResetVisibility = Active == ERTSKeyLayoutPreset::UserDefined ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (ResetModernButton) { ResetModernButton->SetVisibility(ResetVisibility); }
	if (ResetClassicButton) { ResetClassicButton->SetVisibility(ResetVisibility); }
}

void UControlWidget::OnCursorSize100Clicked() { if (URTSKeyBindingSubsystem* B = URTSKeyBindingSubsystem::Get(this)) { B->SetCursorScale(1.f); } }
void UControlWidget::OnCursorSize125Clicked() { if (URTSKeyBindingSubsystem* B = URTSKeyBindingSubsystem::Get(this)) { B->SetCursorScale(1.25f); } }
void UControlWidget::OnCursorSize150Clicked() { if (URTSKeyBindingSubsystem* B = URTSKeyBindingSubsystem::Get(this)) { B->SetCursorScale(1.5f); } }
void UControlWidget::OnCursorSize200Clicked() { if (URTSKeyBindingSubsystem* B = URTSKeyBindingSubsystem::Get(this)) { B->SetCursorScale(2.f); } }

void UControlWidget::OnBindingsChanged()
{
	RefreshKeys();
}

void UControlWidget::OnModernClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::Modern); }
}

void UControlWidget::OnClassicClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::Classic); }
}

void UControlWidget::OnUserClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->SetActivePreset(ERTSKeyLayoutPreset::UserDefined); }
}

void UControlWidget::OnResetModernClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->ResetUserLayout(ERTSKeyLayoutPreset::Modern); }
}

void UControlWidget::OnResetClassicClicked()
{
	if (URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this)) { Bindings->ResetUserLayout(ERTSKeyLayoutPreset::Classic); }
}

const UControlWidget* UControlWidget::FindGameKeyboard(const APlayerController* PC)
{
	const ACameraBase* Camera = PC ? Cast<ACameraBase>(PC->GetPawn()) : nullptr;
	return Camera ? Cast<UControlWidget>(Camera->ControlWidget) : nullptr;
}

void UControlWidget::OnKeyCapClicked(const FKey& Key)
{
	// The key list always in the look of the Esc menu keyboard (the loading screen may have been built
	// before the HUD existed).
	const UControlWidget* GameKeyboard = FindGameKeyboard(GetOwningPlayer());
	URTSKeyBindingListWidget::Open(GetOwningPlayer(), GameKeyboard ? GameKeyboard->Style : Style, Key, nullptr);
}

#undef LOCTEXT_NAMESPACE
