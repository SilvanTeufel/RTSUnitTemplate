#pragma once

#include "CoreMinimal.h"
#include "GameStates/ResourceGameState.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UpgradeGameState.generated.h"

USTRUCT(BlueprintType)
struct FUpgradeStatus : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    bool Researched = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    TSubclassOf<UGameplayEffect> InvestmentEffect;
};

USTRUCT(BlueprintType)
struct FTeamUpgrades : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrades")
    TArray<FUpgradeStatus> Upgrades;

    /** Marks an individual item as dirty for replication */
    void MarkItemDirty(FUpgradeStatus& Item) { FFastArraySerializer::MarkItemDirty(Item); }

    /** Marks the entire array as dirty for replication */
    void MarkArrayDirty() { FFastArraySerializer::MarkArrayDirty(); }
};

UCLASS()
class RTSUNITTEMPLATE_API AUpgradeGameState : public AResourceGameState
{
    GENERATED_BODY()

public:
    AUpgradeGameState();

    // Array of team upgrades
    UPROPERTY(ReplicatedUsing = OnRep_TeamUpgrades, VisibleAnywhere, BlueprintReadOnly, Category = "Upgrades")
    TArray<FTeamUpgrades> TeamUpgradesArray;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_TeamUpgrades();

    // Main setter
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void SetTeamUpgrades(int32 TeamId, const TArray<FUpgradeStatus>& Upgrades);

    // Individual upgrade management
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void SetUpgradeResearched(int32 TeamId, int32 UpgradeIndex, bool bResearched);

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    TSubclassOf<UGameplayEffect> GetUpgradeInvestmentEffect(int32 TeamId, const FString& UpgradeName) const;
    
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void AddUpgradeToTeam(int32 TeamId, const FUpgradeStatus& Upgrade);

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    bool GetUpgradeResearchedStatus(int32 TeamId, FString UpgradeName) const;

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void ResearchUpgradeByName(int32 TeamId, FString UpgradeName);
};
