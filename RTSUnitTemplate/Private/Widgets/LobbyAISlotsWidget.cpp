// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/LobbyAISlotsWidget.h"
#include "System/PlayerTeamSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* LobbyText(UWidgetTree* Tree, const FText& Text, int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		Block->SetFont(FCoreStyle::GetDefaultFontStyle(Size >= 18 ? "Bold" : "Regular", Size));
		Block->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 1.f, 1.f)));
		return Block;
	}
}

UPlayerTeamSubsystem* ULobbyAISlotsWidget::GetTeamSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UPlayerTeamSubsystem>() : nullptr;
}

void ULobbyAISlotsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(LobbyText(WidgetTree, TitleText, 18)))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
		}
		Lines = WidgetTree->ConstructWidget<UVerticalBox>();
		Root->AddChildToVerticalBox(Lines);
	}
}

void ULobbyAISlotsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Only the host decides - the AI players are spawned by its game mode. Clients do not see the setting.
	const UWorld* World = GetWorld();
	SetVisibility(World && World->GetNetMode() == NM_Client ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	Rebuild();
}

void ULobbyAISlotsWidget::SetTeams(const TArray<int32>& InTeams)
{
	Teams = InTeams;
	if (UPlayerTeamSubsystem* Subsystem = GetTeamSubsystem())
	{
		TArray<int32> Slots = Subsystem->GetAISlots();
		Slots.RemoveAll([this](int32 Team) { return !Teams.Contains(Team); });
		Subsystem->SetAISlots(Slots);
	}
	Rebuild();
}

int32 ULobbyAISlotsWidget::GetAICount(int32 TeamId) const
{
	const UPlayerTeamSubsystem* Subsystem = GetTeamSubsystem();
	int32 Count = 0;
	for (const int32 Team : Subsystem ? Subsystem->GetAISlots() : TArray<int32>())
	{
		Count += Team == TeamId ? 1 : 0;
	}
	return Count;
}

void ULobbyAISlotsWidget::ChangeAICount(int32 TeamId, int32 Delta)
{
	UPlayerTeamSubsystem* Subsystem = GetTeamSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const int32 Current = GetAICount(TeamId);
	if (Delta > 0 && Current < MaxPerTeam)
	{
		Subsystem->SetAISlot(TeamId, true);
	}
	else if (Delta < 0 && Current > 0)
	{
		Subsystem->SetAISlot(TeamId, false);
	}
	UE_LOG(LogTemp, Log, TEXT("[Lobby] AI players of team %d: %d."), TeamId, GetAICount(TeamId));
	Rebuild();
}

void ULobbyAISlotsWidget::Rebuild()
{
	if (!Lines)
	{
		return;
	}
	Lines->ClearChildren();
	Proxies.Reset();
	for (const int32 Team : Teams)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
		LabelBox->SetWidthOverride(110.f);
		LabelBox->AddChild(LobbyText(WidgetTree, FText::Format(NSLOCTEXT("LobbyAISlots", "Team", "Team {0}"), FText::AsNumber(Team)), 14));
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(LabelBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		for (int32 Side = 0; Side < 3; ++Side)
		{
			if (Side == 1)
			{
				USizeBox* CountBox = WidgetTree->ConstructWidget<USizeBox>();
				CountBox->SetWidthOverride(40.f);
				UTextBlock* Count = LobbyText(WidgetTree, FText::AsNumber(GetAICount(Team)), 16);
				Count->SetJustification(ETextJustify::Center);
				CountBox->AddChild(Count);
				if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(CountBox))
				{
					S->SetVerticalAlignment(VAlign_Center);
				}
				continue;
			}
			UButton* Button = WidgetTree->ConstructWidget<UButton>();
			Button->AddChild(LobbyText(WidgetTree, FText::FromString(Side == 0 ? TEXT("-") : TEXT("+")), 16));
			ULobbyAISlotButtonProxy* Proxy = NewObject<ULobbyAISlotButtonProxy>(this);
			Proxy->Widget = this;
			Proxy->TeamId = Team;
			Proxy->Delta = Side == 0 ? -1 : 1;
			Proxies.Add(Proxy);
			Button->OnClicked.AddDynamic(Proxy, &ULobbyAISlotButtonProxy::HandleClicked);
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Button))
			{
				S->SetPadding(FMargin(2.f));
			}
		}
		Lines->AddChildToVerticalBox(Line);
	}
}
