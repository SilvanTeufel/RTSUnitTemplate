// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/AutoAbilitySubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "GameStates/UpgradeGameState.h"
#include "GameModes/RTSGameModeBase.h"
#include "GameModes/StartLoadoutComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	bool IsAliveUnit(const AUnitBase* Unit)
	{
		return IsValid(Unit) && Unit->Attributes && Unit->GetUnitState() != UnitData::Dead && Unit->Attributes->GetHealth() > 0.f;
	}
}

bool UAutoAbilitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAutoAbilitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() != NM_Client)
	{
		InWorld.GetTimerManager().SetTimer(SyncTimer, FTimerDelegate::CreateUObject(this, &UAutoAbilitySubsystem::TickSync), 1.f, true);
	}
}

void UAutoAbilitySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SyncTimer);
	}
	Units.Reset();
	Super::Deinitialize();
}

AUpgradeGameState* UAutoAbilitySubsystem::GetGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AUpgradeGameState>() : nullptr;
}

void UAutoAbilitySubsystem::GetPoolForUnit(const AUnitBase* Unit, TArray<TSubclassOf<UAutoAbility>>& OutPool) const
{
	const ARTSGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARTSGameModeBase>() : nullptr;
	const UStartLoadoutComponent* Loadout = GameMode ? GameMode->StartLoadout.Get() : nullptr;
	const UDataTable* Table = Loadout ? Loadout->AutoAbilityCardSets.Get() : nullptr;
	if (!Unit || Unit->bNoAutoAbilities || !Table || Table->GetRowStruct() != FStartLoadoutSetRow::StaticStruct())
	{
		return;
	}
	const FStartLoadoutFaction* Faction = Loadout->FindFactionForTeam(Unit->TeamId);
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FStartLoadoutSetRow* Row = reinterpret_cast<const FStartLoadoutSetRow*>(Pair.Value);
		if (!Row || !Row->AutoAbility.IsSet() || (Faction && Row->Faction != Faction->Faction))
		{
			continue;
		}
		if (Unit->UnitTags.HasTagExact(Row->AutoAbility.UnitTag))
		{
			OutPool.AddUnique(Row->AutoAbility.Ability);
		}
	}
}

void UAutoAbilitySubsystem::RebuildFromTeam(AUnitBase* Unit, FUnitEntry& Entry, const AUpgradeGameState* GameState) const
{
	TArray<TSubclassOf<UAutoAbility>> FromTeam;
	if (GameState && !Unit->bNoAutoAbilities)
	{
		for (const FTeamAutoAbilityGrant& Grant : GameState->TeamAutoAbilityGrants)
		{
			if (Grant.TeamId == Unit->TeamId && Grant.Ability && (Grant.bFromCard || Grant.bFromTree) && Unit->UnitTags.HasTagExact(Grant.UnitTag))
			{
				FromTeam.AddUnique(Grant.Ability);
			}
		}
	}
	// Team abilities that are gone (tree reset) go, level abilities stay.
	for (int32 i = Entry.Abilities.Num() - 1; i >= 0; --i)
	{
		FAbilityState& State = Entry.Abilities[i];
		State.bFromTeam = FromTeam.Contains(State.Ability);
		if (!State.bFromTeam && !State.bFromLevel)
		{
			Entry.Abilities.RemoveAt(i);
		}
	}
	for (const TSubclassOf<UAutoAbility>& Ability : FromTeam)
	{
		if (!Entry.Abilities.ContainsByPredicate([&Ability](const FAbilityState& S) { return S.Ability == Ability; }))
		{
			FAbilityState& State = Entry.Abilities.AddDefaulted_GetRef();
			State.Ability = Ability;
			State.bFromTeam = true;
		}
	}
	for (FAbilityState& State : Entry.Abilities)
	{
		State.CooldownMultiplier = GameState ? GameState->GetAutoAbilityCooldownMultiplier(Unit->TeamId, State.Ability) : 1.f;
	}
	Entry.TeamVersion = GameState ? GameState->AutoAbilityVersion : 0;
	PublishStatus(Unit, Entry);
}

void UAutoAbilitySubsystem::PublishStatus(AUnitBase* Unit, const FUnitEntry& Entry) const
{
	TArray<FAutoAbilityStatus> Status;
	for (const FAbilityState& State : Entry.Abilities)
	{
		FAutoAbilityStatus& S = Status.AddDefaulted_GetRef();
		S.Ability = State.Ability;
		S.ReadyAt = (float)State.ReadyAt;
		S.CooldownMultiplier = State.CooldownMultiplier;
	}
	if (Unit && Unit->AutoAbilityStatus != Status)
	{
		Unit->AutoAbilityStatus = MoveTemp(Status);
		Unit->ForceNetUpdate();
	}
}

void UAutoAbilitySubsystem::TickSync()
{
	UWorld* World = GetWorld();
	AUpgradeGameState* GameState = GetGameState();
	if (!World || !GameState)
	{
		return;
	}
	TSet<int32> TeamsWithGrants;
	for (const FTeamAutoAbilityGrant& Grant : GameState->TeamAutoAbilityGrants)
	{
		TeamsWithGrants.Add(Grant.TeamId);
	}
	TSet<int32> TreeTeams;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsAliveUnit(Unit))
		{
			continue;
		}
		if (Unit->AttributeTreeDataTable && !TreeTeams.Contains(Unit->TeamId))
		{
			TreeTeams.Add(Unit->TeamId);
			GameState->EnsureTreeAutoAbilityPicks(Unit->TeamId);
		}
		FUnitEntry* Entry = Units.Find(Unit);
		if (!Entry)
		{
			if (!TeamsWithGrants.Contains(Unit->TeamId))
			{
				continue;
			}
			Entry = &Units.Add(Unit);
		}
		if (Entry->TeamVersion != GameState->AutoAbilityVersion)
		{
			RebuildFromTeam(Unit, *Entry, GameState);
		}
		for (FAbilityState& State : Entry->Abilities)
		{
			const UAutoAbility* CDO = State.Ability ? State.Ability->GetDefaultObject<UAutoAbility>() : nullptr;
			if (CDO && CDO->Trigger == EAutoAbilityTrigger::Periodic)
			{
				TryFire(Unit, *Entry, State, nullptr);
			}
		}
	}
	for (auto It = Units.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool UAutoAbilitySubsystem::TryFire(AUnitBase* Unit, FUnitEntry& Entry, FAbilityState& State, AUnitBase* Other, bool bIgnoreCooldown)
{
	UWorld* World = GetWorld();
	const UAutoAbility* CDO = State.Ability ? State.Ability->GetDefaultObject<UAutoAbility>() : nullptr;
	if (!World || !CDO)
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (!bIgnoreCooldown)
	{
		if (Now < State.ReadyAt)
		{
			return false;
		}
		if (CDO->TriggerChance < 1.f && FMath::FRand() > CDO->TriggerChance)
		{
			return false;
		}
	}
	if (!CDO->Execute(Unit, Other))
	{
		return false;
	}
	State.ReadyAt = Now + CDO->Cooldown * State.CooldownMultiplier;
	PublishStatus(Unit, Entry);
	UE_LOG(LogTemp, Log, TEXT("[AutoAbility] %s (team %d) -> %s, next in %.1f s%s."), *Unit->GetName(), Unit->TeamId, *CDO->GetClass()->GetName(),
		CDO->Cooldown * State.CooldownMultiplier, State.CooldownMultiplier < 0.999f ? TEXT(" (halved)") : TEXT(""));
	return true;
}

void UAutoAbilitySubsystem::NotifyTrigger(AUnitBase* Unit, EAutoAbilityTrigger Trigger, AUnitBase* Other)
{
	if (!IsAliveUnit(Unit) || !Unit->HasAuthority())
	{
		return;
	}
	UAutoAbilitySubsystem* Self = Unit->GetWorld() ? Unit->GetWorld()->GetSubsystem<UAutoAbilitySubsystem>() : nullptr;
	FUnitEntry* Entry = Self ? Self->Units.Find(Unit) : nullptr;
	if (!Entry)
	{
		return;
	}
	const float HealthShare = Unit->Attributes->GetMaxHealth() > 0.f ? Unit->Attributes->GetHealth() / Unit->Attributes->GetMaxHealth() : 1.f;
	for (FAbilityState& State : Entry->Abilities)
	{
		const UAutoAbility* CDO = State.Ability ? State.Ability->GetDefaultObject<UAutoAbility>() : nullptr;
		if (!CDO)
		{
			continue;
		}
		bool bMatch = false;
		switch (Trigger)
		{
		case EAutoAbilityTrigger::OnAttack:
			bMatch = CDO->Trigger == EAutoAbilityTrigger::OnAttack || CDO->Trigger == EAutoAbilityTrigger::OnEngage;
			break;
		case EAutoAbilityTrigger::OnEngage:
			bMatch = CDO->Trigger == EAutoAbilityTrigger::OnEngage;
			break;
		case EAutoAbilityTrigger::OnHit:
			bMatch = CDO->Trigger == EAutoAbilityTrigger::OnHit
				|| (CDO->Trigger == EAutoAbilityTrigger::LowHealth && HealthShare * 100.f < CDO->LowHealthPercent);
			break;
		default:
			bMatch = CDO->Trigger == Trigger;
			break;
		}
		if (bMatch)
		{
			Self->TryFire(Unit, *Entry, State, Other);
			if (!IsAliveUnit(Unit))
			{
				return;
			}
		}
	}
}

bool UAutoAbilitySubsystem::SyncLevelAbilities(AUnitBase* Unit, int32 Milestones)
{
	if (!IsAliveUnit(Unit) || !Unit->HasAuthority())
	{
		return true;
	}
	FUnitEntry* Entry = Units.Find(Unit);
	const int32 Have = Entry ? Entry->LevelAbilities : 0;
	if (Milestones <= Have)
	{
		return true;
	}
	TArray<TSubclassOf<UAutoAbility>> Pool;
	GetPoolForUnit(Unit, Pool);
	if (Pool.Num() == 0)
	{
		return false;
	}
	if (!Entry)
	{
		Entry = &Units.Add(Unit);
	}
	const AUpgradeGameState* GameState = GetGameState();
	while (Entry->LevelAbilities < Milestones)
	{
		++Entry->LevelAbilities;
		TArray<TSubclassOf<UAutoAbility>> Fresh;
		for (const TSubclassOf<UAutoAbility>& Ability : Pool)
		{
			if (!Entry->Abilities.ContainsByPredicate([&Ability](const FAbilityState& S) { return S.Ability == Ability; }))
			{
				Fresh.Add(Ability);
			}
		}
		if (Fresh.Num() == 0)
		{
			continue;   // it has every ability of its type already
		}
		FAbilityState& State = Entry->Abilities.AddDefaulted_GetRef();
		State.Ability = Fresh[FMath::RandRange(0, Fresh.Num() - 1)];
		State.bFromLevel = true;
		State.CooldownMultiplier = GameState ? GameState->GetAutoAbilityCooldownMultiplier(Unit->TeamId, State.Ability) : 1.f;
		UE_LOG(LogTemp, Log, TEXT("[AutoAbility] %s (team %d) level %d: new auto ability %s."), *Unit->GetName(), Unit->TeamId,
			Unit->LevelData.CharacterLevel, *State.Ability->GetName());
	}
	PublishStatus(Unit, *Entry);
	return true;
}

TArray<TSubclassOf<UAutoAbility>> UAutoAbilitySubsystem::GetUnitAutoAbilities(AUnitBase* Unit) const
{
	TArray<TSubclassOf<UAutoAbility>> Out;
	if (const FUnitEntry* Entry = Units.Find(Unit))
	{
		for (const FAbilityState& State : Entry->Abilities)
		{
			Out.Add(State.Ability);
		}
	}
	return Out;
}

float UAutoAbilitySubsystem::GetCooldownRemaining(AUnitBase* Unit, TSubclassOf<UAutoAbility> Ability) const
{
	if (const FUnitEntry* Entry = Units.Find(Unit))
	{
		for (const FAbilityState& State : Entry->Abilities)
		{
			if (State.Ability == Ability)
			{
				return FMath::Max(0.f, (float)(State.ReadyAt - GetWorld()->GetTimeSeconds()));
			}
		}
	}
	return -1.f;
}

bool UAutoAbilitySubsystem::ForceTrigger(AUnitBase* Unit, TSubclassOf<UAutoAbility> Ability, AUnitBase* Other)
{
	FUnitEntry* Entry = Units.Find(Unit);
	if (!Entry)
	{
		return false;
	}
	for (FAbilityState& State : Entry->Abilities)
	{
		if (State.Ability == Ability)
		{
			return TryFire(Unit, *Entry, State, Other, true);
		}
	}
	return false;
}
