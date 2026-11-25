// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GameModes/StartLoadoutComponent.h"
#include "GameModes/RTSGameModeBase.h"
#include "GameModes/ResourceGameMode.h"
#include "Components/StartLoadoutPlayerComponent.h"
#include "Widgets/StartLoadoutWidget.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Actors/WorkArea.h"
#include "Actors/Waypoint.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/AttributeSetBase.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "GameStates/UpgradeGameState.h"
#include "GAS/AutoAbility.h"

namespace
{
	// The music (cassette) keeps playing in the selection pause - the game stands, the music does not.
	void SetSelectionPauseMusic(UWorld* World, bool bKeepPlaying)
	{
		if (!World)
		{
			return;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get()))
			{
				PC->Client_SetPauseMusicKeepsPlaying(bKeepPlaying);
			}
		}
	}
}


namespace
{
	// The sets offered last time, per faction. A process-wide memory on purpose: "Restart Level" builds a new world
	// (and a new component), and the player wants different cards than the ones he just had.
	TMap<FName, TArray<FName>> LastOfferedSets;

	int32 CountCards(const FStartLoadoutSetRow& Set)
	{
		auto Has = [](const FStartLoadoutCard& Card) { return Card.UnitClass && Card.Count > 0 ? 1 : 0; };
		int32 Count = Has(Set.Workers) + Has(Set.Army) + Has(Set.Tech) + Has(Set.Production) + Has(Set.Support) + (Set.bResetCooldowns ? 1 : 0)
			+ (Set.AutoAbility.IsSet() ? 2 : 0);
		for (const FStartLoadoutCard& Extra : Set.ExtraArmy)
		{
			Count += Has(Extra);
		}
		return Count;
	}
}

bool FStartLoadoutAutoAbility::IsSet() const
{
	return Ability.Get() != nullptr && UnitTag.IsValid();
}

// Test helper: every offered set gets this bonus row (name in UStartLoadoutComponent::Bonuses). Empty = drawn at random.
static TAutoConsoleVariable<FString> CVarStartLoadoutForceBonus(TEXT("RTS.StartLoadout.ForceBonus"), TEXT(""),
	TEXT("Start loadout test: name of a bonus row every offered set gets (empty = random)."));

UStartLoadoutComponent::UStartLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WidgetClass = UStartLoadoutWidget::StaticClass();
}

void UStartLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UStartLoadoutComponent::OnControllersGathered()
{
	if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bGathered = true;
	// The players' line to the server now - it has replicated to them by the time the selection opens.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (PC && !PC->bIsAi)
		{
			UStartLoadoutPlayerComponent::FindOrAdd(PC);
		}
	}
	if (bBeginPending)
	{
		BeginSelection();
	}
}

void UStartLoadoutComponent::GetSetsForFaction(const UDataTable* Table, FName Faction, TArray<FName>& OutRowNames, TArray<FStartLoadoutSetRow>& OutRows) const
{
	if (!Table || Table->GetRowStruct() != FStartLoadoutSetRow::StaticStruct())
	{
		return;
	}
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FStartLoadoutSetRow* Row = reinterpret_cast<const FStartLoadoutSetRow*>(Pair.Value);
		if (Row && Row->Faction == Faction)
		{
			OutRowNames.Add(Pair.Key);
			FStartLoadoutSetRow& Set = OutRows.Add_GetRef(*Row);
			// Too many cards for the screen: drop the last extra army cards, then the support building. The offer and
			// the spawn both use this copy (FindOfferedRow), so the player gets exactly what the cards show.
			const int32 Before = CountCards(Set);
			while (MaxCardsPerSet > 0 && CountCards(Set) > MaxCardsPerSet)
			{
				if (Set.ExtraArmy.Num() > 0)
				{
					Set.ExtraArmy.Pop();
				}
				else if (Set.Support.UnitClass)
				{
					Set.Support = FStartLoadoutCard();
				}
				else
				{
					break;
				}
			}
			if (CountCards(Set) != Before)
			{
				UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] Set %s has %d cards, more than MaxCardsPerSet (%d) - offered with %d."),
					*Pair.Key.ToString(), Before, MaxCardsPerSet, CountCards(Set));
			}
		}
	}
}

const FStartLoadoutFaction* UStartLoadoutComponent::FindFactionForTeam(int32 TeamId) const
{
	for (const FStartLoadoutFaction& Faction : Factions)
	{
		if (Faction.TeamIds.Contains(TeamId))
		{
			return &Faction;
		}
	}
	// No explicit team: the faction whose main base the team already has.
	for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->TeamId != TeamId || !It->IsBase)
		{
			continue;
		}
		for (const FStartLoadoutFaction& Faction : Factions)
		{
			if (Faction.TeamIds.Num() == 0 && Faction.MainBase.UnitClass && It->IsA(Faction.MainBase.UnitClass))
			{
				return &Faction;
			}
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Selections
// ---------------------------------------------------------------------------------------------------------------

void UStartLoadoutComponent::BeginSelection()
{
	UWorld* World = GetWorld();
	if (!bEnabled || bStarted || !World || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (!bGathered)
	{
		bBeginPending = true;   // the teams are not known yet - OnControllersGathered starts it
		return;
	}
	// First the difficulty (the host chooses), then the cards.
	if (Mode == EStartLoadoutMode::StartLoadout && Difficulties.Num() > 0 && ChosenDifficulty == INDEX_NONE)
	{
		if (!bChoosingDifficulty)
		{
			OpenDifficultySelection();
		}
		return;
	}
	bStarted = true;
	if (Mode == EStartLoadoutMode::Autobattler)
	{
		OpenAutobattlerRound();
		return;
	}
	// The later rounds count from now - the match start (game time: a pause holds them back).
	for (int32 i = 0; i < ExtraRounds.Num(); ++i)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UStartLoadoutComponent::OpenExtraRound, i),
			FMath::Max(0.1f, ExtraRounds[i].AtSeconds), false);
	}
	FSelection Selection;
	Selection.Kind = ESelectionKind::Start;
	Selection.Table = CardSets;
	Selection.Offered = OfferedSets;
	Selection.Seconds = SelectionSeconds;
	Selection.Header = NSLOCTEXT("StartLoadout", "Header", "Choose your start");
	OpenSelection(Selection);
}

void UStartLoadoutComponent::OpenExtraRound(int32 Index)
{
	if (!ExtraRounds.IsValidIndex(Index))
	{
		return;
	}
	if (bSelecting)
	{
		QueuedExtraRounds.Add(Index);   // one selection at a time
		return;
	}
	const FStartLoadoutRound& Round = ExtraRounds[Index];
	FSelection Selection;
	Selection.Kind = ESelectionKind::Extra;
	Selection.Table = Round.CardSets ? Round.CardSets.Get() : CardSets.Get();
	Selection.Offered = Round.OfferedSets;
	Selection.Seconds = Round.SelectionSeconds;
	Selection.bBonusForAllUnits = Round.bBonusForAllUnits;
	Selection.Header = Round.Header;
	OpenSelection(Selection);
}

void UStartLoadoutComponent::OpenAutoAbilityRound()
{
	if (!AutoAbilityCardSets)
	{
		return;
	}
	if (bSelecting)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UStartLoadoutComponent::OpenAutoAbilityRound));
		}
		return;
	}
	FSelection Selection;
	Selection.Kind = ESelectionKind::AutoAbility;
	Selection.Table = AutoAbilityCardSets;
	Selection.Offered = AutoAbilityOfferedSets;
	Selection.Seconds = AutoAbilitySelectionSeconds;
	Selection.Header = AutoAbilityHeader;
	Selection.Hint = AutoAbilityHint;
	OpenSelection(Selection);
}

void UStartLoadoutComponent::ApplyAutoAbilityCard(int32 TeamId, const FStartLoadoutSetRow& Set)
{
	AUpgradeGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AUpgradeGameState>() : nullptr;
	if (!GameState || !Set.AutoAbility.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] team %d: auto ability card %s not applied (%s)."), TeamId, *Set.Title.ToString(),
			GameState ? TEXT("no ability or unit tag") : TEXT("no AUpgradeGameState"));
		return;
	}
	GameState->GrantTeamAutoAbility(TeamId, Set.AutoAbility.UnitTag, Set.AutoAbility.Ability, /*bFromCard=*/true);
}

void UStartLoadoutComponent::OpenAutobattlerRound()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (bSelecting)
	{
		World->GetTimerManager().SetTimer(RoundTimer, this, &UStartLoadoutComponent::OpenAutobattlerRound, 1.f, false);
		return;
	}
	++AutoRound;
	FSelection Selection;
	Selection.Kind = ESelectionKind::Autobattler;
	Selection.Table = CardSets;
	Selection.Offered = OfferedSets;
	Selection.Seconds = AutobattlerSelectionSeconds;
	Selection.Header = FText::Format(NSLOCTEXT("StartLoadout", "AutobattlerHeader", "Round {0} - choose a card"), FText::AsNumber(AutoRound));
	OpenSelection(Selection);
}

void UStartLoadoutComponent::DrawOffer(FName Faction, TArray<FName>& Names, TArray<FStartLoadoutSetRow>& Rows) const
{
	GetSetsForFaction(Active.Table, Faction, Names, Rows);
	// The autobattler has no use for a cooldown reset - every round brings fresh units.
	if (Active.Kind == ESelectionKind::Autobattler)
	{
		for (int32 i = Rows.Num() - 1; i >= 0; --i)
		{
			if (Rows[i].bResetCooldowns && !Rows[i].Army.UnitClass)
			{
				Rows.RemoveAt(i);
				Names.RemoveAt(i);
			}
		}
	}
	// Every player his own random draw - different army units where the table has enough (table order on screen).
	if (Active.Offered > 0 && Names.Num() > Active.Offered)
	{
		// An own stream, seeded from the clock: the C generator behind FMath::RandRange can be reset by a fixed seed
		// (ARTSGameModeBase::FesterZufallsStartwert, rts.ai.seed) - then every restart drew the same sets. With a
		// fixed seed set on purpose the draw stays reproducible.
		const ARTSGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARTSGameModeBase>() : nullptr;
		const bool bFixedSeed = GameMode && GameMode->FesterZufallsStartwert != 0;
		const FRandomStream Stream(bFixedSeed ? FMath::Rand() : (int32)(FPlatformTime::Cycles() ^ (uint32)FDateTime::Now().GetTicks() ^ (uint32)FMath::Rand()));

		TArray<int32> Shuffled;
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			Shuffled.Add(i);
		}
		for (int32 i = Shuffled.Num() - 1; i > 0; --i)
		{
			Shuffled.Swap(i, Stream.RandRange(0, i));
		}
		// The sets of the last offer to the back: a restart shows different cards as long as the table has enough.
		if (const TArray<FName>* Last = LastOfferedSets.Find(Faction))
		{
			TArray<int32> Fresh;
			TArray<int32> Seen;
			for (const int32 i : Shuffled)
			{
				(Last->Contains(Names[i]) ? Seen : Fresh).Add(i);
			}
			if (Fresh.Num() >= Active.Offered)
			{
				Shuffled = MoveTemp(Fresh);   // enough new ones: none of the old sets at all
			}
			else
			{
				Shuffled = MoveTemp(Fresh);
				Shuffled.Append(Seen);
			}
		}
		TArray<int32> Order;
		TSet<UClass*> Armies;
		for (int32 Pass = 0; Pass < 2 && Order.Num() < Active.Offered; ++Pass)
		{
			for (const int32 i : Shuffled)
			{
				// Different army units - or, for auto ability cards, different abilities - where the table has enough.
				UClass* Key = Rows[i].AutoAbility.IsSet() ? Rows[i].AutoAbility.Ability.Get() : Rows[i].Army.UnitClass.Get();
				if (Order.Num() < Active.Offered && !Order.Contains(i) && (Pass == 1 || !Armies.Contains(Key)))
				{
					Order.Add(i);
					Armies.Add(Key);
				}
			}
		}
		Order.Sort();
		TArray<FName> DrawnNames;
		TArray<FStartLoadoutSetRow> DrawnRows;
		for (const int32 i : Order)
		{
			DrawnNames.Add(Names[i]);
			DrawnRows.Add(Rows[i]);
		}
		Names = MoveTemp(DrawnNames);
		Rows = MoveTemp(DrawnRows);
	}
	if (Active.Kind == ESelectionKind::Start)
	{
		// Several players of one faction: the union of their offers.
		TArray<FName>& Last = LastOfferedSets.FindOrAdd(Faction);
		if (!LastOfferRenewed.Contains(Faction))
		{
			Last.Reset();
			LastOfferRenewed.Add(Faction);
		}
		for (const FName& Name : Names)
		{
			Last.AddUnique(Name);
		}
	}
	DrawBonuses(Rows);
}

void UStartLoadoutComponent::OpenSelection(const FSelection& Selection)
{
	UWorld* World = GetWorld();
	Active = Selection;
	Choices.Reset();
	LastOfferRenewed.Reset();
	Waiting.Reset();
	Offered.Reset();
	OfferedRows.Reset();
	TeamPicks.Reset();
	if (!World || !Selection.Table || Selection.Table->GetRowStruct() != FStartLoadoutSetRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] No card set table (row struct StartLoadoutSetRow) - nothing to choose."));
		return;
	}

	// The match is decided (lost or won): no more cards on top of the end screen.
	if (const ARTSGameModeBase* GameMode = World->GetAuthGameMode<ARTSGameModeBase>(); GameMode && GameMode->IsWinLoseTriggered())
	{
		UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Match decided - no card selection."));
		return;
	}
	{
		bool bAnyHumanStillPlaying = false;
		bool bAnyHuman = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
			if (PC && !PC->bIsAi)
			{
				bAnyHuman = true;
				// Out = decided, or (after the start) nothing left alive: the defeat check runs on a timer and a card
				// round opening in between paused the game on top of a lost match (19.11.2026).
				const ARTSGameModeBase* GM = World->GetAuthGameMode<ARTSGameModeBase>();
				const bool bWipedOut = Selection.Kind != ESelectionKind::Start && GM && GM->CountAliveUnitsForTeam(PC->SelectableTeamId) == 0;
				bAnyHumanStillPlaying |= !PC->bWinLoseResolved && !bWipedOut;
			}
		}
		if (bAnyHuman && !bAnyHumanStillPlaying)
		{
			UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Every player is out - no card selection."));
			return;
		}
	}

	// Alone there is no one to wait for: no countdown (bTimeLimitOnlyInMultiplayer).
	int32 Humans = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		Humans += PC && !PC->bIsAi && FindFactionForTeam(PC->SelectableTeamId) ? 1 : 0;
	}
	if (bTimeLimitOnlyInMultiplayer && Humans <= 1)
	{
		Active.Seconds = 0.f;
	}

	APlayerController* FirstHuman = nullptr;
	TSet<int32> TeamsWithPlayers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (!PC)
		{
			continue;
		}
		const FStartLoadoutFaction* Faction = FindFactionForTeam(PC->SelectableTeamId);
		if (!Faction)
		{
			UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s (team %d): no faction - no cards."), *PC->GetName(), PC->SelectableTeamId);
			continue;
		}
		TeamsWithPlayers.Add(PC->SelectableTeamId);
		TArray<FName> Names;
		TArray<FStartLoadoutSetRow> Rows;
		DrawOffer(Faction->Faction, Names, Rows);
		if (Names.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] Faction %s has no card sets in %s."), *Faction->Faction.ToString(), *GetNameSafe(Selection.Table));
			continue;
		}
		if (Selection.Kind == ESelectionKind::AutoAbility)
		{
			// The card text from the ability's values - halved cooldown when the team has it from the tree already.
			const AUpgradeGameState* GameState = World->GetGameState<AUpgradeGameState>();
			for (FStartLoadoutSetRow& Row : Rows)
			{
				const UAutoAbility* Ability = Row.AutoAbility.Ability ? Row.AutoAbility.Ability->GetDefaultObject<UAutoAbility>() : nullptr;
				if (Ability && Row.Description.IsEmpty())
				{
					const bool bTree = GameState && GameState->HasAutoAbilityFrom(PC->SelectableTeamId, Row.AutoAbility.Ability, false);
					Row.Description = Ability->GetAutoDescription(bTree ? 0.5f : 1.f);
				}
				if (Ability && Row.Title.IsEmpty())
				{
					Row.Title = Ability->GetAutoName();
				}
			}
		}
		Offered.Add(PC, Names);
		OfferedRows.Add(PC, Rows);
		if (PC->bIsAi)
		{
			const FName Pick = Names[FMath::RandRange(0, Names.Num() - 1)];
			Choices.Add(PC, Pick);
			UE_LOG(LogTemp, Log, TEXT("[StartLoadout] AI %s (team %d) takes %s at random."), *PC->GetName(), PC->SelectableTeamId, *Pick.ToString());
			continue;
		}
		UStartLoadoutPlayerComponent* Line = UStartLoadoutPlayerComponent::FindOrAdd(PC);
		if (!Line)
		{
			continue;
		}
		Waiting.Add(PC);
		FirstHuman = FirstHuman ? FirstHuman : PC;
		// The fixed main base card only belongs to the start selection.
		FStartLoadoutFaction Shown = *Faction;
		if (Selection.Kind != ESelectionKind::Start)
		{
			Shown.MainBase = FStartLoadoutCard();
		}
		Line->Client_OfferLoadout(WidgetClass, Shown, Names, Rows, Active.Seconds, Selection.Header, Selection.Hint);
		UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s (team %d, %s) chooses from %d set(s)."), *PC->GetName(), PC->SelectableTeamId,
			*Faction->Faction.ToString(), Names.Num());
	}

	// Autobattler teams nobody plays still take part - with a random card.
	if (Selection.Kind == ESelectionKind::Autobattler && bFillEmptyTeams)
	{
		for (const FStartLoadoutFaction& Faction : Factions)
		{
			for (const int32 TeamId : Faction.TeamIds)
			{
				if (TeamsWithPlayers.Contains(TeamId) || TeamPicks.Contains(TeamId))
				{
					continue;
				}
				TArray<FName> Names;
				TArray<FStartLoadoutSetRow> Rows;
				DrawOffer(Faction.Faction, Names, Rows);
				if (Rows.Num() > 0)
				{
					const int32 Pick = FMath::RandRange(0, Rows.Num() - 1);
					TeamPicks.Add(TeamId, Rows[Pick]);
					UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Team %d (no player) takes %s at random."), TeamId, *Names[Pick].ToString());
				}
			}
		}
	}

	if (Waiting.Num() == 0)
	{
		FinishSelection(TEXT("nobody has to choose"));
		return;
	}
	bSelecting = true;
	EndTime = Active.Seconds > 0.f ? FPlatformTime::Seconds() + Active.Seconds : 0.0;
	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (bPauseWhileChoosing && GameMode && FirstHuman && !World->IsPaused() && GameMode->AllowPausing(FirstHuman))
	{
		SetSelectionPauseMusic(World, true);
		bPaused = GameMode->SetPause(FirstHuman, FCanUnpause::CreateUObject(this, &UStartLoadoutComponent::CanUnpause));
	}
	// Real time: the world may stand still.
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return TickSelection(DeltaTime); }), 0.25f);
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Selection '%s' open: %d player(s) choose%s."), *Selection.Header.ToString(), Waiting.Num(),
		bPaused ? TEXT(", game paused") : TEXT(""));
}

bool UStartLoadoutComponent::CanUnpause() const
{
	return !bSelecting && !bChoosingDifficulty;
}

// ---------------------------------------------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------------------------------------------

void UStartLoadoutComponent::OpenDifficultySelection()
{
	UWorld* World = GetWorld();
	const int32 Default = Difficulties.IsValidIndex(DefaultDifficulty) ? DefaultDifficulty : Difficulties.Num() - 1;
	// The host chooses: the first human player (a listen server's own player comes first).
	ACameraControllerBase* Chooser = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It && !Chooser; ++It)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (PC && !PC->bIsAi && FindFactionForTeam(PC->SelectableTeamId))
		{
			Chooser = PC;
		}
	}
	UStartLoadoutPlayerComponent* Line = Chooser ? UStartLoadoutPlayerComponent::FindOrAdd(Chooser) : nullptr;
	if (!Line)
	{
		SetDifficulty(Default);
		BeginSelection();
		return;
	}
	bChoosingDifficulty = true;
	DifficultyChooser = Chooser;
	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (bPauseWhileChoosing && GameMode && !World->IsPaused() && GameMode->AllowPausing(Chooser))
	{
		SetSelectionPauseMusic(World, true);
		bPaused = GameMode->SetPause(Chooser, FCanUnpause::CreateUObject(this, &UStartLoadoutComponent::CanUnpause));
	}
	const FStartLoadoutFaction* ChooserFaction = FindFactionForTeam(Chooser->SelectableTeamId);
	Line->Client_OfferDifficulty(Difficulties, Default, DifficultyHeader, ChooserFaction ? ChooserFaction->CardMaterial.Get() : nullptr);
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s chooses the difficulty (%d level(s))."), *Chooser->GetName(), Difficulties.Num());
}

void UStartLoadoutComponent::ReceiveDifficulty(APlayerController* PC, int32 Index)
{
	if (!bChoosingDifficulty || !PC || PC != DifficultyChooser.Get() || !Difficulties.IsValidIndex(Index))
	{
		return;
	}
	bChoosingDifficulty = false;
	SetDifficulty(Index);
	if (UStartLoadoutPlayerComponent* Line = PC->FindComponentByClass<UStartLoadoutPlayerComponent>())
	{
		Line->Client_EndDifficulty();
	}
	// The pause stays for the card selection; lifted here when there is no card selection after all.
	BeginSelection();
	UWorld* World = GetWorld();
	if (!bSelecting && bPaused && World && World->GetAuthGameMode())
	{
		World->GetAuthGameMode()->ClearPause();
		bPaused = false;
		SetSelectionPauseMusic(World, false);
	}
}

void UStartLoadoutComponent::SetDifficulty(int32 Index)
{
	ChosenDifficulty = Index;
	UWorld* World = GetWorld();
	HumanTeams.Reset();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (PC && !PC->bIsAi)
		{
			HumanTeams.Add(PC->SelectableTeamId);
		}
	}
	const FStartLoadoutDifficulty& Level = Difficulties[Index];
	int32 Count = 0;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		if (IsValid(*It) && !HumanTeams.Contains(It->TeamId))
		{
			ApplyDifficultyTo(*It);
			++Count;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Difficulty '%s': enemy health x%.2f, damage x%.2f%s - %d unit(s) already on the field."),
		*Level.Name.ToString(), Level.EnemyHealthMultiplier, Level.EnemyDamageMultiplier, Level.EnemyEffect ? TEXT(", plus effect") : TEXT(""), Count);
}

void UStartLoadoutComponent::ApplyDifficultyTo(AUnitBase* Unit)
{
	if (!Difficulties.IsValidIndex(ChosenDifficulty) || !IsValid(Unit) || HumanTeams.Contains(Unit->TeamId) || DifficultyScaled.Contains(Unit))
	{
		return;
	}
	const FStartLoadoutDifficulty& Level = Difficulties[ChosenDifficulty];
	if (FMath::IsNearlyEqual(Level.EnemyHealthMultiplier, 1.f) && FMath::IsNearlyEqual(Level.EnemyDamageMultiplier, 1.f) && !Level.EnemyEffect)
	{
		return;
	}
	DifficultyScaled.Add(Unit);
	// After the Mass setup, which resets the attributes once more (like the start bonuses).
	TWeakObjectPtr<AUnitBase> WeakUnit(Unit);
	TWeakObjectPtr<const UStartLoadoutComponent> WeakThis(this);
	Unit->RunAfterAttributeInit([WeakUnit, WeakThis]()
	{
		if (WeakUnit.IsValid() && WeakThis.IsValid())
		{
			WeakThis->ScaleForDifficulty(WeakUnit.Get());
		}
	});
}

void UStartLoadoutComponent::ScaleForDifficulty(AUnitBase* Unit) const
{
	UAttributeSetBase* A = Unit->Attributes;
	if (!A || !Difficulties.IsValidIndex(ChosenDifficulty))
	{
		return;
	}
	const FStartLoadoutDifficulty& Level = Difficulties[ChosenDifficulty];
	// Like an investment: the healthbars must not take the attribute changes for damage or healing.
	++Unit->ActiveInvestments;
	ON_SCOPE_EXIT { --Unit->ActiveInvestments; };

	const float HealthShare = A->GetMaxHealth() > 0.f ? FMath::Clamp(A->GetHealth() / A->GetMaxHealth(), 0.f, 1.f) : 1.f;
	const float H = Level.EnemyHealthMultiplier;
	const float D = Level.EnemyDamageMultiplier;
	UE_LOG(LogTemp, Verbose, TEXT("[StartLoadout] Difficulty: %s (team %d) health %.0f -> %.0f, damage %.1f -> %.1f."), *Unit->GetName(),
		Unit->TeamId, A->GetMaxHealth(), A->GetMaxHealth() * H, A->GetAttackDamage(), A->GetAttackDamage() * D);
	A->SetMaxHealth(A->GetMaxHealth() * H);
	A->SetBaseHealth(A->GetBaseHealth() * H);
	A->SetHealth(FMath::Max(1.f, A->GetMaxHealth() * HealthShare));
	A->SetAttackDamage(A->GetAttackDamage() * D);
	A->SetBaseAttackDamage(A->GetBaseAttackDamage() * D);
	if (Level.EnemyEffect)
	{
		if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(Unit);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Level.EnemyEffect, 1.f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
}

void UStartLoadoutComponent::ReceiveChoice(APlayerController* PC, FName RowName)
{
	if (!bSelecting || !PC || !Waiting.Contains(PC))
	{
		return;
	}
	const TArray<FName>* Allowed = Offered.Find(PC);
	if (!Allowed || !Allowed->Contains(RowName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] %s chose %s - not one of the sets he was offered, ignored."), *PC->GetName(), *RowName.ToString());
		return;
	}
	Choices.Add(PC, RowName);
	Waiting.Remove(PC);
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s chose %s (%d still choosing)."), *PC->GetName(), *RowName.ToString(), Waiting.Num());
	if (Waiting.Num() == 0)
	{
		FinishSelection(TEXT("everyone has chosen"));
	}
}

bool UStartLoadoutComponent::TickSelection(float DeltaTime)
{
	if (!bSelecting)
	{
		Ticker.Reset();
		return false;
	}
	// Players who left do not hold the others up.
	Waiting.RemoveAll([](const TObjectPtr<APlayerController>& PC) { return !IsValid(PC); });
	if (Waiting.Num() == 0)
	{
		Ticker.Reset();
		FinishSelection(TEXT("everyone has chosen"));
		return false;
	}
	if (EndTime > 0.0 && FPlatformTime::Seconds() >= EndTime)
	{
		Ticker.Reset();
		FinishSelection(TEXT("time is up"));
		return false;
	}
	return true;
}

void UStartLoadoutComponent::FinishSelection(const TCHAR* Reason)
{
	UWorld* World = GetWorld();
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
	// Who has not chosen gets one of his sets at random.
	for (const TObjectPtr<APlayerController>& PC : Waiting)
	{
		const TArray<FName>* Allowed = Offered.Find(PC);
		if (Allowed && Allowed->Num() > 0)
		{
			Choices.Add(PC, (*Allowed)[FMath::RandRange(0, Allowed->Num() - 1)]);
		}
	}
	Waiting.Reset();
	bSelecting = false;
	if (bPaused && World && World->GetAuthGameMode())
	{
		World->GetAuthGameMode()->ClearPause();
		SetSelectionPauseMusic(World, false);
	}
	bPaused = false;
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Selection '%s' over (%s)."), *Active.Header.ToString(), Reason);

	CollectObstacles();
	for (const TPair<TObjectPtr<APlayerController>, FName>& Choice : Choices)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(Choice.Key);
		if (!IsValid(PC))
		{
			continue;
		}
		const FStartLoadoutFaction* Faction = FindFactionForTeam(PC->SelectableTeamId);
		const FStartLoadoutSetRow* Row = FindOfferedRow(PC, Choice.Value);
		if (Faction && Row)
		{
			switch (Active.Kind)
			{
			case ESelectionKind::Start: ApplyLoadout(PC, *Faction, *Row); break;
			case ESelectionKind::Extra: ApplyReinforcements(PC->SelectableTeamId, *Row); break;
			case ESelectionKind::Autobattler: Collected.FindOrAdd(PC->SelectableTeamId).Add(*Row); break;
			case ESelectionKind::AutoAbility: ApplyAutoAbilityCard(PC->SelectableTeamId, *Row); break;
			}
		}
		if (UStartLoadoutPlayerComponent* Line = PC->bIsAi ? nullptr : PC->FindComponentByClass<UStartLoadoutPlayerComponent>())
		{
			Line->Client_EndSelection(Choice.Value);
		}
	}
	for (const TPair<int32, FStartLoadoutSetRow>& Pick : TeamPicks)
	{
		Collected.FindOrAdd(Pick.Key).Add(Pick.Value);
	}
	Choices.Reset();
	Offered.Reset();
	OfferedRows.Reset();
	TeamPicks.Reset();

	if (Active.Kind == ESelectionKind::Autobattler && World)
	{
		SpawnAutobattlerWave();
		World->GetTimerManager().SetTimer(RoundTimer, this, &UStartLoadoutComponent::OpenAutobattlerRound, AutobattlerRoundSeconds, false);
	}
	// After the start cards and after every reinforcement round: the auto ability cards. The queued rounds wait
	// for them (OpenAutoAbilityRound's own FinishSelection opens the next one).
	const bool bAutoAbilityNext = (Active.Kind == ESelectionKind::Start || Active.Kind == ESelectionKind::Extra) && AutoAbilityCardSets && World;
	if (bAutoAbilityNext)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UStartLoadoutComponent::OpenAutoAbilityRound));
	}
	else if (QueuedExtraRounds.Num() > 0 && World)
	{
		const int32 Next = QueuedExtraRounds[0];
		QueuedExtraRounds.RemoveAt(0);
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UStartLoadoutComponent::OpenExtraRound, Next));
	}
}

const FStartLoadoutSetRow* UStartLoadoutComponent::FindOfferedRow(APlayerController* PC, FName RowName) const
{
	const TArray<FName>* Names = Offered.Find(PC);
	const TArray<FStartLoadoutSetRow>* Rows = OfferedRows.Find(PC);
	const int32 Index = Names ? Names->IndexOfByKey(RowName) : INDEX_NONE;
	if (Rows && Rows->IsValidIndex(Index))
	{
		return &(*Rows)[Index];
	}
	return Active.Table ? Active.Table->FindRow<FStartLoadoutSetRow>(RowName, TEXT("StartLoadout"), false) : nullptr;
}

void UStartLoadoutComponent::DrawBonuses(TArray<FStartLoadoutSetRow>& Rows) const
{
	if (!Bonuses || Bonuses->GetRowStruct() != FStartLoadoutBonusRow::StaticStruct())
	{
		return;
	}
	TArray<const FStartLoadoutBonusRow*> Pool;
	for (const TPair<FName, uint8*>& Pair : Bonuses->GetRowMap())
	{
		const FStartLoadoutBonusRow* Row = reinterpret_cast<const FStartLoadoutBonusRow*>(Pair.Value);
		if (Row && Row->Weight > 0.f && !Row->Bonus.IsEmpty())
		{
			Pool.Add(Row);
		}
	}
	const FString Forced = CVarStartLoadoutForceBonus.GetValueOnGameThread();
	const FStartLoadoutBonusRow* ForcedRow = Forced.IsEmpty() ? nullptr : Bonuses->FindRow<FStartLoadoutBonusRow>(FName(*Forced), TEXT("StartLoadout"), false);
	// Weighted, and no bonus twice on one offer while the pool lasts. A pure cooldown card stays as it is.
	for (FStartLoadoutSetRow& Set : Rows)
	{
		if (!Set.Bonus.IsEmpty() || (Set.bResetCooldowns && !Set.Army.UnitClass) || Set.AutoAbility.IsSet())
		{
			continue;
		}
		if (ForcedRow)
		{
			Set.Bonus = ForcedRow->Bonus;
			continue;
		}
		if (Pool.Num() == 0)
		{
			continue;
		}
		float Total = 0.f;
		for (const FStartLoadoutBonusRow* Row : Pool)
		{
			Total += Row->Weight;
		}
		float Roll = FMath::FRandRange(0.f, Total);
		int32 Pick = Pool.Num() - 1;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			Roll -= Pool[i]->Weight;
			if (Roll <= 0.f)
			{
				Pick = i;
				break;
			}
		}
		Set.Bonus = Pool[Pick]->Bonus;
		Pool.RemoveAt(Pick);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------------------------------------------

void UStartLoadoutComponent::ApplyBonus(AUnitBase* Unit, const FStartLoadoutBonus& Bonus, bool bFresh) const
{
	UAttributeSetBase* A = Unit ? Unit->Attributes : nullptr;
	if (!A || Bonus.IsEmpty())
	{
		return;
	}
	// Like an investment: the healthbars must not take the attribute changes for damage or healing.
	++Unit->ActiveInvestments;
	ON_SCOPE_EXIT { --Unit->ActiveInvestments; };

	const float HealthShare = A->GetMaxHealth() > 0.f ? FMath::Clamp(A->GetHealth() / A->GetMaxHealth(), 0.f, 1.f) : 1.f;
	const float ShieldBefore = A->GetShield();
	const float MaxShieldBefore = A->GetMaxShield();

	if (Bonus.Levels > 0)
	{
		Unit->RaiseToLevel(Unit->LevelData.CharacterLevel + Bonus.Levels);
	}
	const int32 Cap = Unit->LevelUpData.MaxTalentsPerStat;
	for (const FStartLoadoutStatPoints& Points : Bonus.StatPoints)
	{
		TSubclassOf<UGameplayEffect> Effect;
		FGameplayAttribute Attribute;
		switch (Points.Stat)
		{
		case EAttributeTreeStat::Stamina: Effect = Unit->StaminaInvestmentEffect; Attribute = UAttributeSetBase::GetStaminaAttribute(); break;
		case EAttributeTreeStat::AttackPower: Effect = Unit->AttackPowerInvestmentEffect; Attribute = UAttributeSetBase::GetAttackPowerAttribute(); break;
		case EAttributeTreeStat::Willpower: Effect = Unit->WillpowerInvestmentEffect; Attribute = UAttributeSetBase::GetWillpowerAttribute(); break;
		case EAttributeTreeStat::Haste: Effect = Unit->HasteInvestmentEffect; Attribute = UAttributeSetBase::GetHasteAttribute(); break;
		case EAttributeTreeStat::Armor: Effect = Unit->ArmorInvestmentEffect; Attribute = UAttributeSetBase::GetArmorAttribute(); break;
		default: Effect = Unit->MagicResistanceInvestmentEffect; Attribute = UAttributeSetBase::GetMagicResistanceAttribute(); break;
		}
		for (int32 i = 0; Effect && i < Points.Points && Attribute.GetNumericValue(A) < Cap; ++i)
		{
			Unit->ApplyInvestmentEffect(Effect);
		}
	}
	if (Bonus.HealthPercent != 0.f)
	{
		const float Add = A->GetMaxHealth() * Bonus.HealthPercent / 100.f;
		A->SetMaxHealth(A->GetMaxHealth() + Add);
		A->SetBaseHealth(A->GetBaseHealth() + Add);
	}
	if (Bonus.ShieldPercent != 0.f)
	{
		A->SetMaxShield(A->GetMaxShield() + A->GetMaxHealth() * Bonus.ShieldPercent / 100.f);
	}
	if (Bonus.HealthRegenPercent != 0.f)
	{
		A->SetHealthRegeneration(A->GetHealthRegeneration() + A->GetMaxHealth() * Bonus.HealthRegenPercent / 100.f);
	}
	if (Bonus.ShieldRegenPercent != 0.f)
	{
		A->SetShieldRegeneration(A->GetShieldRegeneration() + A->GetMaxShield() * Bonus.ShieldRegenPercent / 100.f);
	}
	if (Bonus.DamagePercent != 0.f)
	{
		const float Factor = 1.f + Bonus.DamagePercent / 100.f;
		A->SetAttackDamage(A->GetAttackDamage() * Factor);
		A->SetBaseAttackDamage(A->GetBaseAttackDamage() * Factor);
	}
	if (Bonus.SpeedPercent != 0.f)
	{
		const float Factor = 1.f + Bonus.SpeedPercent / 100.f;
		A->SetRunSpeed(A->GetRunSpeed() * Factor);
		A->SetBaseRunSpeed(A->GetBaseRunSpeed() * Factor);
	}
	if (Bonus.RangePercent != 0.f)
	{
		A->SetRange(A->GetRange() * (1.f + Bonus.RangePercent / 100.f));
	}
	if (bFresh)
	{
		// A fresh unit: full health and shield (a level-up keeps the old health, see bFullHealOnLevelUp).
		A->SetHealth(A->GetMaxHealth());
		A->SetShield(A->GetMaxShield());
	}
	else
	{
		// A unit in the fight keeps its share of health; new shield comes on top.
		A->SetHealth(FMath::Max(1.f, A->GetMaxHealth() * HealthShare));
		A->SetShield(FMath::Clamp(ShieldBefore + (A->GetMaxShield() - MaxShieldBefore), 0.f, A->GetMaxShield()));
	}
}

void UStartLoadoutComponent::ApplyBonusToSpawned(const TArray<AUnitBase*>& Units, const FStartLoadoutBonus& Bonus, int32 TeamId) const
{
	if (Bonus.IsEmpty())
	{
		return;
	}
	// After the Mass setup, which resets the attributes once more (AUnitBase::RunAfterAttributeInit).
	for (AUnitBase* Unit : Units)
	{
		TWeakObjectPtr<AUnitBase> WeakUnit(Unit);
		TWeakObjectPtr<const UStartLoadoutComponent> WeakThis(this);
		const bool bLog = Unit == Units.Last();
		Unit->RunAfterAttributeInit([WeakUnit, WeakThis, Bonus, bLog, TeamId]()
		{
			AUnitBase* U = WeakUnit.Get();
			if (!U || !WeakThis.IsValid())
			{
				return;
			}
			WeakThis->ApplyBonus(U, Bonus, true);
			if (bLog && U->Attributes)
			{
				const UAttributeSetBase* A = U->Attributes;
				UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d: bonus '%s' - e.g. %s now level %d, health %.0f/%.0f, shield %.0f, damage %.1f, armor %.0f, speed %.0f, range %.0f, regen %.2f/%.2f."),
					TeamId, *Bonus.Title.ToString(), *U->GetClass()->GetName(), U->LevelData.CharacterLevel, A->GetHealth(), A->GetMaxHealth(),
					A->GetMaxShield(), A->GetAttackDamage(), A->GetArmor(), A->GetRunSpeed(), A->GetRange(), A->GetHealthRegeneration(),
					A->GetShieldRegeneration());
			}
		});
	}
}

void UStartLoadoutComponent::ResetTeamCooldowns(int32 TeamId)
{
	int32 Removed = 0;
	int32 UnitsTouched = 0;
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* Unit = *It;
		UAbilitySystemComponent* ASC = IsValid(Unit) && Unit->TeamId == TeamId ? Unit->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			continue;
		}
		const int32 Before = Removed;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UGameplayAbility* Ability = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
			if (!Ability)
			{
				continue;
			}
			if (const FGameplayTagContainer* Tags = Ability->GetCooldownTags(); Tags && Tags->Num() > 0)
			{
				Removed += ASC->RemoveActiveEffectsWithGrantedTags(*Tags);
			}
			if (const UGameplayEffect* Cooldown = Ability->GetCooldownGameplayEffect())
			{
				FGameplayEffectQuery Query;
				Query.EffectDefinition = Cooldown->GetClass();
				Removed += ASC->RemoveActiveEffects(Query);
			}
		}
		UnitsTouched += Removed > Before ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d: %d cooldown(s) on %d unit(s) reset."), TeamId, Removed, UnitsTouched);
}

void UStartLoadoutComponent::ChargeSupply(AUnitBase* Unit) const
{
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetOwner());
	if (!bChargeSupply || !Unit || !ResourceGameMode || Unit->bIsBuilding || Unit->bIsConstructionUnit)
	{
		return;
	}
	// Like a level-placed unit (AUnitBase::ApplyStartupSupplyCost): billed now, handed back by ReleaseUnitSupply at death.
	const int32 Amount = Unit->StartupSupplyCost > 0 ? Unit->StartupSupplyCost : Unit->UnitSpaceNeeded;
	if (Amount <= 0)
	{
		return;
	}
	static const EResourceType AllTypes[] = {
		EResourceType::Primary, EResourceType::Secondary, EResourceType::Tertiary,
		EResourceType::Rare, EResourceType::Epic, EResourceType::Legendary };
	for (const EResourceType Type : AllTypes)
	{
		if (ResourceGameMode->IsSupplyLikeResource(Type))
		{
			ResourceGameMode->ModifyResource(Type, Unit->TeamId, -(float)Amount);
		}
	}
	Unit->ChargedSupplyAmount = Amount;
	Unit->bSupplyAmountKnown = true;
}

void UStartLoadoutComponent::ApplyReinforcements(int32 TeamId, const FStartLoadoutSetRow& Set)
{
	FVector Centre, Forward;
	float Radius = 300.f;
	if (!FindTeamAnchor(TeamId, Centre, Radius, Forward))
	{
		return;
	}
	const FVector Side(-Forward.Y, Forward.X, 0.f);
	TArray<AUnitBase*> NewUnits;
	PlaceUnits(Set.Workers, Centre + Side * (Radius + 350.f) - Forward * 300.f, Forward, TeamId, NewUnits);
	PlaceUnits(Set.Army, Centre + Forward * (Radius + 500.f), Forward, TeamId, NewUnits);
	PlaceExtraArmy(Set, Centre + Forward * (Radius + 500.f), Forward, TeamId, NewUnits);
	if (Set.bResetCooldowns)
	{
		ResetTeamCooldowns(TeamId);
	}
	ApplyBonusToSpawned(NewUnits, Set.Bonus, TeamId);
	if (!Set.Bonus.IsEmpty() && Active.bBonusForAllUnits)
	{
		// The whole army on the field (not buildings, not the dead).
		int32 Count = 0;
		for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
		{
			AUnitBase* Unit = *It;
			if (!IsValid(Unit) || Unit->TeamId != TeamId || Unit->bIsBuilding || Unit->bIsConstructionUnit || Unit->IsA<ABuildingBase>()
				|| NewUnits.Contains(Unit) || Unit->GetUnitState() == UnitData::Dead || !Unit->Attributes || Unit->Attributes->GetHealth() <= 0.f)
			{
				continue;
			}
			ApplyBonus(Unit, Set.Bonus, false);
			++Count;
		}
		UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d: bonus '%s' on %d unit(s) on the field."), TeamId, *Set.Bonus.Title.ToString(), Count);
	}
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d gets %s (%d new unit(s))%s."), TeamId, *Set.Title.ToString(), NewUnits.Num(),
		Set.bResetCooldowns ? TEXT(", cooldowns reset") : TEXT(""));
}

AWaypoint* UStartLoadoutComponent::GetEnemyWaypoint(int32 TeamId)
{
	if (const TObjectPtr<AWaypoint>* Found = EnemyWaypoints.Find(TeamId); Found && IsValid(*Found))
	{
		return *Found;
	}
	// The other teams' starts (their bases, if they have some).
	FVector Enemy = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<APlayerStartBase> It(GetWorld()); It; ++It)
	{
		if (It->SelectableTeamId != TeamId && FindFactionForTeam(It->SelectableTeamId))
		{
			Enemy += It->GetActorLocation();
			++Count;
		}
	}
	if (Count == 0)
	{
		return nullptr;
	}
	Enemy /= Count;
	FVector Nav;
	if (ProjectToNav(Enemy, Nav))
	{
		Enemy = Nav;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWaypoint* Waypoint = GetWorld()->SpawnActor<AWaypoint>(AWaypoint::StaticClass(), FTransform(Enemy + FVector(0, 0, 50.f)), Params);
	EnemyWaypoints.Add(TeamId, Waypoint);
	return Waypoint;
}

void UStartLoadoutComponent::SpawnAutobattlerWave()
{
	CollectObstacles();
	for (const TPair<int32, TArray<FStartLoadoutSetRow>>& Pair : Collected)
	{
		const int32 TeamId = Pair.Key;
		FVector Centre, Forward;
		float Radius = 300.f;
		if (!FindTeamAnchor(TeamId, Centre, Radius, Forward))
		{
			continue;
		}
		const FVector Side(-Forward.Y, Forward.X, 0.f);
		FSpawnOptions Options;
		Options.Waypoint = GetEnemyWaypoint(TeamId);
		Options.bSelectable = bAutobattlerUnitsSelectable;
		Options.State = AutobattlerUnitState;
		int32 Total = 0;
		for (int32 k = 0; k < Pair.Value.Num(); ++k)
		{
			// Every card its own block beside the others.
			const float Lane = (k % 2 == 0 ? 1.f : -1.f) * ((k + 1) / 2) * 900.f;
			TArray<AUnitBase*> Units;
			PlaceUnits(Pair.Value[k].Army, Centre + Forward * (Radius + 400.f) + Side * Lane, Forward, TeamId, Units, Options);
			PlaceExtraArmy(Pair.Value[k], Centre + Forward * (Radius + 400.f) + Side * Lane, Forward, TeamId, Units, Options);
			ApplyBonusToSpawned(Units, Pair.Value[k].Bonus, TeamId);
			Total += Units.Num();
		}
		UE_LOG(LogTemp, Log, TEXT("[StartLoadout] Autobattler round %d: team %d sends %d unit(s) from %d card(s) to %s."), AutoRound, TeamId,
			Total, Pair.Value.Num(), Options.Waypoint ? *Options.Waypoint->GetActorLocation().ToCompactString() : TEXT("-"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------------------------------------------

namespace
{
	/**
	 * Half the width of a unit or building on the ground: its mesh (the Mass units draw it through instances, so
	 * the actor's own bounds are often empty) or its capsule. With bClass the class default, before it is spawned.
	 */
	float UnitRadius(const AUnitBase* Unit, bool bClass)
	{
		float Radius = 0.f;
		const USceneComponent* Root = Unit->GetRootComponent();
		const FVector RootScale = Root ? (bClass ? Root->GetRelativeScale3D() : Root->GetComponentScale()).GetAbs() : FVector::OneVector;
		if (const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent())
		{
			Radius = Capsule->GetUnscaledCapsuleRadius() * FMath::Max(RootScale.X, RootScale.Y);
		}
		if (const UInstancedStaticMeshComponent* ISM = Unit->ISMComponent; ISM && ISM->GetStaticMesh())
		{
			const FVector Scale = bClass ? ISM->GetRelativeScale3D().GetAbs() * RootScale : ISM->GetComponentScale().GetAbs();
			const FBox Box = ISM->GetStaticMesh()->GetBoundingBox();
			// The mesh may sit off its pivot: the farthest corner from it counts.
			const float X = FMath::Max(FMath::Abs(Box.Min.X), FMath::Abs(Box.Max.X)) * Scale.X;
			const float Y = FMath::Max(FMath::Abs(Box.Min.Y), FMath::Abs(Box.Max.Y)) * Scale.Y;
			Radius = FMath::Max(Radius, FMath::Max(X, Y));
		}
		return Radius;
	}

	float FootprintRadius(const AActor* Actor)
	{
		if (const AUnitBase* Unit = Cast<AUnitBase>(Actor))
		{
			return UnitRadius(Unit, false);
		}
		FVector Origin, Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		return FMath::Max(Extent.X, Extent.Y);
	}

	const UCapsuleComponent* ClassCapsule(TSubclassOf<AUnitBase> Class)
	{
		const AUnitBase* CDO = Class ? Class->GetDefaultObject<AUnitBase>() : nullptr;
		return CDO ? CDO->GetCapsuleComponent() : nullptr;
	}
}

void UStartLoadoutComponent::CollectObstacles()
{
	Obstacles.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		// Buildings, resources and build spots - start buildings keep away from them; units move aside.
		if (Actor->IsA<ABuildingBase>() || Actor->IsA<AWorkArea>())
		{
			Obstacles.Add({ FVector2D(Actor->GetActorLocation()), FMath::Min(FootprintRadius(Actor), 3000.f) });
		}
	}
}

AUnitBase* UStartLoadoutComponent::FindMainBase(int32 TeamId, const FVector& Near) const
{
	ABuildingBase* Best = nullptr;
	for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->TeamId == TeamId && It->IsBase
			&& (!Best || FVector::DistSquared2D(It->GetActorLocation(), Near) < FVector::DistSquared2D(Best->GetActorLocation(), Near)))
		{
			Best = *It;
		}
	}
	return Best;
}

bool UStartLoadoutComponent::ProjectToNav(const FVector& P, FVector& Out) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Loc;
	if (Nav && Nav->ProjectPointToNavigation(P, Loc, FVector(150.f, 150.f, 2000.f)))
	{
		Out = Loc.Location;
		return true;
	}
	return false;
}

bool UStartLoadoutComponent::GroundAt(const FVector& P, FVector& Out) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(TEXT("StartLoadoutGround"), true);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 5000.f), P - FVector(0, 0, 10000.f), Objects, Params))
	{
		// Not on a building: those are the obstacles, not the ground.
		if (Hit.GetActor() && Hit.GetActor()->IsA<AUnitBase>())
		{
			return false;
		}
		Out = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool UStartLoadoutComponent::FindBuildingSpot(const FVector& Centre, float CentreRadius, const FVector& Back, float Radius, FVector& Out) const
{
	// Rings around the main base, the side away from the enemy first; on the base's own level first (not down a cliff).
	FVector BaseNav;
	const bool bBaseLevel = ProjectToNav(Centre, BaseNav);
	for (int32 Pass = bBaseLevel ? 0 : 1; Pass < 2; ++Pass)
	for (int32 Ring = 0; Ring < 6; ++Ring)
	{
		const float Dist = CentreRadius + BuildingGap + Radius + Ring * (Radius + BuildingGap) * 0.8f;
		const int32 Steps = FMath::Clamp(FMath::RoundToInt(2.f * PI * Dist / (Radius * 0.9f)), 8, 48);
		float BestScore = MAX_flt;
		bool bFound = false;
		for (int32 s = 0; s < Steps; ++s)
		{
			const float Angle = 2.f * PI * s / Steps;
			const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			FVector Nav, Ground;
			if (!ProjectToNav(Centre + Dir * Dist, Nav) || FVector::Dist2D(Nav, Centre + Dir * Dist) > Radius * 0.5f
				|| !GroundAt(Nav, Ground) || (Pass == 0 && FMath::Abs(Ground.Z - BaseNav.Z) > FMath::Min(MaxHeightDifference * 1.5f, SameLevelMaxHeightDifference)))
			{
				continue;
			}
			// Flat: the ground under the footprint within MaxHeightDifference, and navmesh all around (no cliff edge).
			bool bOk = true;
			for (int32 k = 0; k < 8 && bOk; ++k)
			{
				const float A = 2.f * PI * k / 8.f;
				const FVector Edge = Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius * 0.85f;
				FVector EdgeGround, EdgeNav;
				bOk = GroundAt(Edge, EdgeGround) && FMath::Abs(EdgeGround.Z - Ground.Z) <= MaxHeightDifference
					&& ProjectToNav(Edge, EdgeNav) && FVector::Dist2D(EdgeNav, Edge) < 150.f;
			}
			for (const FObstacle& O : Obstacles)
			{
				bOk = bOk && FVector2D::Distance(O.Position, FVector2D(Ground)) >= O.Radius + Radius + BuildingGap;
			}
			if (!bOk)
			{
				continue;
			}
			const float Score = -FVector::DotProduct(Dir, Back);
			if (Score < BestScore)
			{
				BestScore = Score;
				Out = Ground;
				bFound = true;
			}
		}
		if (bFound)
		{
			return true;
		}
	}
	return false;
}

AUnitBase* UStartLoadoutComponent::SpawnAt(const FStartLoadoutCard& Card, const FVector& Ground, int32 TeamId, const FSpawnOptions& Options) const
{
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetOwner());
	if (!GameMode || !Card.UnitClass)
	{
		return nullptr;
	}
	const AUnitBase* CDO = Card.UnitClass->GetDefaultObject<AUnitBase>();
	const UCapsuleComponent* Capsule = ClassCapsule(Card.UnitClass);
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 100.f;

	FUnitSpawnParameter Param;
	Param.UnitBaseClass = Card.UnitClass;
	Param.UnitCount = 1;
	// SpawnSingleUnit takes Z from UnitOffset alone.
	Param.UnitOffset = FVector(0.f, 0.f, Ground.Z + HalfHeight);
	Param.UnitMinRange = FVector::ZeroVector;
	Param.UnitMaxRange = FVector::ZeroVector;
	Param.State = Options.State != UnitData::None ? Options.State : (Card.State != UnitData::None ? Card.State.GetValue() : CDO->UnitState.GetValue());
	Param.StatePlaceholder = Options.State != UnitData::None ? Options.State : CDO->UnitStatePlaceholder.GetValue();
	Param.CanBeSelected = Options.bSelectable;
	AUnitBase* Unit = GameMode->SpawnSingleUnit(Param, FVector(Ground.X, Ground.Y, 0.f), nullptr, TeamId, Options.Waypoint);
	ChargeSupply(Unit);
	return Unit;
}

void UStartLoadoutComponent::PlaceExtraArmy(const FStartLoadoutSetRow& Set, const FVector& ArmyAnchor, const FVector& Forward, int32 TeamId,
	TArray<AUnitBase*>& OutUnits, const FSpawnOptions& Options) const
{
	// Left and right of the army, then further out.
	const FVector Side(-Forward.Y, Forward.X, 0.f);
	for (int32 i = 0; i < Set.ExtraArmy.Num(); ++i)
	{
		const float Offset = (i % 2 == 0 ? 1.f : -1.f) * (750.f + (i / 2) * 450.f);
		PlaceUnits(Set.ExtraArmy[i], ArmyAnchor + Side * Offset - Forward * 150.f, Forward, TeamId, OutUnits, Options);
	}
}

void UStartLoadoutComponent::PlaceUnits(const FStartLoadoutCard& Card, const FVector& Anchor, const FVector& Forward, int32 TeamId, TArray<AUnitBase*>& OutUnits,
	const FSpawnOptions& Options) const
{
	if (!Card.UnitClass || Card.Count <= 0)
	{
		return;
	}
	const UCapsuleComponent* Capsule = ClassCapsule(Card.UnitClass);
	const float Spacing = FMath::Max(UnitSpacing, Capsule ? Capsule->GetScaledCapsuleRadius() * 2.4f : 0.f);
	const FVector Right(-Forward.Y, Forward.X, 0.f);
	const int32 PerRow = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(float(Card.Count)) * 1.5f), 1, 8);
	int32 Spawned = 0;
	int32 OffNav = 0, Blocked = 0, OnGround = 0;
	TSet<int32> Used;
	// A grid in front of the anchor; spots off the navmesh (cliffs, water) are skipped. A second pass takes the plain
	// ground where the navmesh is missing - at match start it may still be building. While SameLevelZ is set (the start
	// of a match) both passes keep to the main base's level, so no unit starts on another plateau; only if that
	// leaves units without a spot, a last pass takes any level rather than losing them.
	int32 OtherLevel = 0, AnyLevel = 0;
	for (int32 Pass = 0; Pass < 3 && Spawned < Card.Count; ++Pass)
	{
		const bool bKeepLevel = SameLevelZ.IsSet() && Pass < 2;
		if (Pass == 2 && !SameLevelZ.IsSet())
		{
			break;
		}
		for (int32 Slot = 0; Slot < Card.Count * 8 && Spawned < Card.Count; ++Slot)
		{
			if (Used.Contains(Slot))
			{
				continue;
			}
			const int32 Row = Slot / PerRow;
			const int32 Col = Slot % PerRow;
			const FVector P = Anchor + Forward * (Row * Spacing) + Right * ((Col - (PerRow - 1) * 0.5f) * Spacing);
			FVector Spot;
			if (!ProjectToNav(P, Spot) || FVector::Dist2D(Spot, P) > Spacing * 0.5f)
			{
				if (Pass == 0 || !GroundAt(P, Spot))
				{
					OffNav += Pass == 0 ? 1 : 0;
					continue;
				}
				++OnGround;
			}
			if (bKeepLevel && FMath::Abs(Spot.Z - SameLevelZ.GetValue()) > SameLevelMaxHeightDifference)
			{
				OtherLevel += Pass == 0 ? 1 : 0;
				continue;
			}
			bool bFree = true;
			for (const FObstacle& O : Obstacles)
			{
				bFree = bFree && FVector2D::Distance(O.Position, FVector2D(Spot)) >= O.Radius + Spacing * 0.5f;
			}
			if (!bFree)
			{
				Blocked += Pass == 0 ? 1 : 0;
				continue;
			}
			if (AUnitBase* Unit = SpawnAt(Card, Spot, TeamId, Options))
			{
				Used.Add(Slot);
				OutUnits.Add(Unit);
				++Spawned;
				AnyLevel += Pass == 2 ? 1 : 0;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d: %d x %s (%d spot(s) off the navmesh, %d blocked, %d on plain ground, %d on another level skipped, %d placed on any level)."),
		TeamId, Spawned, *Card.UnitClass->GetName(), OffNav, Blocked, OnGround, OtherLevel, AnyLevel);
}

bool UStartLoadoutComponent::FindTeamAnchor(int32 TeamId, FVector& OutCentre, float& OutRadius, FVector& OutForward) const
{
	// The team's main base (else its start) and the direction to the enemy: the other teams' bases, else their starts.
	UWorld* World = GetWorld();
	bool bFound = false;
	FVector Start = FVector::ZeroVector;
	for (TActorIterator<APlayerStartBase> It(World); It; ++It)
	{
		if (It->SelectableTeamId == TeamId)
		{
			Start = It->GetActorLocation();
			bFound = true;
			break;
		}
	}
	const AUnitBase* MainBase = FindMainBase(TeamId, Start);
	if (!bFound && !MainBase)
	{
		return false;
	}
	OutCentre = MainBase ? MainBase->GetActorLocation() : Start;
	OutRadius = MainBase ? FMath::Min(FootprintRadius(MainBase), 2500.f) : 300.f;

	FVector Enemy = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->IsBase && It->TeamId != TeamId)
		{
			Enemy += It->GetActorLocation();
			++Count;
		}
	}
	if (Count == 0)
	{
		for (TActorIterator<APlayerStartBase> It(World); It; ++It)
		{
			if (It->SelectableTeamId != TeamId)
			{
				Enemy += It->GetActorLocation();
				++Count;
			}
		}
	}
	OutForward = Count > 0 ? (Enemy / Count - OutCentre).GetSafeNormal2D() : FVector::ZeroVector;
	if (OutForward.IsNearlyZero())
	{
		OutForward = FVector::ForwardVector;
	}
	return true;
}

void UStartLoadoutComponent::ApplyLoadout(ACameraControllerBase* PC, const FStartLoadoutFaction& Faction, const FStartLoadoutSetRow& Set)
{
	const int32 TeamId = PC->SelectableTeamId;
	UWorld* World = GetWorld();

	// The team's start and the direction to the enemy (the other teams' bases, else the middle of the map).
	FVector Start = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
	for (TActorIterator<APlayerStartBase> It(World); It; ++It)
	{
		if (It->SelectableTeamId == TeamId)
		{
			Start = It->GetActorLocation();
			break;
		}
	}
	AUnitBase* MainBase = FindMainBase(TeamId, Start);
	if (!MainBase && Faction.MainBase.UnitClass)
	{
		FVector Nav, Ground;
		if (ProjectToNav(Start, Nav) && GroundAt(Nav, Ground))
		{
			MainBase = SpawnAt(Faction.MainBase, Ground, TeamId);
			if (MainBase)
			{
				Obstacles.Add({ FVector2D(Ground), FootprintRadius(MainBase) });
			}
		}
	}
	const FVector Centre = MainBase ? MainBase->GetActorLocation() : Start;
	const float CentreRadius = MainBase ? FMath::Min(FootprintRadius(MainBase), 2500.f) : 300.f;

	// The start units stay on the main base's level (see PlaceUnits).
	SameLevelZ.Reset();
	{
		FVector BaseNav;
		if (ProjectToNav(Centre, BaseNav))
		{
			SameLevelZ = BaseNav.Z;
		}
	}
	ON_SCOPE_EXIT { SameLevelZ.Reset(); };

	FVector Enemy = FVector::ZeroVector;
	int32 EnemyCount = 0;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->IsBase && It->TeamId != TeamId)
		{
			Enemy += It->GetActorLocation();
			++EnemyCount;
		}
	}
	Enemy = EnemyCount > 0 ? Enemy / EnemyCount : FVector::ZeroVector;
	FVector Forward = (Enemy - Centre).GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	// Buildings behind the base.
	for (const FStartLoadoutCard* Card : { &Set.Support, &Set.Tech, &Set.Production })
	{
		if (!Card->UnitClass)
		{
			continue;
		}
		const float Radius = FMath::Max(BuildingRadius, UnitRadius(Card->UnitClass->GetDefaultObject<AUnitBase>(), true));
		for (int32 i = 0; i < FMath::Max(1, Card->Count); ++i)
		{
			FVector Spot;
			if (!FindBuildingSpot(Centre, CentreRadius, -Forward, Radius, Spot))
			{
				UE_LOG(LogTemp, Warning, TEXT("[StartLoadout] team %d: no free spot for %s."), TeamId, *Card->UnitClass->GetName());
				break;
			}
			if (AUnitBase* Building = SpawnAt(*Card, Spot, TeamId))
			{
				Obstacles.Add({ FVector2D(Spot), FMath::Max(Radius, FootprintRadius(Building)) });
				UE_LOG(LogTemp, Log, TEXT("[StartLoadout] team %d: %s at %s (radius %.0f, %.0f from the base of radius %.0f)."), TeamId,
					*Card->UnitClass->GetName(), *Spot.ToCompactString(), FootprintRadius(Building), FVector::Dist2D(Spot, Centre), CentreRadius);
			}
		}
	}

	// Workers beside the base, the army in front of it.
	const FVector Side(-Forward.Y, Forward.X, 0.f);
	TArray<AUnitBase*> Units;
	PlaceUnits(Set.Workers, Centre + Side * (CentreRadius + 350.f) - Forward * 300.f, Forward, TeamId, Units);
	PlaceUnits(Set.Army, Centre + Forward * (CentreRadius + 500.f), Forward, TeamId, Units);
	PlaceExtraArmy(Set, Centre + Forward * (CentreRadius + 500.f), Forward, TeamId, Units);

	// The set's start bonus on every unit of it.
	ApplyBonusToSpawned(Units, Set.Bonus, TeamId);
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s (team %d) starts with %s around %s at %s."), *PC->GetName(), TeamId, *Set.Title.ToString(),
		*GetNameSafe(MainBase), *Centre.ToCompactString());
}
