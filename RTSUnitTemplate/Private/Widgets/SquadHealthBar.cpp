// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/SquadHealthBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "EngineUtils.h"
#include "UObject/ObjectKey.h"

namespace
{
	struct FSquadKey
	{
		FObjectKey WorldKey;
		int32 TeamId = 0;
		int32 SquadId = 0;

		bool operator==(const FSquadKey& Other) const
		{
			return WorldKey == Other.WorldKey && TeamId == Other.TeamId && SquadId == Other.SquadId;
		}
	};

	FORCEINLINE uint32 GetTypeHash(const FSquadKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.WorldKey);
		Hash = HashCombineFast(Hash, ::GetTypeHash(Key.TeamId));
		Hash = HashCombineFast(Hash, ::GetTypeHash(Key.SquadId));
		return Hash;
	}

	struct FSquadBaseline
	{
		float MaxHealth = 0.f;
		float MaxShield = 0.f;
		bool bInitialized = false;
	};

	static TMap<FSquadKey, FSquadBaseline> GSquadBaselines;
}

void USquadHealthBar::UpdateWidget()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<USquadHealthBar> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (USquadHealthBar* StrongThis = WeakThis.Get())
			{
				StrongThis->UpdateWidget();
			}
		});
		return;
	}

	if (!OwnerCharacter || !OwnerCharacter->IsValidLowLevel())
	{
		return;
	}

	float CurrentHealth = 0.f, MaxHealth = 0.f, CurrentShield = 0.f, MaxShield = 0.f;
	ComputeSquadHealth(CurrentHealth, MaxHealth, CurrentShield, MaxShield);

	if (HealthBar)
	{
		float HealthPercent = (MaxHealth > 0.f) ? (CurrentHealth / MaxHealth) : 0.f;
		HealthBar->SetPercent(HealthPercent);
	}

	FNumberFormattingOptions Opts; Opts.SetMaximumFractionalDigits(0);
	if (CurrentHealthLabel) CurrentHealthLabel->SetText(FText::AsNumber(CurrentHealth, &Opts));
	if (MaxHealthLabel) MaxHealthLabel->SetText(FText::AsNumber(MaxHealth, &Opts));

	if (ShieldBar)
	{
		if (CurrentShield <= 0.f)
		{
			ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ShieldBar->SetVisibility(ESlateVisibility::Visible);
		}
		float ShieldPercent = (MaxShield > 0.f) ? (CurrentShield / MaxShield) : 0.f;
		ShieldBar->SetPercent(ShieldPercent);
	}
	if (CurrentShieldLabel) CurrentShieldLabel->SetText(FText::AsNumber(CurrentShield, &Opts));
	if (MaxShieldLabel) MaxShieldLabel->SetText(FText::AsNumber(MaxShield, &Opts));

	// Level and XP remain tied to the owner's display (optional to aggregate)
	if (CharacterLevel)
	{
		CharacterLevel->SetText(FText::AsNumber(OwnerCharacter->LevelData.CharacterLevel, &Opts));
	}
	UpdateExperience();
}

void USquadHealthBar::ComputeSquadHealth(float& OutCurrentHealth, float& OutMaxHealth, float& OutCurrentShield, float& OutMaxShield) const
{
	OutCurrentHealth = 0.f; OutMaxHealth = 0.f; OutCurrentShield = 0.f; OutMaxShield = 0.f;
	if (!OwnerCharacter) return;

	const int Squad = OwnerCharacter->SquadId;
	const int Team = OwnerCharacter->TeamId;
	if (Squad <= 0) // Not a squad; fall back to single owner values
	{
		if (OwnerCharacter->Attributes)
		{
			OutCurrentHealth = OwnerCharacter->Attributes->GetHealth();
			OutMaxHealth = OwnerCharacter->Attributes->GetMaxHealth();
			OutCurrentShield = OwnerCharacter->Attributes->GetShield();
			OutMaxShield = OwnerCharacter->Attributes->GetMaxShield();
		}
		return;
	}

	UWorld* World = OwnerCharacter->GetWorld();
	if (!World) return;

	// Sum current values from alive/valid units
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!Unit || Unit->IsActorBeingDestroyed()) continue;
		if (Unit->TeamId != Team || Unit->SquadId != Squad) continue;
		if (!Unit->Attributes) continue;
		// Skip dead units to avoid counting stale health/shield
		if (Unit->GetUnitState() == UnitData::Dead) continue;

		const float UnitMax = FMath::Max(0.f, Unit->Attributes->GetMaxHealth());
		const float UnitHealth = FMath::Clamp(Unit->Attributes->GetHealth(), 0.f, UnitMax);
		OutCurrentHealth += UnitHealth;

		const float UnitShieldMax = FMath::Max(0.f, Unit->Attributes->GetMaxShield());
		const float UnitShield = FMath::Clamp(Unit->Attributes->GetShield(), 0.f, UnitShieldMax);
		OutCurrentShield += UnitShield;
	}

	// Use or initialize baseline max totals for this squad (do not shrink when members die)
	FSquadKey Key{ FObjectKey(World), Team, Squad };
	FSquadBaseline& Baseline = GSquadBaselines.FindOrAdd(Key);

	// If baseline was initialized to zeros (e.g., before replication), allow re-initialization later
	if (Baseline.bInitialized && Baseline.MaxHealth <= 0.f && Baseline.MaxShield <= 0.f)
	{
		Baseline.bInitialized = false;
	}

	float TempMaxHealth = 0.f;
	float TempMaxShield = 0.f;
	if (!Baseline.bInitialized)
	{
		// Initialize from current snapshot. We intentionally include all squad members
		// (even dead) so the baseline represents the full squad composition.
		for (TActorIterator<AUnitBase> It(World); It; ++It)
		{
			AUnitBase* Unit = *It;
			if (!Unit || Unit->IsActorBeingDestroyed()) continue;
			if (Unit->TeamId != Team || Unit->SquadId != Squad) continue;
			if (!Unit->Attributes) continue;
			TempMaxHealth += FMath::Max(0.f, Unit->Attributes->GetMaxHealth());
			TempMaxShield += FMath::Max(0.f, Unit->Attributes->GetMaxShield());
		}

		// Only lock in the baseline if we actually observed non-zero totals.
		if (TempMaxHealth > 0.f || TempMaxShield > 0.f)
		{
			Baseline.MaxHealth = TempMaxHealth;
			Baseline.MaxShield = TempMaxShield;
			Baseline.bInitialized = true;
		}
	}

	// Clamp current totals to the baseline to avoid showing >100%, but only if a valid baseline exists.
	if (Baseline.bInitialized)
	{
		OutCurrentHealth = FMath::Clamp(OutCurrentHealth, 0.f, Baseline.MaxHealth);
		OutCurrentShield = FMath::Clamp(OutCurrentShield, 0.f, Baseline.MaxShield);
	}

	// Report max values using the established baseline if available, otherwise fall back to the latest snapshot
	OutMaxHealth = Baseline.bInitialized ? Baseline.MaxHealth : TempMaxHealth;
	OutMaxShield = Baseline.bInitialized ? Baseline.MaxShield : TempMaxShield;
}
