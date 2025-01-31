#pragma once

#include "CoreMinimal.h"
#include "GameModes/ResourceGameMode.h"
#include "UpgradeGameMode.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AUpgradeGameMode : public AResourceGameMode
{
	GENERATED_BODY()

public:
	AUpgradeGameMode();

protected:
	virtual void BeginPlay() override;

public:
	// Initialize upgrades for all teams
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void InitializeUpgradesForTeams();

	// Add upgrade to a specific team
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void AddUpgrade(int32 TeamId, const FString& UpgradeName, TSubclassOf<UGameplayEffect> InvestmentEffect);

	// Research an upgrade for a specific team
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void ResearchUpgrade(int32 TeamId, const FString& UpgradeName);

	// Initialize a single upgrade for a specific team
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void InitializeSingleUpgrade(int32 TeamId, const FString& UpgradeName, bool bResearched, TSubclassOf<UGameplayEffect> InvestmentEffect);
};
