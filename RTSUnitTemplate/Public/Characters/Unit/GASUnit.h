// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GAS/GAS.h"
#include "Core/UnitData.h"
#include "AbilitySystemInterface.h"
#include <GameplayEffectTypes.h>
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/AttributeSetBase.h"
#include "GASUnit.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AGASUnit : public ACharacter, public IAbilitySystemInterface
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

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void InitializeAttributes();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void GiveAbilities();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void ActivateAbilityByInputID(EGASAbilityInputID InputID);

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	TSubclassOf<UGameplayAbility> GetAbilityForInputID(EGASAbilityInputID InputID);
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TSubclassOf<class UGameplayEffect>DefaultAttributeEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefaultAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID AttackAbilityID;
	
};
