// LevelUnit.h

#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
#include "LevelUnit.generated.h"

/**
 * ALevelUnit is a child class of AGASUnit that includes leveling and talent point functionality.
 */
UCLASS()
class RTSUNITTEMPLATE_API ALevelUnit : public AGASUnit
{
	GENERATED_BODY()

public:
	//ALevelUnit();

	// Properties to store the unit's level and talent points
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 CharacterLevel = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPointsPerLevel = 5;
	// Gameplay Effects for talent point investment
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> StaminaInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> AttackPowerInvestmentEffect;

	// Methods for handling leveling up and investing talent points
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoStamina();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoAttackPower();

protected:
	// Helper method to handle the actual attribute increase when a point is invested
	void ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect);
};
