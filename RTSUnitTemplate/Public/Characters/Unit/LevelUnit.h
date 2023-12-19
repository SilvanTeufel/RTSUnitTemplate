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

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool IsDoingMagicDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool AutoLeveling = true;
	// Properties to store the unit's level and talent points

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	FLevelData LevelData;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	FLevelUpData LevelUpData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationTimer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationDelayTime = 1.f;

	// Gameplay Effects for talent point investment
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> StaminaInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> AttackPowerInvestmentEffect;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> WillpowerInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> HasteInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> ArmorInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TSubclassOf<UGameplayEffect> MagicResistanceInvestmentEffect;
	
	// Methods for handling leveling up and investing talent points
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Leveling")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void AutoLevelUp();
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetLevel(int32 CharLevel);
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoStamina();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Leveling")
	void ServerInvestPointIntoStamina();
	
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

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Leveling")
	TEnumAsByte<UInvestmentData::InvestmentState> CurrentInvestmentState;
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void HandleInvestment(TEnumAsByte<UInvestmentData::InvestmentState> State);
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetTalents();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetLevel();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SaveLevelDataAndAttributes(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LoadLevelDataAndAttributes(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void UpdateAttributes(UAttributeSetBase* LoadedAttributes);
	
//protected:
	// Helper method to handle the actual attribute increase when a point is invested
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect);
};
