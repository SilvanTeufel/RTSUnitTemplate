// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/AbilityTemplateSubsystem.h"

void UAbilityTemplateSubsystem::Merken(int32 TeamId, FGameplayTag TierTag, int32 Slot,
                                       EGASAbilityInputID AbilityID)
{
	if (!TierTag.IsValid() || Slot < 0 || Slot > 3)
	{
		return;
	}
	FTierAbilityTemplate& Eintrag = Vorlagen.FindOrAdd(TeamId).FindOrAdd(TierTag);
	Eintrag.Setze(Slot, AbilityID);

	UE_LOG(LogTemp, Log, TEXT("[Vorlage] Team %d %s Slot %d -> %d gemerkt."),
		TeamId, *TierTag.ToString(), Slot, (int32)AbilityID);
}

const FTierAbilityTemplate* UAbilityTemplateSubsystem::Hole(int32 TeamId,
                                                            FGameplayTag TierTag) const
{
	if (const TMap<FGameplayTag, FTierAbilityTemplate>* ProTeam = Vorlagen.Find(TeamId))
	{
		return ProTeam->Find(TierTag);
	}
	return nullptr;
}

void UAbilityTemplateSubsystem::SammleTags(int32 TeamId, TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	if (const TMap<FGameplayTag, FTierAbilityTemplate>* ProTeam = Vorlagen.Find(TeamId))
	{
		ProTeam->GetKeys(OutTags);
	}
}
