// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/UnitData.h"
#include "PathSeekerBase.h"
#include "TimerManager.h"
#include "AbilityUnit.generated.h"




UCLASS()
class RTSUNITTEMPLATE_API AAbilityUnit : public APathSeekerBase
{
	GENERATED_BODY()

private:
	FTimerHandle AccelerationTimerHandle;
	FVector TargetVelocity;
	FVector CurrentVelocity;
	float AccelerationRate;
	FVector ChargeDestination;
	float RequiredDistanceToStart;
	
	void Accelerate();
	void AccelerateFrom();

public:	
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void LevelUp_Implementation() override;
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void TeleportToValidLocation(const FVector& Destination);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void StartAcceleratingFromDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);

	// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);

	UFUNCTION(BlueprintCallable, Category = Ability)
	TEnumAsByte<UnitData::EState> GetUnitState();

	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void GetAbilitiesArrays();
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;
	///////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>OffensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>AttackAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>ThrowAbilities;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID ThrowAbilityID;

	UPROPERTY(EditAnywhere, Category = Ability)
	int32 AutoAbilitySequence[4] = {0, 1, 2, 3};

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetAutoAbilitySequence(int Index, int32 Value);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	bool AutoApplyAbility = true;

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool IsAbilityAllowed(EGASAbilityInputID AbilityID, int Ability);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SpendAbilityPoints( EGASAbilityInputID AbilityID, int Ability);

	UFUNCTION(BlueprintCallable, Category = Ability)
	int32 DetermineAbilityID(int32 Level);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AutoAbility();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AddAbilityPoint();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SaveAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void LoadAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void ResetAbility();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityResetPenalty = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityCostIncreaser = 1;
};
