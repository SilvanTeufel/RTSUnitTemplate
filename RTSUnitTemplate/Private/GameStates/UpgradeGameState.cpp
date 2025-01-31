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

void AUpgradeGameState::SetTeamUpgrades(const TArray<FTeamUpgrades>& NewUpgrades)
{
    if (HasAuthority()) // Ensure this is only called on the server
    {
        TeamUpgradesArray = NewUpgrades;

        // Mark all upgrades in the array as dirty for replication
        for (FTeamUpgrades& TeamUpgrade : TeamUpgradesArray)
        {
            TeamUpgrade.MarkArrayDirty();
        }
    }
}

void AUpgradeGameState::SetUpgradeResearched(int32 TeamIndex, int32 UpgradeIndex, bool bResearched)
{
    if (HasAuthority() && TeamIndex < TeamUpgradesArray.Num())
    {
        FTeamUpgrades& TeamUpgrades = TeamUpgradesArray[TeamIndex];
        
        if (UpgradeIndex < TeamUpgrades.Upgrades.Num())
        {
            FUpgradeStatus& Upgrade = TeamUpgrades.Upgrades[UpgradeIndex];
            Upgrade.Researched = bResearched;

            // Mark the specific upgrade as dirty for replication
            TeamUpgrades.MarkItemDirty(Upgrade);
        }
    }
}

TSubclassOf<UGameplayEffect> AUpgradeGameState::GetUpgradeInvestmentEffect(int32 TeamIndex, int32 UpgradeIndex) const
{
    if (TeamIndex < TeamUpgradesArray.Num())
    {
        const FTeamUpgrades& TeamUpgrades = TeamUpgradesArray[TeamIndex];
        
        if (UpgradeIndex < TeamUpgrades.Upgrades.Num())
        {
            return TeamUpgrades.Upgrades[UpgradeIndex].InvestmentEffect;
        }
    }
    return nullptr;
}

void AUpgradeGameState::AddUpgradeToTeam(int32 TeamIndex, const FUpgradeStatus& Upgrade)
{
    if (HasAuthority() && TeamIndex < TeamUpgradesArray.Num())
    {
        FTeamUpgrades& TeamUpgrades = TeamUpgradesArray[TeamIndex];
        
        TeamUpgrades.Upgrades.Add(Upgrade);
        TeamUpgrades.MarkArrayDirty();
    }
}

bool AUpgradeGameState::GetUpgradeResearchedStatus(FString UpgradeName) const
{
    for (const FTeamUpgrades& TeamUpgrades : TeamUpgradesArray)
    {
        for (const FUpgradeStatus& Upgrade : TeamUpgrades.Upgrades)
        {
            if (Upgrade.Name.Equals(UpgradeName))
            {
                return Upgrade.Researched;
            }
        }
    }
    return false;
}

void AUpgradeGameState::ResearchUpgradeByName(FString UpgradeName)
{
    if (HasAuthority())
    {
        for (FTeamUpgrades& TeamUpgrades : TeamUpgradesArray)
        {
            for (FUpgradeStatus& Upgrade : TeamUpgrades.Upgrades)
            {
                if (Upgrade.Name.Equals(UpgradeName))
                {
                    Upgrade.Researched = true;
                    TeamUpgrades.MarkItemDirty(Upgrade);
                    return;
                }
            }
        }
    }
}
