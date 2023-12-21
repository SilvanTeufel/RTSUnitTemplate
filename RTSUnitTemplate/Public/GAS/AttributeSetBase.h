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
	UAttributeSetBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// Health //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Health);

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	// Health //

	// MaxHealth //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxHealth);
	
	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	// MaxHealth //

	// HealthRegeneration //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_HealthRegeneration)
	FGameplayAttributeData HealthRegeneration;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, HealthRegeneration);

	UFUNCTION()
	virtual void OnRep_HealthRegeneration(const FGameplayAttributeData& OldHealthRegeneration);
	// HealthRegeneration //

	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAttributeHealth(float NewHealth);

	// Shield //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Shield);

	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);
	// Shield //

	// MaxShield //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxShield);

	UFUNCTION()
	virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);
	// MaxShield //


	// Shield //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_ShieldRegeneration)
	FGameplayAttributeData ShieldRegeneration;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ShieldRegeneration);

	UFUNCTION()
	virtual void OnRep_ShieldRegeneration(const FGameplayAttributeData& OldShieldRegeneration);
	// Shield //
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAttributeShield(float NewShield);
	
	// AttackDamage //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_AttackDamage)
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, AttackDamage);

	UFUNCTION()
	virtual void OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage);
	// AttackDamage //

	
	// Range //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Range)
	FGameplayAttributeData Range;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Range);

	UFUNCTION()
	virtual void OnRep_Range(const FGameplayAttributeData& OldRange);
	// Range //

	// MaxRunSpeed //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_RunSpeed)
	FGameplayAttributeData RunSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, RunSpeed);

	UFUNCTION()
	virtual void OnRep_RunSpeed(const FGameplayAttributeData& OldRunSpeed);
	// MaxRunSpeed //

	// IsAttackedSpeed //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_IsAttackedSpeed)
	FGameplayAttributeData IsAttackedSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, IsAttackedSpeed);

	UFUNCTION()
	virtual void OnRep_IsAttackedSpeed(const FGameplayAttributeData& OldIsAttackedSpeed);
	// IsAttackedSpeed //

	// RunSpeedScale //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_RunSpeedScale)
	FGameplayAttributeData RunSpeedScale;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, RunSpeedScale);

	UFUNCTION()
	virtual void OnRep_RunSpeedScale(const FGameplayAttributeData& OldRunSpeedScale);
	// RunSpeedScale //

	// ProjectileScaleActorDirectionOffset //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_ProjectileScaleActorDirectionOffset)
	FGameplayAttributeData ProjectileScaleActorDirectionOffset;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ProjectileScaleActorDirectionOffset);

	UFUNCTION()
	virtual void OnRep_ProjectileScaleActorDirectionOffset(const FGameplayAttributeData& OldProjectileScaleActorDirectionOffset);
	// ProjectileScaleActorDirectionOffset //

	// ProjectileSpeed //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_ProjectileSpeed)
	FGameplayAttributeData ProjectileSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, ProjectileSpeed);

	UFUNCTION()
	virtual void OnRep_ProjectileSpeed(const FGameplayAttributeData& OldProjectileSpeed);
	// ProjectileSpeed //

	// HERE WE START LEVEL ATTRIBUTES!!! //
	
	// Stamina //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Stamina);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
	// Stamina //

	// AttackPower //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, AttackPower);

	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
	// AttackPower //

	// Willpower //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Willpower)
	FGameplayAttributeData Willpower;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Willpower);

	UFUNCTION()
	virtual void OnRep_Willpower(const FGameplayAttributeData& OldWillpower);
	// Willpower //

	// Haste //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Haste)
	FGameplayAttributeData Haste;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Haste);

	UFUNCTION()
	virtual void OnRep_Haste(const FGameplayAttributeData& OldHaste);
	// Haste //

	// Armor //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, Armor);

	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);
	// Armor //

	// MagicResistence //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_MagicResistance)
	FGameplayAttributeData MagicResistance;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MagicResistance);

	UFUNCTION()
	virtual void OnRep_MagicResistance(const FGameplayAttributeData& OldMagicResistance);
	// MagicResistence //
	/*
	// MaxHealthPerStamina //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_MaxHealthPerStamina)
	FGameplayAttributeData MaxHealthPerStamina;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, MaxHealthPerStamina);

	UFUNCTION()
	virtual void OnRep_MaxHealthPerStamina(const FGameplayAttributeData& OldMaxHealthPerStamina);
	// MaxHealthPerStamina //

	// AttackDamagePerAttackPower //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_AttackDamagePerAttackPower)
	FGameplayAttributeData AttackDamagePerAttackPower;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, AttackDamagePerAttackPower);

	UFUNCTION()
	virtual void OnRep_AttackDamagePerAttackPower(const FGameplayAttributeData& OldAttackDamagePerAttackPower);
	// AttackDamagePerAttackPower //
	
	// MaxWalkSpeedPerHaste //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_RunSpeedPerHaste)
	FGameplayAttributeData RunSpeedPerHaste;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, RunSpeedPerHaste);
	UFUNCTION()
	virtual void OnRep_RunSpeedPerHaste(const FGameplayAttributeData& OldRunSpeedPerHaste);
	// MaxWalkSpeedPerHaste //*/
	
	// BaseHealth //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_BaseHealth)
	FGameplayAttributeData BaseHealth;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, BaseHealth);

	UFUNCTION()
	virtual void OnRep_BaseHealth(const FGameplayAttributeData& OldBaseHealth);
	// BaseHealth //

	// BaseAttackDamage //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_BaseAttackDamage)
	FGameplayAttributeData BaseAttackDamage;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, BaseAttackDamage);

	UFUNCTION()
	virtual void OnRep_BaseAttackDamage(const FGameplayAttributeData& OldBaseAttackDamage);
	// BaseAttackDamage //
	
	// BaseMaxWalkSpeed //
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attributes", SaveGame, ReplicatedUsing= OnRep_BaseRunSpeed)
	FGameplayAttributeData BaseRunSpeed;
	ATTRIBUTE_ACCESSORS(UAttributeSetBase, BaseRunSpeed);
	UFUNCTION()
	virtual void OnRep_BaseRunSpeed(const FGameplayAttributeData& OldBaseRunSpeed);
	// BaseMaxWalkSpeed //

};

