// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AttributeSetBase.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS(BlueprintType)
class RTSUNITTEMPLATE_API UAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	//UAttributeSetBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// Health //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Health);

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	// Health //

	// MaxHealth //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxHealth);
	
	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	// MaxHealth //

	// HealthRegeneration //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_HealthRegeneration)
	FGameplayAttributeData HealthRegeneration;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, HealthRegeneration);

	UFUNCTION()
	virtual void OnRep_HealthRegeneration(const FGameplayAttributeData& OldHealthRegeneration);
	// HealthRegeneration //

	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAttributeHealth(float NewHealth);

	// Shield //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Shield);

	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);
	// Shield //

	// MaxShield //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxShield);

	UFUNCTION()
	virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);
	// MaxShield //


	// Shield //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_ShieldRegeneration)
	FGameplayAttributeData ShieldRegeneration;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ShieldRegeneration);

	UFUNCTION()
	virtual void OnRep_ShieldRegeneration(const FGameplayAttributeData& OldShieldRegeneration);
	// Shield //
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAttributeShield(float NewShield);
	
	// AttackDamage //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_AttackDamage)
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, AttackDamage);

	UFUNCTION()
	virtual void OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage);
	// AttackDamage //

	
	// Range //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Range)
	FGameplayAttributeData Range;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Range);

	UFUNCTION()
	virtual void OnRep_Range(const FGameplayAttributeData& OldRange);
	// Range //

	// MaxRunSpeed //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_MaxRunSpeed)
	FGameplayAttributeData MaxRunSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxRunSpeed);

	UFUNCTION()
	virtual void OnRep_MaxRunSpeed(const FGameplayAttributeData& OldMaxRunSpeed);
	// MaxRunSpeed //

	// IsAttackedSpeed //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_IsAttackedSpeed)
	FGameplayAttributeData IsAttackedSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, IsAttackedSpeed);

	UFUNCTION()
	virtual void OnRep_IsAttackedSpeed(const FGameplayAttributeData& OldIsAttackedSpeed);
	// IsAttackedSpeed //

	// RunSpeedScale //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_RunSpeedScale)
	FGameplayAttributeData RunSpeedScale;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, RunSpeedScale);

	UFUNCTION()
	virtual void OnRep_RunSpeedScale(const FGameplayAttributeData& OldRunSpeedScale);
	// RunSpeedScale //

	// ProjectileScaleActorDirectionOffset //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_ProjectileScaleActorDirectionOffset)
	FGameplayAttributeData ProjectileScaleActorDirectionOffset;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ProjectileScaleActorDirectionOffset);

	UFUNCTION()
	virtual void OnRep_ProjectileScaleActorDirectionOffset(const FGameplayAttributeData& OldProjectileScaleActorDirectionOffset);
	// ProjectileScaleActorDirectionOffset //

	// ProjectileSpeed //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_ProjectileSpeed)
	FGameplayAttributeData ProjectileSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ProjectileSpeed);

	UFUNCTION()
	virtual void OnRep_ProjectileSpeed(const FGameplayAttributeData& OldProjectileSpeed);
	// ProjectileSpeed //

	// HERE WE START LEVEL ATTRIBUTES!!! //
	
	// Stamina //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Stamina);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
	// Stamina //

	// AttackPower //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, AttackPower);

	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
	// AttackPower //

	// Willpower //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Willpower)
	FGameplayAttributeData Willpower;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Willpower);

	UFUNCTION()
	virtual void OnRep_Willpower(const FGameplayAttributeData& OldWillpower);
	// Willpower //

	// Haste //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Haste)
	FGameplayAttributeData Haste;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Haste);

	UFUNCTION()
	virtual void OnRep_Haste(const FGameplayAttributeData& OldHaste);
	// Haste //

	// Armor //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Armor);

	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);
	// Armor //

	// MagicResistence //
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing= OnRep_MagicResistance)
	FGameplayAttributeData MagicResistance;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MagicResistance);

	UFUNCTION()
	virtual void OnRep_MagicResistance(const FGameplayAttributeData& OldMagicResistance);
	// MagicResistence //

};

