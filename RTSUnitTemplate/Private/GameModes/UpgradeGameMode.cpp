#include "GameModes/UpgradeGameMode.h"
#include "GameStates/UpgradeGameState.h"
#include "Net/UnrealNetwork.h"

AUpgradeGameMode::AUpgradeGameMode()
{
    // Constructor code if needed
}

void AUpgradeGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Initialize upgrades when the game starts
    InitializeUpgradesForTeams();
}

void AUpgradeGameMode::InitializeUpgradesForTeams()
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        // Loop over the 8 teams and initialize upgrades for each team
        for (int32 TeamId = 1; TeamId <= 8; ++TeamId)
        {
            // Initialize all the upgrades for this team
            InitializeSingleUpgrade(TeamId, TEXT("Robotic Cleave"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Robotic Shield"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Rifle Multishot"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Sniper Aimed Shot"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Buggy Damage"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Tank Health"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Tank Shield"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Plasma Fire"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Drone Swarm"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Teleport"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Charge"), false, nullptr);
        }
    }
}

void AUpgradeGameMode::AddUpgrade(int32 TeamId, const FString& UpgradeName, TSubclassOf<UGameplayEffect> InvestmentEffect)
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        // Subtract 1 from TeamId to get the correct team index (TeamId - 1)
        int32 TeamIndex = TeamId - 1;
        
        // Check if the team index is valid
        if (TeamIndex < 0 || TeamIndex >= UpgradeGameState->TeamUpgradesArray.Num()) return;

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

        // Create new upgrade
        FUpgradeStatus NewUpgrade;
        NewUpgrade.Name = UpgradeName;
        NewUpgrade.Researched = false;
        NewUpgrade.InvestmentEffect = InvestmentEffect;

        // Add the upgrade to the correct team
        TeamUpgrades.Upgrades.Add(NewUpgrade);
        TeamUpgrades.MarkArrayDirty();
    }
}

void AUpgradeGameMode::ResearchUpgrade(int32 TeamId, const FString& UpgradeName)
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        // Subtract 1 from TeamId to get the correct team index (TeamId - 1)
        int32 TeamIndex = TeamId - 1;

        // Check if the team index is valid
        if (TeamIndex < 0 || TeamIndex >= UpgradeGameState->TeamUpgradesArray.Num()) return;

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

        // Search for the upgrade and mark it as researched
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

// New function to initialize a single upgrade for a team
void AUpgradeGameMode::InitializeSingleUpgrade(int32 TeamId, const FString& UpgradeName, bool bResearched, TSubclassOf<UGameplayEffect> InvestmentEffect)
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        // Subtract 1 from TeamId to get the correct team index (TeamId - 1)
        int32 TeamIndex = TeamId - 1;

        // Check if the team index is valid
        if (TeamIndex < 0 || TeamIndex >= UpgradeGameState->TeamUpgradesArray.Num()) return;

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

        // Create a new upgrade status
        FUpgradeStatus NewUpgrade;
        NewUpgrade.Name = UpgradeName;
        NewUpgrade.Researched = bResearched;
        NewUpgrade.InvestmentEffect = InvestmentEffect;

        // Add the upgrade to the correct team
        TeamUpgrades.Upgrades.Add(NewUpgrade);
        TeamUpgrades.MarkArrayDirty();
    }
}
