// ResourceAttributeSet.cpp

#include "GAS/ResourceAttributeSet.h"
#include "Net/UnrealNetwork.h"

UResourceAttributeSet::UResourceAttributeSet()
{
}



void UResourceAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Existing resource attributes
	DOREPLIFETIME(UResourceAttributeSet, PrimaryResource);
	DOREPLIFETIME(UResourceAttributeSet, SecondaryResource);
	DOREPLIFETIME(UResourceAttributeSet, TertiaryResource);
	DOREPLIFETIME(UResourceAttributeSet, RareResource);
	DOREPLIFETIME(UResourceAttributeSet, EpicResource);
	DOREPLIFETIME(UResourceAttributeSet, LegendaryResource);

	// Worker count attributes
	DOREPLIFETIME(UResourceAttributeSet, PrimaryWorkers);
	DOREPLIFETIME(UResourceAttributeSet, SecondaryWorkers);
	DOREPLIFETIME(UResourceAttributeSet, TertiaryWorkers);
	DOREPLIFETIME(UResourceAttributeSet, RareWorkers);
	DOREPLIFETIME(UResourceAttributeSet, EpicWorkers);
	DOREPLIFETIME(UResourceAttributeSet, LegendaryWorkers);
}

void UResourceAttributeSet::OnRep_PrimaryResource(const FGameplayAttributeData& OldPrimaryResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, PrimaryResource, OldPrimaryResource);
}

void UResourceAttributeSet::OnRep_SecondaryResource(const FGameplayAttributeData& OldSecondaryResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, SecondaryResource, OldSecondaryResource);
}

void UResourceAttributeSet::OnRep_TertiaryResource(const FGameplayAttributeData& OldTertiaryResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, TertiaryResource, OldTertiaryResource);
}

void UResourceAttributeSet::OnRep_RareResource(const FGameplayAttributeData& OldRareResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, RareResource, OldRareResource);
}

void UResourceAttributeSet::OnRep_EpicResource(const FGameplayAttributeData& OldEpicResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, EpicResource, OldEpicResource);
}

void UResourceAttributeSet::OnRep_LegendaryResource(const FGameplayAttributeData& OldLegendaryResource)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, LegendaryResource, OldLegendaryResource);
}


// Worker Count per Resource
void UResourceAttributeSet::OnRep_PrimaryWorkers(const FGameplayAttributeData& OldPrimaryWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, PrimaryWorkers, OldPrimaryWorkers);
}

void UResourceAttributeSet::OnRep_SecondaryWorkers(const FGameplayAttributeData& OldSecondaryWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, SecondaryWorkers, OldSecondaryWorkers);
}

void UResourceAttributeSet::OnRep_TertiaryWorkers(const FGameplayAttributeData& OldTertiaryWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, TertiaryWorkers, OldTertiaryWorkers);
}

void UResourceAttributeSet::OnRep_RareWorkers(const FGameplayAttributeData& OldRareWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, RareWorkers, OldRareWorkers);
}

void UResourceAttributeSet::OnRep_EpicWorkers(const FGameplayAttributeData& OldEpicWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, EpicWorkers, OldEpicWorkers);
}

void UResourceAttributeSet::OnRep_LegendaryWorkers(const FGameplayAttributeData& OldLegendaryWorkers)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UResourceAttributeSet, LegendaryWorkers, OldLegendaryWorkers);
}