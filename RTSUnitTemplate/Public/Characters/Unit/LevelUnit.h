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

	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere, Category = "Leveling")
	int32 UnitIndex;

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetUnitIndex(int32 NewIndex);
	
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
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> StaminaInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> AttackPowerInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> WillpowerInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> HasteInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> ArmorInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> MagicResistanceInvestmentEffect;

	// Array of Custom Effects
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TArray<TSubclassOf<UGameplayEffect>> CustomEffects;
	
	// Methods for handling leveling up and investing talent points
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Leveling")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void AutoLevelUp();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	TArray<int32> AutolevelConfig = {1, 1, 1, 1, 1, 0};
	
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

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SaveLevelDataAndAttributes(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LoadLevelDataAndAttributes(const FString& SlotName);
	
//protected:
	// Helper method to handle the actual attribute increase when a point is invested
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ApplyInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect);
};
