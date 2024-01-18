// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/UnitData.h"
#include "PathSeekerBase.h"
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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TeleportToValidLocation(const FVector& Destination);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartAcceleratingFromDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);

	// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> GetUnitState();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void GetAbilitiesArrays();
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitState", Keywords = "RTSUnitTemplate UnitState"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitStatePlaceholder", Keywords = "RTSUnitTemplate UnitStatePlaceholder"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;
	///////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>OffensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>AttackAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>ThrowAbilities;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID ThrowAbilityID;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	int32 AutoAbilitySequence[4] = {0, 1, 2, 3};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AutoApplyAbility = true;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpendAbilityPoints( EGASAbilityInputID AbilityID, int Ability);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 DetermineAbilityID(int32 Level);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AutoAbility();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddAbilitPoint();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SaveAbilityData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LoadAbilityData(const FString& SlotName);
};
