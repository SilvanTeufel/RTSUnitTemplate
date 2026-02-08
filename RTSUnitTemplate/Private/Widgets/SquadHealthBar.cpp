// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/SquadHealthBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "EngineUtils.h"

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

	// Sum current values from alive/valid units only
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!Unit || Unit->IsActorBeingDestroyed()) continue;
		if (Unit->TeamId != Team || Unit->SquadId != Squad) continue;
		
		// Recalculate based on remaining units: skip dead units for both Current and Max totals
		if (Unit->GetUnitState() == UnitData::Dead) continue;
		
		if (!Unit->Attributes) continue;

		const float UnitMaxH = FMath::Max(0.f, Unit->Attributes->GetMaxHealth());
		const float UnitMaxS = FMath::Max(0.f, Unit->Attributes->GetMaxShield());
		
		OutMaxHealth += UnitMaxH;
		OutMaxShield += UnitMaxS;

		OutCurrentHealth += FMath::Clamp(Unit->Attributes->GetHealth(), 0.f, UnitMaxH);
		OutCurrentShield += FMath::Clamp(Unit->Attributes->GetShield(), 0.f, UnitMaxS);
	}
}
