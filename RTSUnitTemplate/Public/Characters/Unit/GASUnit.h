// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAbilitySystemComponentBase* AbilitySystemComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	class UAttributeSetBase* Attributes;
	
//protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void InitializeAttributes();
	virtual void GiveAbilities();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void ActivateAbilityByInputID(EGASAbilityInputID InputID);
	
	TSubclassOf<UGameplayAbility> GetAbilityForInputID(EGASAbilityInputID InputID);
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=RTSUnitTemplate)
	TSubclassOf<class UGameplayEffect>DefaultAttributeEffect;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefaultAbilities;


	
};
