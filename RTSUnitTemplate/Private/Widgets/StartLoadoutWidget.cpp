// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/StartLoadoutWidget.h"
#include "Components/StartLoadoutPlayerComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GAS/AutoAbility.h"

// Test helper: the widget chooses and confirms by itself in the game's own tick (a call from an editor script runs a
// Server RPC locally, so a script cannot stand in for the click). The server takes set N, a client set N+1.
static TAutoConsoleVariable<int32> CVarStartLoadoutAutoPick(TEXT("RTS.StartLoadout.AutoPick"), -1,
	TEXT("Start loadout test: -1 = off, N = the selection widget picks set N by itself (on clients N+1)."));

namespace
{
	const FLinearColor TextColour(0.92f, 0.95f, 1.f, 1.f);
	const FLinearColor DimColour(0.6f, 0.68f, 0.78f, 1.f);

	// Nearly opaque: the HUD behind the choice made the screen look busy (feedback 21.11.2026).
	constexpr float StartLoadoutBackdropOpacity = 0.94f;

	FSlateBrush MaterialBrush(UMaterialInterface* Material, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		if (Material)
		{
			Brush.SetResourceObject(Material);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(Tint);
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.08f, 0.12f, 0.92f) * Tint);
		}
		return Brush;
	}

	// ChakraPetch from the project (the plugin ships without it); the engine font where it is missing.
	FSlateFontInfo LoadoutFont(const FSoftObjectPath& Path, int32 Size, const ANSICHAR* FallbackStyle)
	{
		if (UObject* FontObject = Path.IsNull() ? nullptr : Path.TryLoad())
		{
			return FSlateFontInfo(FontObject, Size);
		}
		return FCoreStyle::GetDefaultFontStyle(FallbackStyle, Size);
	}

	UTextBlock* Spaced(UTextBlock* Block, int32 LetterSpacing)
	{
		FSlateFontInfo Font = Block->GetFont();
		Font.LetterSpacing = LetterSpacing;
		Block->SetFont(Font);
		return Block;
	}

	FText TypeLabel(EStartLoadoutCardType Type)
	{
		switch (Type)
		{
		case EStartLoadoutCardType::MainBase: return NSLOCTEXT("StartLoadout", "TypeMainBase", "MAIN BASE");
		case EStartLoadoutCardType::Workers: return NSLOCTEXT("StartLoadout", "TypeWorkers", "WORKERS");
		case EStartLoadoutCardType::Army: return NSLOCTEXT("StartLoadout", "TypeArmy", "ARMY");
		case EStartLoadoutCardType::Tech: return NSLOCTEXT("StartLoadout", "TypeTech", "TECH");
		case EStartLoadoutCardType::Effect: return NSLOCTEXT("StartLoadout", "TypeEffect", "EFFECT");
		case EStartLoadoutCardType::Support: return NSLOCTEXT("StartLoadout", "TypeSupport", "SUPPORT");
		case EStartLoadoutCardType::AutoAbility: return NSLOCTEXT("StartLoadout", "TypeAutoAbility", "AUTO ABILITY");
		case EStartLoadoutCardType::UnitType: return NSLOCTEXT("StartLoadout", "TypeUnitType", "FOR EVERY");
		default: return NSLOCTEXT("StartLoadout", "TypeProduction", "PRODUCTION");
		}
	}
}

namespace
{
	// The selection screens on the screen right now (several PIE clients share the process, hence per world).
	TArray<TWeakObjectPtr<UUserWidget>> OpenLoadoutWidgets;
}

bool UStartLoadoutWidget::IsAnyOpen(const UWorld* World)
{
	OpenLoadoutWidgets.RemoveAll([](const TWeakObjectPtr<UUserWidget>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<UUserWidget>& W : OpenLoadoutWidgets)
	{
		if (!World || W->GetWorld() == World)
		{
			return true;
		}
	}
	return false;
}

void UStartLoadoutWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OpenLoadoutWidgets.AddUnique(this);
}

void UStartLoadoutWidget::NativeDestruct()
{
	OpenLoadoutWidgets.Remove(this);
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------------------------------------------

void UStartLoadoutDifficultyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OpenLoadoutWidgets.AddUnique(this);
}

void UStartLoadoutDifficultyWidget::NativeDestruct()
{
	OpenLoadoutWidgets.Remove(this);
	Super::NativeDestruct();
}

UTextBlock* UStartLoadoutDifficultyWidget::MakeText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bWrap)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	Block->SetFont(LoadoutFont(Size >= 20 ? HeadingFontPath : BodyFontPath, Size, Size >= 20 ? "Bold" : "Regular"));
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetAutoWrapText(bWrap);
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

void UStartLoadoutDifficultyWidget::Setup(UStartLoadoutPlayerComponent* InOwner, const TArray<FStartLoadoutDifficulty>& InLevels, int32 InDefaultIndex, const FText& InHeader,
	UMaterialInterface* CardMaterial)
{
	OwnerComponent = InOwner;
	Levels = InLevels;
	DefaultIndex = InDefaultIndex;
	bChosen = false;
	if (!WidgetTree)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.02f, StartLoadoutBackdropOpacity));
	if (UOverlaySlot* S = Root->AddChildToOverlay(Dim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* S = Root->AddChildToOverlay(Column))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	auto AddRow = [Column](UWidget* W, float Bottom)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		}
	};
	AddRow(Spaced(MakeText(InHeader.ToUpper(), 36, TextColour), 160), 6.f);
	UTextBlock* Hint = MakeText(HintText, 15, DimColour, true);
	USizeBox* HintBox = WidgetTree->ConstructWidget<USizeBox>();
	HintBox->SetMaxDesiredWidth(900.f);
	HintBox->AddChild(Hint);
	AddRow(HintBox, 28.f);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddRow(Row, 0.f);
	for (int32 i = 0; i < Levels.Num(); ++i)
	{
		const FStartLoadoutDifficulty& Level = Levels[i];
		const bool bPreselected = i == DefaultIndex;
		const FLinearColor Accent = Level.AccentColor;

		// Card: frame in the level's colour > button with the faction's card face > accent strip, level number,
		// name, threat meter, enemy strength, description and the ranking badge.
		USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();
		CardBox->SetWidthOverride(320.f);
		CardBox->SetMinDesiredHeight(360.f);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetPadding(FMargin(bPreselected ? 3.f : 2.f));
		Frame->SetBrushColor(Accent * FLinearColor(1.f, 1.f, 1.f, bPreselected ? 1.f : 0.6f));
		Frame->SetHorizontalAlignment(HAlign_Fill);
		Frame->SetVerticalAlignment(VAlign_Fill);
		CardBox->AddChild(Frame);

		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Style;
		Style.SetNormal(MaterialBrush(CardMaterial, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)));
		Style.SetHovered(MaterialBrush(CardMaterial, FLinearColor(1.25f, 1.25f, 1.25f, 1.f)));
		Style.SetPressed(MaterialBrush(CardMaterial, FLinearColor(0.6f, 0.6f, 0.6f, 1.f)));
		Style.SetNormalPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		Style.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 17.f));
		Button->SetStyle(Style);
		Frame->SetContent(Button);

		UStartLoadoutDifficultyButtonProxy* Proxy = NewObject<UStartLoadoutDifficultyButtonProxy>(this);
		Proxy->Widget = this;
		Proxy->Index = i;
		Proxies.Add(Proxy);
		Button->OnClicked.AddDynamic(Proxy, &UStartLoadoutDifficultyButtonProxy::HandleClicked);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UButtonSlot* ButtonContent = Cast<UButtonSlot>(Button->AddChild(Content)))
		{
			ButtonContent->SetHorizontalAlignment(HAlign_Fill);
			ButtonContent->SetVerticalAlignment(VAlign_Fill);
		}
		auto Add = [Content](UWidget* W, float Top, EHorizontalAlignment Align = HAlign_Center, bool bFill = false)
		{
			if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(W))
			{
				S->SetHorizontalAlignment(Align);
				S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
				if (bFill)
				{
					S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
		};

		// Accent strip across the top edge.
		UBorder* Strip = WidgetTree->ConstructWidget<UBorder>();
		Strip->SetBrushColor(Accent);
		Strip->SetPadding(FMargin(0.f, 2.f));
		Add(Strip, 0.f, HAlign_Fill);

		Add(Spaced(MakeText(FText::Format(NSLOCTEXT("StartLoadout", "DifficultyLevel", "LEVEL {0}"), FText::AsNumber(i + 1)), 12, DimColour), 300), 14.f);
		Add(Spaced(MakeText(Level.Name.ToUpper(), 28, Accent), 90), 2.f);

		// Threat meter: five segments, filled by the enemies' strength.
		const float Strength = 0.5f * (Level.EnemyHealthMultiplier + Level.EnemyDamageMultiplier);
		const int32 Filled = FMath::Clamp(FMath::RoundToInt((Strength - 0.4f) / 0.6f * 4.f) + 1, 1, 5);
		UHorizontalBox* Meter = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (int32 Seg = 0; Seg < 5; ++Seg)
		{
			USizeBox* SegBox = WidgetTree->ConstructWidget<USizeBox>();
			SegBox->SetWidthOverride(30.f);
			SegBox->SetHeightOverride(7.f);
			UBorder* SegFill = WidgetTree->ConstructWidget<UBorder>();
			SegFill->SetBrushColor(Seg < Filled ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.12f));
			SegBox->AddChild(SegFill);
			if (UHorizontalBoxSlot* S = Meter->AddChildToHorizontalBox(SegBox))
			{
				S->SetPadding(FMargin(2.f, 0.f));
			}
		}
		Add(Meter, 12.f);

		const FText StrengthText = FMath::IsNearlyEqual(Level.EnemyHealthMultiplier, 1.f) && FMath::IsNearlyEqual(Level.EnemyDamageMultiplier, 1.f)
			? NSLOCTEXT("StartLoadout", "DifficultyFull", "ENEMIES AT FULL STRENGTH")
			: FText::Format(NSLOCTEXT("StartLoadout", "DifficultyStrength", "ENEMY HEALTH {0}%  |  DAMAGE {1}%"),
				FText::AsNumber(FMath::RoundToInt(Level.EnemyHealthMultiplier * 100.f)), FText::AsNumber(FMath::RoundToInt(Level.EnemyDamageMultiplier * 100.f)));
		Add(Spaced(MakeText(StrengthText, 12, TextColour), 30), 8.f);

		// Divider.
		USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>();
		LineBox->SetWidthOverride(220.f);
		LineBox->SetHeightOverride(1.f);
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
		Line->SetBrushColor(Accent * FLinearColor(1.f, 1.f, 1.f, 0.45f));
		LineBox->AddChild(Line);
		Add(LineBox, 14.f);

		UTextBlock* Desc = MakeText(Level.Description, 15, TextColour, true);
		USizeBox* DescBox = WidgetTree->ConstructWidget<USizeBox>();
		DescBox->SetWidthOverride(270.f);
		DescBox->AddChild(Desc);
		Add(DescBox, 14.f, HAlign_Center, true);

		// Ranking badge.
		const bool bRanked = Level.IsRanked();
		UBorder* Badge = WidgetTree->ConstructWidget<UBorder>();
		Badge->SetBrushColor(bRanked ? FLinearColor(1.f, 0.7f, 0.15f, 0.22f) : FLinearColor(1.f, 1.f, 1.f, 0.07f));
		Badge->SetPadding(FMargin(14.f, 5.f));
		Badge->SetContent(Spaced(MakeText(bRanked
			? NSLOCTEXT("StartLoadout", "DifficultyRanked", "RANKED - COUNTS FOR THE BEST TIMES")
			: NSLOCTEXT("StartLoadout", "DifficultyUnranked", "UNRANKED - SHOWS YOUR TIME ONLY"), 11,
			bRanked ? FLinearColor(1.f, 0.78f, 0.25f, 1.f) : DimColour), 20));
		Add(Badge, 12.f);

		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CardBox))
		{
			S->SetPadding(FMargin(12.f, 0.f));
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void UStartLoadoutDifficultyWidget::ChooseLevel(int32 Index)
{
	if (bChosen || !Levels.IsValidIndex(Index) || !OwnerComponent)
	{
		return;
	}
	bChosen = true;
	SetIsEnabled(false);
	OwnerComponent->ChooseDifficulty(Index);
}

void UStartLoadoutDifficultyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The card test (RTS.StartLoadout.AutoPick) takes the preselected level.
	if (!bChosen && CVarStartLoadoutAutoPick.GetValueOnGameThread() >= 0)
	{
		ChooseLevel(Levels.IsValidIndex(DefaultIndex) ? DefaultIndex : Levels.Num() - 1);
	}
}

void UStartLoadoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A Blueprint child with its own layout keeps it.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
}

UTextBlock* UStartLoadoutWidget::MakeText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bWrap)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	Block->SetFont(LoadoutFont(Size >= 20 ? HeadingFontPath : BodyFontPath, Size, Size >= 20 ? "Bold" : "Regular"));
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetAutoWrapText(bWrap);
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

void UStartLoadoutWidget::BuildLayout()
{
	bBuiltLayout = true;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.02f, StartLoadoutBackdropOpacity));
	if (UOverlaySlot* S = Root->AddChildToOverlay(Dim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* S = Root->AddChildToOverlay(Column))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	auto AddRow = [Column](UWidget* W, float Bottom)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		}
	};
	HeaderBlock = Spaced(MakeText(HeaderText, 34, TextColour), 120);
	AddRow(HeaderBlock, 6.f);
	HintBlock = MakeText(HintText, 15, DimColour, true);
	USizeBox* HintBox = WidgetTree->ConstructWidget<USizeBox>();
	HintBox->SetMaxDesiredWidth(900.f);
	HintBox->AddChild(HintBlock);
	AddRow(HintBox, 20.f);

	// The sets, and right of them the place the confirm bar moves to when the screen is too low for it below the
	// sets (a set with many cards on 1080p) - see UpdateConfirmPlacement.
	LayoutColumn = Column;
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	SetRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(SetRow))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	SideColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(SideColumn))
	{
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
	}
	AddRow(Body, 20.f);

	UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>();
	ConfirmBar = Bottom;
	CountdownText = MakeText(FText::GetEmpty(), 18, DimColour);
	if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(CountdownText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));
	}
	ConfirmButton = WidgetTree->ConstructWidget<UButton>();
	ConfirmButton->SetBackgroundColor(FLinearColor(0.2f, 0.6f, 0.9f, 1.f));
	UTextBlock* ConfirmLabel = MakeText(ConfirmText, 20, FLinearColor::White);
	ConfirmButton->AddChild(ConfirmLabel);
	if (UButtonSlot* S = Cast<UButtonSlot>(ConfirmLabel->Slot))
	{
		S->SetPadding(FMargin(40.f, 10.f));
	}
	ConfirmButton->OnClicked.AddDynamic(this, &UStartLoadoutWidget::HandleConfirmClicked);
	ConfirmButton->SetIsEnabled(false);
	Bottom->AddChildToHorizontalBox(ConfirmButton);
	AddRow(Bottom, 10.f);

	StatusText = MakeText(FText::GetEmpty(), 16, DimColour);
	AddRow(StatusText, 0.f);
}

FText UStartLoadoutWidget::CardTitle(const FStartLoadoutCard& Card)
{
	if (!Card.Title.IsEmpty())
	{
		return Card.Title;
	}
	if (!Card.UnitClass)
	{
		return FText::GetEmpty();
	}
	FString Name = Card.UnitClass->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	for (const TCHAR* Cut : { TEXT("BP_"), TEXT("BuildingBase_"), TEXT("UnitBaseRanged_"), TEXT("UnitBase_"), TEXT("Singularian_"), TEXT("Xeno_"), TEXT("_AH") })
	{
		Name.ReplaceInline(Cut, TEXT(""));
	}
	return FText::FromString(Name);
}

UWidget* UStartLoadoutWidget::MakeCard(const FStartLoadoutCard& Card, EStartLoadoutCardType Type, UMaterialInterface* Material, bool bLocked)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(140.f);
	Box->SetHeightOverride(176.f);

	UBorder* Face = WidgetTree->ConstructWidget<UBorder>();
	Face->SetBrush(MaterialBrush(Material, bLocked ? FLinearColor(0.75f, 0.75f, 0.75f, 1.f) : FLinearColor::White));
	Face->SetPadding(FMargin(10.f, 8.f));
	Face->SetHorizontalAlignment(HAlign_Fill);
	Face->SetVerticalAlignment(VAlign_Fill);
	Box->AddChild(Face);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Face->SetContent(Content);
	auto Add = [Content](UWidget* W, bool bFill = false)
	{
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			if (bFill)
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
	};
	Add(Spaced(MakeText(TypeLabel(Type), 11, DimColour), 120));

	UTexture2D* Icon = Card.Icon;
	if (!Icon && Card.UnitClass)
	{
		Icon = Card.UnitClass->GetDefaultObject<AUnitBase>()->UnitIcon;
	}
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(84.f);
	IconBox->SetHeightOverride(84.f);
	if (Icon)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>();
		Image->SetBrushFromTexture(Icon, false);
		IconBox->AddChild(Image);
	}
	Add(IconBox, true);

	const FText Count = Type == EStartLoadoutCardType::MainBase || Card.Count <= 1
		? CardTitle(Card)
		: FText::Format(NSLOCTEXT("StartLoadout", "CountTitle", "{0} x {1}"), FText::AsNumber(Card.Count), CardTitle(Card));
	Add(MakeText(Count, 14, TextColour, true));
	if (bLocked)
	{
		Add(MakeText(NSLOCTEXT("StartLoadout", "Fixed", "always included"), 11, DimColour));
	}
	return Box;
}

void UStartLoadoutWidget::Setup(UStartLoadoutPlayerComponent* InOwner, const FStartLoadoutFaction& InFaction, const TArray<FName>& InRowNames,
	const TArray<FStartLoadoutSetRow>& InSets, float Seconds, const FText& InHint)
{
	OwnerComponent = InOwner;
	Faction = InFaction;
	RowNames = InRowNames;
	Sets = InSets;
	SelectedIndex = INDEX_NONE;
	bConfirmed = false;
	EndTime = Seconds > 0.f ? FPlatformTime::Seconds() + Seconds : 0.0;
	if (HeaderBlock)
	{
		HeaderBlock->SetText(HeaderText);
	}
	if (HintBlock)
	{
		// Only the start selection shows the main base card.
		HintBlock->SetText(!InHint.IsEmpty() ? InHint : (Faction.MainBase.UnitClass ? HintText : ReinforcementsHintText));
	}
	if (bBuiltLayout)
	{
		BuildSets();
	}
	OnLoadoutOffered(Faction, RowNames, Sets, Seconds);
}

void UStartLoadoutWidget::BuildSets()
{
	SetRow->ClearChildren();
	SetFrames.Reset();
	Proxies.Reset();

	// The fixed main base first.
	if (Faction.MainBase.UnitClass)
	{
		UVerticalBox* Fixed = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UVerticalBoxSlot* S = Fixed->AddChildToVerticalBox(MakeText(Faction.DisplayName.IsEmpty() ? FText::FromName(Faction.Faction) : Faction.DisplayName, 20, TextColour)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));
		}
		if (UVerticalBoxSlot* S = Fixed->AddChildToVerticalBox(MakeCard(Faction.MainBase, EStartLoadoutCardType::MainBase, Faction.CardMaterial, true)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* S = SetRow->AddChildToHorizontalBox(Fixed))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 26.f, 0.f));
		}
	}

	// All set cards look alike: same width (description and card grid are fixed), the same height (every set
	// stretches to the tallest one, content at the top) and the bonus line keeps its room in every set as soon as
	// one of them has a bonus.
	bool bAnyBonus = false;
	bool bAnyAutoAbility = false;
	for (const FStartLoadoutSetRow& Set : Sets)
	{
		bAnyBonus |= !Set.Bonus.Title.IsEmpty();
		bAnyAutoAbility |= Set.AutoAbility.IsSet();
	}

	for (int32 i = 0; i < Sets.Num(); ++i)
	{
		const FStartLoadoutSetRow& Set = Sets[i];
		UMaterialInterface* Material = Set.CardMaterial ? Set.CardMaterial.Get() : Faction.CardMaterial.Get();

		// Frame (lights up when selected) > button > title, text and the four cards.
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetPadding(FMargin(4.f));
		Frame->SetHorizontalAlignment(HAlign_Fill);
		Frame->SetVerticalAlignment(VAlign_Fill);
		SetFrames.Add(Frame);

		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Style;
		Style.SetNormal(MaterialBrush(Material, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)));
		Style.SetHovered(MaterialBrush(Material, FLinearColor(1.1f, 1.1f, 1.1f, 1.f)));
		Style.SetPressed(MaterialBrush(Material, FLinearColor(0.65f, 0.65f, 0.65f, 1.f)));
		Style.SetNormalPadding(FMargin(14.f, 12.f));
		Style.SetPressedPadding(FMargin(14.f, 13.f, 14.f, 11.f));
		Button->SetStyle(Style);
		Frame->SetContent(Button);

		UStartLoadoutSetButtonProxy* Proxy = NewObject<UStartLoadoutSetButtonProxy>(this);
		Proxy->Widget = this;
		Proxy->Index = i;
		Proxies.Add(Proxy);
		Button->OnClicked.AddDynamic(Proxy, &UStartLoadoutSetButtonProxy::HandleClicked);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UButtonSlot* ButtonContent = Cast<UButtonSlot>(Button->AddChild(Content)))
		{
			ButtonContent->SetHorizontalAlignment(HAlign_Center);
			ButtonContent->SetVerticalAlignment(VAlign_Top);
		}
		auto Add = [Content](UWidget* W, float Bottom)
		{
			if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(W))
			{
				S->SetHorizontalAlignment(HAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			}
		};
		// Title and bonus line have the width of the card grid; a long bonus wraps (two lines of room) instead of
		// making this set wider than the others.
		USizeBox* TitleBox = WidgetTree->ConstructWidget<USizeBox>();
		TitleBox->SetWidthOverride(296.f);
		TitleBox->AddChild(MakeText(Set.Title, 20, Set.AccentColor, true));
		Add(TitleBox, 2.f);
		if (bAnyBonus)
		{
			USizeBox* BonusBox = WidgetTree->ConstructWidget<USizeBox>();
			BonusBox->SetWidthOverride(296.f);
			BonusBox->SetHeightOverride(46.f);
			if (!Set.Bonus.Title.IsEmpty())
			{
				BonusBox->AddChild(MakeText(FText::Format(NSLOCTEXT("StartLoadout", "BonusLine", "Start bonus: {0}"), Set.Bonus.Title), 14, FLinearColor(1.f, 0.78f, 0.25f, 1.f), true));
			}
			Add(BonusBox, 4.f);
		}
		USizeBox* DescBox = WidgetTree->ConstructWidget<USizeBox>();
		DescBox->SetWidthOverride(296.f);
		// The auto ability text (effect + cooldown) needs more lines.
		DescBox->SetHeightOverride(bAnyAutoAbility ? 112.f : 64.f);
		DescBox->AddChild(MakeText(Set.Description, 12, DimColour, true));
		Add(DescBox, 8.f);

		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		Grid->SetSlotPadding(FMargin(4.f));
		// Only the cards the set has (reinforcements come without buildings), the extra army after the army, plus the
		// cooldown reset.
		TArray<TPair<const FStartLoadoutCard*, EStartLoadoutCardType>> Cards;
		Cards.Emplace(&Set.Workers, EStartLoadoutCardType::Workers);
		Cards.Emplace(&Set.Army, EStartLoadoutCardType::Army);
		for (const FStartLoadoutCard& Extra : Set.ExtraArmy)
		{
			Cards.Emplace(&Extra, EStartLoadoutCardType::Army);
		}
		Cards.Emplace(&Set.Tech, EStartLoadoutCardType::Tech);
		Cards.Emplace(&Set.Production, EStartLoadoutCardType::Production);
		Cards.Emplace(&Set.Support, EStartLoadoutCardType::Support);
		int32 CardSlot = 0;
		for (const TPair<const FStartLoadoutCard*, EStartLoadoutCardType>& Card : Cards)
		{
			if (Card.Key->UnitClass && Card.Key->Count > 0)
			{
				Grid->AddChildToUniformGrid(MakeCard(*Card.Key, Card.Value, Material, false), CardSlot / 2, CardSlot % 2);
				++CardSlot;
			}
		}
		if (Set.AutoAbility.IsSet())
		{
			// Two cards: the ability, and the unit type it is switched on for.
			const UAutoAbility* Ability = Set.AutoAbility.Ability->GetDefaultObject<UAutoAbility>();
			FStartLoadoutCard AbilityCard;
			AbilityCard.Title = Ability->GetAutoName();
			AbilityCard.Icon = Ability->AbilityIcon;
			Grid->AddChildToUniformGrid(MakeCard(AbilityCard, EStartLoadoutCardType::AutoAbility, Material, false), CardSlot / 2, CardSlot % 2);
			++CardSlot;
			FStartLoadoutCard TypeCard;
			TypeCard.Title = Set.AutoAbility.TargetName.IsEmpty() ? FText::FromString(Set.AutoAbility.UnitTag.ToString()) : Set.AutoAbility.TargetName;
			TypeCard.Icon = Set.AutoAbility.TargetIcon;
			Grid->AddChildToUniformGrid(MakeCard(TypeCard, EStartLoadoutCardType::UnitType, Material, false), CardSlot / 2, CardSlot % 2);
			++CardSlot;
		}
		if (Set.bResetCooldowns)
		{
			FStartLoadoutCard Reset;
			Reset.Title = NSLOCTEXT("StartLoadout", "ResetCooldowns", "All cooldowns reset");
			Reset.Icon = Faction.ResetCooldownsIcon;
			Grid->AddChildToUniformGrid(MakeCard(Reset, EStartLoadoutCardType::Effect, Material, false), CardSlot / 2, CardSlot % 2);
		}
		Add(Grid, 0.f);

		if (UHorizontalBoxSlot* S = SetRow->AddChildToHorizontalBox(Frame))
		{
			S->SetPadding(FMargin(8.f, 0.f));
			// Fill: every set as tall as the tallest one.
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}
	RefreshSelection();
}

void UStartLoadoutWidget::SelectSet(int32 Index)
{
	if (bConfirmed || !Sets.IsValidIndex(Index))
	{
		return;
	}
	SelectedIndex = Index;
	RefreshSelection();
}

void UStartLoadoutWidget::RefreshSelection()
{
	for (int32 i = 0; i < SetFrames.Num(); ++i)
	{
		const bool bSelected = i == SelectedIndex;
		SetFrames[i]->SetBrushColor(bSelected ? Sets[i].AccentColor : FLinearColor(0.f, 0.f, 0.f, 0.f));
		SetFrames[i]->SetRenderScale(FVector2D(bSelected ? 1.03f : 1.f));
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(!bConfirmed && SelectedIndex != INDEX_NONE);
	}
	if (StatusText)
	{
		StatusText->SetText(bConfirmed ? WaitingText : (Sets.IsValidIndex(SelectedIndex) ? Sets[SelectedIndex].Title : FText::GetEmpty()));
	}
}

void UStartLoadoutWidget::ConfirmSelection()
{
	if (bConfirmed || !RowNames.IsValidIndex(SelectedIndex))
	{
		return;
	}
	bConfirmed = true;
	if (OwnerComponent)
	{
		OwnerComponent->ChooseLoadout(RowNames[SelectedIndex]);
	}
	RefreshSelection();
}

void UStartLoadoutWidget::HandleConfirmClicked()
{
	ConfirmSelection();
}

void UStartLoadoutWidget::UpdateConfirmPlacement(const FGeometry& MyGeometry)
{
	if (!bBuiltLayout || !LayoutColumn || !ConfirmBar || !SideColumn)
	{
		return;
	}
	const float Available = MyGeometry.GetLocalSize().Y;
	if (Available <= 1.f)
	{
		return;
	}
	// The height the column needs with the bar below the sets. Moved beside them as soon as it does not fit, back
	// only with clear room again (no flicker at the edge).
	const float BarHeight = ConfirmBar->GetDesiredSize().Y + 10.f;
	const float Needed = LayoutColumn->GetDesiredSize().Y + (bConfirmAtSide ? BarHeight : 0.f);
	const bool bWantSide = bConfirmAtSide ? Needed > Available - 60.f : Needed > Available - 16.f;
	if (bWantSide == bConfirmAtSide)
	{
		return;
	}
	bConfirmAtSide = bWantSide;
	ConfirmBar->RemoveFromParent();
	if (bConfirmAtSide)
	{
		if (UVerticalBoxSlot* S = SideColumn->AddChildToVerticalBox(ConfirmBar))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
	}
	else
	{
		// Back to its place below the sets (header, hint, sets, bar, status).
		LayoutColumn->InsertChildAt(3, ConfirmBar);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(ConfirmBar->Slot))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
	}
}

void UStartLoadoutWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateConfirmPlacement(MyGeometry);
	const int32 AutoPick = CVarStartLoadoutAutoPick.GetValueOnGameThread();
	if (AutoPick >= 0 && !bConfirmed && Sets.Num() > 0)
	{
		const APlayerController* PC = GetOwningPlayer();
		SelectSet((AutoPick + (PC && PC->HasAuthority() ? 0 : 1)) % Sets.Num());
		ConfirmSelection();
	}
	if (CountdownText)
	{
		const int32 Left = EndTime > 0.0 ? FMath::Max(0, FMath::CeilToInt(EndTime - FPlatformTime::Seconds())) : -1;
		CountdownText->SetText(Left < 0 ? FText::GetEmpty() : FText::Format(NSLOCTEXT("StartLoadout", "TimeLeft", "{0} s"), FText::AsNumber(Left)));
	}
}
