// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "GameStates/UpgradeGameState.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

AUpgradeGameState::AUpgradeGameState()
{
    // Initialize the team upgrades array (empty array by default)
}

void AUpgradeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AUpgradeGameState, TeamUpgradesArray);
    DOREPLIFETIME(AUpgradeGameState, TeamAttributeTrees);
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

// --------------------------------------------------------------------------
//  Attributbaum je Team
// --------------------------------------------------------------------------

void AUpgradeGameState::OnRep_TeamAttributeTrees()
{
    // Das Widget liest den Topf bei jedem Zeichnen neu; hier ist nichts zu tun.
    // Der Haken bleibt stehen, weil eine spaetere Anzeige-Aktualisierung genau hier hingehoert.
}

const FTeamAttributeTree* AUpgradeGameState::FindTeamAttributeTree(int32 TeamId) const
{
    const int32 TeamIndex = TeamId - 1;
    return TeamAttributeTrees.IsValidIndex(TeamIndex) ? &TeamAttributeTrees[TeamIndex] : nullptr;
}

FTeamAttributeTree* AUpgradeGameState::FindOrAddTeamAttributeTree(int32 TeamId)
{
    if (!HasAuthority() || TeamId < 1)
    {
        return nullptr;
    }

    const int32 TeamIndex = TeamId - 1;
    if (TeamIndex >= TeamAttributeTrees.Num())
    {
        TeamAttributeTrees.SetNum(TeamIndex + 1);
    }
    return &TeamAttributeTrees[TeamIndex];
}

void AUpgradeGameState::GrantTeamAttributeTreePoints(int32 TeamId, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    if (FTeamAttributeTree* Tree = FindOrAddTeamAttributeTree(TeamId))
    {
        Tree->AvailablePoints += Count;
    }
}

int32 AUpgradeGameState::GetTeamAttributeTreePoints(int32 TeamId) const
{
    const FTeamAttributeTree* Tree = FindTeamAttributeTree(TeamId);
    return Tree ? Tree->AvailablePoints : 0;
}

int32 AUpgradeGameState::GetTeamUsedAttributeTreePoints(int32 TeamId) const
{
    const FTeamAttributeTree* Tree = FindTeamAttributeTree(TeamId);
    return Tree ? Tree->UsedPoints : 0;
}

int32 AUpgradeGameState::GetTeamAttributeTreeNodePoints(int32 TeamId, FName NodeId) const
{
    const FTeamAttributeTree* Tree = FindTeamAttributeTree(TeamId);
    if (!Tree)
    {
        return 0;
    }
    for (const FAttributeTreeNodeState& Node : Tree->Nodes)
    {
        if (Node.NodeId == NodeId)
        {
            return Node.Points;
        }
    }
    return 0;
}

TArray<FAttributeTreeNodeState> AUpgradeGameState::GetTeamAttributeTreeNodes(int32 TeamId) const
{
    const FTeamAttributeTree* Tree = FindTeamAttributeTree(TeamId);
    return Tree ? Tree->Nodes : TArray<FAttributeTreeNodeState>();
}

bool AUpgradeGameState::InvestTeamAttributeTreeNode(int32 TeamId, FName NodeId, int32 MaxPoints)
{
    FTeamAttributeTree* Tree = FindOrAddTeamAttributeTree(TeamId);
    if (!Tree || Tree->AvailablePoints <= 0 || NodeId.IsNone())
    {
        return false;
    }

    FAttributeTreeNodeState* Existing = nullptr;
    for (FAttributeTreeNodeState& Node : Tree->Nodes)
    {
        if (Node.NodeId == NodeId)
        {
            Existing = &Node;
            break;
        }
    }

    const int32 Current = Existing ? Existing->Points : 0;
    if (MaxPoints > 0 && Current >= MaxPoints)
    {
        return false;
    }

    if (Existing)
    {
        Existing->Points = Current + 1;
    }
    else
    {
        FAttributeTreeNodeState NewNode;
        NewNode.NodeId = NodeId;
        NewNode.Points = 1;
        Tree->Nodes.Add(NewNode);
    }

    Tree->AvailablePoints -= 1;
    Tree->UsedPoints += 1;
    return true;
}

void AUpgradeGameState::ResetTeamAttributeTree(int32 TeamId)
{
    FTeamAttributeTree* Tree = FindOrAddTeamAttributeTree(TeamId);
    if (!Tree)
    {
        return;
    }

    Tree->AvailablePoints += Tree->UsedPoints;
    Tree->UsedPoints = 0;
    Tree->Nodes.Reset();
}

const UDataTable* AUpgradeGameState::FindTeamAttributeTreeTable(int32 TeamId) const
{
    const UWorld* Welt = GetWorld();
    if (!Welt)
    {
        return nullptr;
    }
    for (TActorIterator<ALevelUnit> It(const_cast<UWorld*>(Welt)); It; ++It)
    {
        const ALevelUnit* Unit = *It;
        if (IsValid(Unit) && Unit->TeamId == TeamId && Unit->AttributeTreeDataTable)
        {
            return Unit->AttributeTreeDataTable;
        }
    }
    return nullptr;
}

bool AUpgradeGameState::IsTeamAttributeTreeNodeUnlocked(int32 TeamId, FName NodeId) const
{
    const UDataTable* Table = FindTeamAttributeTreeTable(TeamId);
    if (!Table)
    {
        return false;
    }

    const FAttributeTreeNodeRow* Row = Table->FindRow<FAttributeTreeNodeRow>(
        NodeId, TEXT("IsTeamAttributeTreeNodeUnlocked"), /*bWarnIfMissing=*/false);
    if (!Row)
    {
        return false;
    }
    if (Row->PrevId.IsNone())
    {
        return true; // Wurzelknoten
    }

    const FAttributeTreeNodeRow* Parent = Table->FindRow<FAttributeTreeNodeRow>(
        Row->PrevId, TEXT("IsTeamAttributeTreeNodeUnlocked"), /*bWarnIfMissing=*/false);
    if (!Parent)
    {
        return false;
    }
    return GetTeamAttributeTreeNodePoints(TeamId, Row->PrevId) >= FMath::Max(1, Parent->MaxPoints);
}
