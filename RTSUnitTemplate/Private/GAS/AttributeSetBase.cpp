// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/AttributeSetBase.h"
#include "Net/UnrealNetwork.h"

UAttributeSetBase::UAttributeSetBase()
{
}

void UAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Range, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, MaxRunSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, IsAttackedSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, RunSpeedScale, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, ProjectileScaleActorDirectionOffset, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, ProjectileSpeed, COND_None, REPNOTIFY_Always);
}

void UAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Health, OldHealth);
}

void UAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MaxHealth, OldMaxHealth);
}



void UAttributeSetBase::SetAttributeHealth(float NewHealth)
{
	if (NewHealth > GetMaxHealth())  // Assuming you have a getter for MaxHealth
	{
			Health.SetCurrentValue(GetMaxHealth());
	}
	else
	{
		Health.SetCurrentValue(FMath::Max(NewHealth, 0.0f));
	}

	// If health falls to zero or below, handle as needed
	if (Health.GetCurrentValue() <= 0.0f)
	{
		// Handle zero-health scenario
		// This could be notifying the unit, triggering an event, etc.
	}

	// Don't forget to apply the change
	//ApplyAttributeChange();
}

void UAttributeSetBase::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Shield, OldShield);
}

void UAttributeSetBase::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MaxShield, OldMaxShield);
}

void UAttributeSetBase::SetAttributeShield(float NewShield)
{
	if(NewShield <= 0)
	{
		Shield = 0.f;
		SetHealth(GetHealth()+NewShield);
	}else if(NewShield > GetMaxShield())
	{
		Shield = GetMaxShield();
	}else
	{
		Shield = NewShield;
	}
}

void UAttributeSetBase::OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, AttackDamage, OldAttackDamage);
}

void UAttributeSetBase::OnRep_Range(const FGameplayAttributeData& OldRange)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Range, OldRange);
}

void UAttributeSetBase::OnRep_MaxRunSpeed(const FGameplayAttributeData& OldMaxRunSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MaxRunSpeed, OldMaxRunSpeed);
}

void UAttributeSetBase::OnRep_IsAttackedSpeed(const FGameplayAttributeData& OldIsAttackedSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, IsAttackedSpeed, OldIsAttackedSpeed);
}

void UAttributeSetBase::OnRep_RunSpeedScale(const FGameplayAttributeData& OldRunSpeedScale)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, RunSpeedScale, OldRunSpeedScale);
}

void UAttributeSetBase::OnRep_ProjectileScaleActorDirectionOffset(
	const FGameplayAttributeData& OldProjectileScaleActorDirectionOffset)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, ProjectileScaleActorDirectionOffset, OldProjectileScaleActorDirectionOffset);
}

void UAttributeSetBase::OnRep_ProjectileSpeed(const FGameplayAttributeData& OldProjectileSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, ProjectileSpeed, OldProjectileSpeed);
}


void UAttributeSetBase::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Stamina, OldStamina);
}

void UAttributeSetBase::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, AttackPower, OldAttackPower);
}
