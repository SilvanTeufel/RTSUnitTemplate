// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GAS/GAS.h"
#include "Core/UnitData.h"
#include "AbilitySystemInterface.h"
#include <GameplayEffectTypes.h>

#include "SpawnerUnit.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/AttributeSetBase.h"
#include "GASUnit.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AGASUnit : public ASpawnerUnit, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	//AGASUnit();
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAbilitySystemComponentBase* AbilitySystemComponent;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAttributeSetBase* Attributes;
	
//protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void InitializeAttributes();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void GiveAbilities();

	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ToggleUnitDetection = false;

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetToggleUnitDetection(bool ToggleTo);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool GetToggleUnitDetection();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void ActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	TSubclassOf<UGameplayAbility> GetAbilityForInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TSubclassOf<class UGameplayEffect>DefaultAttributeEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefaultAbilities;

protected:
	// Function called when an ability is activated
	void OnAbilityActivated(UGameplayAbility* ActivatedAbility);

	// Setup delegates for ability system
	void SetupAbilitySystemDelegates();

public:
	// Reference to the activated ability instance
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	UGameplayAbilityBase* ActivatedAbilityInstance;
};
