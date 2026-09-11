// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GAS/GAS.h"
#include "AbilityTemplateSubsystem.generated.h"

/**
 * Die vier Faehigkeits-Slots einer Tierklasse, so wie der Spieler sie im AbilityChooser
 * gesetzt hat. None heisst "nicht vergeben".
 */
USTRUCT()
struct FTierAbilityTemplate
{
	GENERATED_BODY()

	EGASAbilityInputID Offensive = EGASAbilityInputID::None;

	EGASAbilityInputID Defensive = EGASAbilityInputID::None;

	EGASAbilityInputID Attack = EGASAbilityInputID::None;

	EGASAbilityInputID Throw = EGASAbilityInputID::None;

	/** Slot 0 Offensive, 1 Defensive, 2 Attack, 3 Throw - dieselbe Reihenfolge wie in
	 *  AAbilityUnit::SpendAbilityPoints. */
	EGASAbilityInputID Hole(int32 Slot) const
	{
		switch (Slot)
		{
		case 0:  return Offensive;
		case 1:  return Defensive;
		case 2:  return Attack;
		default: return Throw;
		}
	}

	void Setze(int32 Slot, EGASAbilityInputID Wert)
	{
		switch (Slot)
		{
		case 0:  Offensive = Wert; break;
		case 1:  Defensive = Wert; break;
		case 2:  Attack    = Wert; break;
		default: Throw     = Wert; break;
		}
	}
};

/**
 * Merkt sich je Team und Tierklasse, welche Faehigkeiten der Spieler gewaehlt hat.
 *
 * Hintergrund: der AbilityChooser vergab bisher nur an die Einheiten, die im Moment des
 * Klicks existierten. Jede spaeter gebaute Einheit stand wieder ohne da und musste einzeln
 * angeklickt werden. Die Vorlage hier wird von UAbilityTemplateProcessor alle paar Sekunden
 * auf neue Einheiten angewandt.
 *
 * Bewusst ein WorldSubsystem und kein Feld am Controller: der Prozessor laeuft ueber Mass und
 * hat keinen Controller zur Hand, und die Vorlage soll eine Partie lang gelten - nicht laenger.
 */
UCLASS()
class RTSUNITTEMPLATE_API UAbilityTemplateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Haelt eine Wahl fest. Slot wie in SpendAbilityPoints (0-3). */
	void Merken(int32 TeamId, FGameplayTag TierTag, int32 Slot, EGASAbilityInputID AbilityID);

	/** Die Vorlage der Tierklasse, oder nullptr wenn der Spieler dort noch nichts gewaehlt hat. */
	const FTierAbilityTemplate* Hole(int32 TeamId, FGameplayTag TierTag) const;

	/** Alle bekannten Tags eines Teams - der Prozessor prueft damit die Tags einer Einheit. */
	void SammleTags(int32 TeamId, TArray<FGameplayTag>& OutTags) const;

private:
	/** Team -> Tierklassen-Tag -> Vorlage. */
	TMap<int32, TMap<FGameplayTag, FTierAbilityTemplate>> Vorlagen;
};
