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
        // Ensure the array has enough teams
        UpgradeGameState->TeamUpgradesArray.SetNum(8); 

        // Loop over the 8 teams and initialize upgrades for each team
        for (int32 TeamId = 1; TeamId <= 8; ++TeamId)
        {
            InitializeSingleUpgrade(TeamId, TEXT("Robotic Cleave"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Robotic Shield"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Rifle Multishot"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Sniper Aimed Shot"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Buggy Damage"), false, nullptr);
            InitializeSingleUpgrade(TeamId, TEXT("Buggy Mine"), false, nullptr);
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
        int32 TeamIndex = TeamId - 1;
        
        // Ensure array size is correct before accessing
        if (TeamIndex < 0) return;
        UpgradeGameState->TeamUpgradesArray.SetNum(FMath::Max(UpgradeGameState->TeamUpgradesArray.Num(), TeamIndex + 1));

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

        FUpgradeStatus NewUpgrade;
        NewUpgrade.Name = UpgradeName;
        NewUpgrade.Researched = false;
        NewUpgrade.InvestmentEffect = InvestmentEffect;

        TeamUpgrades.Upgrades.Add(NewUpgrade);
        TeamUpgrades.MarkArrayDirty();
    }
}

void AUpgradeGameMode::ResearchUpgrade(int32 TeamId, const FString& UpgradeName)
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        int32 TeamIndex = TeamId - 1;
        
        if (TeamIndex < 0) return;
        UpgradeGameState->TeamUpgradesArray.SetNum(FMath::Max(UpgradeGameState->TeamUpgradesArray.Num(), TeamIndex + 1));

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

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

void AUpgradeGameMode::InitializeSingleUpgrade(int32 TeamId, const FString& UpgradeName, bool bResearched, TSubclassOf<UGameplayEffect> InvestmentEffect)
{
    AUpgradeGameState* UpgradeGameState = GetGameState<AUpgradeGameState>();
    if (UpgradeGameState)
    {
        int32 TeamIndex = TeamId - 1;
        
        if (TeamIndex < 0) return;
        UpgradeGameState->TeamUpgradesArray.SetNum(FMath::Max(UpgradeGameState->TeamUpgradesArray.Num(), TeamIndex + 1));

        FTeamUpgrades& TeamUpgrades = UpgradeGameState->TeamUpgradesArray[TeamIndex];

        FUpgradeStatus NewUpgrade;
        NewUpgrade.Name = UpgradeName;
        NewUpgrade.Researched = bResearched;
        NewUpgrade.InvestmentEffect = InvestmentEffect;

        TeamUpgrades.Upgrades.Add(NewUpgrade);
        TeamUpgrades.MarkArrayDirty();
    }
}
