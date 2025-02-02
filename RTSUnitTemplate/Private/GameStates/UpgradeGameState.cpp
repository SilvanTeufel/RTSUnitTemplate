#include "GameStates/UpgradeGameState.h"
#include "Net/UnrealNetwork.h"

AUpgradeGameState::AUpgradeGameState()
{
    // Initialize the team upgrades array (empty array by default)
}

void AUpgradeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AUpgradeGameState, TeamUpgradesArray);
}

void AUpgradeGameState::OnRep_TeamUpgrades()
{
    // Handle UI updates or gameplay reactions when TeamUpgradesArray changes
}

void AUpgradeGameState::SetTeamUpgrades(int32 TeamId, const TArray<FUpgradeStatus>& Upgrades)
{
    if (HasAuthority() && TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        // Ensure the array is large enough
        if (TeamIndex >= TeamUpgradesArray.Num())
        {
            TeamUpgradesArray.SetNum(TeamIndex + 1);
        }

        TeamUpgradesArray[TeamIndex].Upgrades = Upgrades;
        TeamUpgradesArray[TeamIndex].MarkArrayDirty();
    }
}

void AUpgradeGameState::SetUpgradeResearched(int32 TeamId, int32 UpgradeIndex, bool bResearched)
{
    if (HasAuthority() && TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        if (TeamIndex < TeamUpgradesArray.Num() && UpgradeIndex < TeamUpgradesArray[TeamIndex].Upgrades.Num())
        {
            TeamUpgradesArray[TeamIndex].Upgrades[UpgradeIndex].Researched = bResearched;
            TeamUpgradesArray[TeamIndex].MarkItemDirty(TeamUpgradesArray[TeamIndex].Upgrades[UpgradeIndex]);
        }
    }
}

TSubclassOf<UGameplayEffect> AUpgradeGameState::GetUpgradeInvestmentEffect(int32 TeamId, const FString& UpgradeName) const
{
    if (TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        if (TeamIndex < TeamUpgradesArray.Num())
        {
            const FTeamUpgrades& TeamUpgrades = TeamUpgradesArray[TeamIndex];

            for (const FUpgradeStatus& Upgrade : TeamUpgrades.Upgrades)
            {
                if (Upgrade.Name.Equals(UpgradeName, ESearchCase::IgnoreCase))
                {
                    return Upgrade.InvestmentEffect;
                }
            }
        }
    }
    return nullptr;
}

void AUpgradeGameState::AddUpgradeToTeam(int32 TeamId, const FUpgradeStatus& Upgrade)
{
    if (HasAuthority() && TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        if (TeamIndex >= TeamUpgradesArray.Num())
        {
            TeamUpgradesArray.SetNum(TeamIndex + 1);
        }

        TeamUpgradesArray[TeamIndex].Upgrades.Add(Upgrade);
        TeamUpgradesArray[TeamIndex].MarkArrayDirty();
    }
}

bool AUpgradeGameState::GetUpgradeResearchedStatus(int32 TeamId, FString UpgradeName) const
{
    if (TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        if (TeamIndex < TeamUpgradesArray.Num())
        {
            for (const FUpgradeStatus& Upgrade : TeamUpgradesArray[TeamIndex].Upgrades)
            {
                if (Upgrade.Name.Equals(UpgradeName))
                {
                    return Upgrade.Researched;
                }
            }
        }
    }
    return false;
}

void AUpgradeGameState::ResearchUpgradeByName(int32 TeamId, FString UpgradeName)
{
    if (HasAuthority() && TeamId >= 1)
    {
        int32 TeamIndex = TeamId - 1;

        if (TeamIndex < TeamUpgradesArray.Num())
        {
            for (FUpgradeStatus& Upgrade : TeamUpgradesArray[TeamIndex].Upgrades)
            {
                if (Upgrade.Name.Equals(UpgradeName))
                {
                    Upgrade.Researched = true;
                    TeamUpgradesArray[TeamIndex].MarkItemDirty(Upgrade);
                    return;
                }
            }
        }
    }
}
