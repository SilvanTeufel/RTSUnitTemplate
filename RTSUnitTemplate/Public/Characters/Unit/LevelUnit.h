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
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool IsDoingMagicDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool AutoLeveling = true;
	// Properties to store the unit's level and talent points
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 Experience = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 ExperiencePerLevel = 2;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 CharacterLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 MaxCharacterLevel = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 TalentPointsPerLevel = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 MaxTalentsPerStat = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	int32 UsedTalentPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationTimer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationDelayTime = 1.f;
	
	// Gameplay Effects for talent point investment
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> StaminaInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> AttackPowerInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> WillpowerInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> HasteInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> ArmorInvestmentEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> MagicResistanceInvestmentEffect;
	
	// Methods for handling leveling up and investing talent points
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void AutoLevelUp();
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetLevel(int32 CharLevel);
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoStamina();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoAttackPower();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoWillPower();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoHaste();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoArmor();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoMagicResistance();
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetTalents();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetLevel();
protected:
	// Helper method to handle the actual attribute increase when a point is invested
	void ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect);
};
