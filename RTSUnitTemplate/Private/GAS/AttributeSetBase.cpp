// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/AttributeSetBase.h"

#include "Actors/IndicatorActor.h"
#include "Characters/Unit/UnitBase.h"
#include "Kismet/GameplayStatics.h"
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
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, AttackDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Range, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, RunSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, IsAttackedSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, RunSpeedScale, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, ProjectileScaleActorDirectionOffset, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, ProjectileSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, HealthRegeneration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, ShieldRegeneration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Willpower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Haste, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, MagicResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, BaseHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, BaseAttackDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, BaseRunSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, EffectDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAttributeSetBase, EffectShield, COND_None, REPNOTIFY_Always);
}

void UAttributeSetBase::UpdateAttributes(const FAttributeSaveData SourceData)
{
	SetAttributeMaxHealth(SourceData.MaxHealth);
	SetAttributeHealth(SourceData.MaxHealth);
	SetAttributeHealthRegeneration(SourceData.HealthRegeneration);
	SetAttributeMaxShield(SourceData.MaxShield);
	SetAttributeShield(SourceData.MaxShield);
	SetAttributeShieldRegeneration(SourceData.ShieldRegeneration);
	SetAttributeAttackDamage(SourceData.AttackDamage);
	SetAttributeRange(SourceData.Range);
	SetAttributeRunSpeed(SourceData.RunSpeed);
	SetAttributeIsAttackedSpeed(SourceData.IsAttackedSpeed);
	SetAttributeRunSpeedScale(SourceData.RunSpeedScale);
	SetAttributeProjectileScaleActorDirectionOffset(SourceData.ProjectileScaleActorDirectionOffset);
	SetAttributeProjectileSpeed(SourceData.ProjectileSpeed);
	SetAttributeStamina(SourceData.Stamina);
	SetAttributeAttackPower(SourceData.AttackPower);
	SetAttributeWillpower(SourceData.Willpower);
	SetAttributeHaste(SourceData.Haste);
	SetAttributeArmor(SourceData.Armor);
	SetAttributeMagicResistance(SourceData.MagicResistance);
	SetAttributeBaseHealth(SourceData.BaseHealth);
	SetAttributeBaseAttackDamage(SourceData.BaseAttackDamage);
	SetAttributeBaseRunSpeed(SourceData.BaseRunSpeed);
}

void UAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	AUnitBase* UnitBase = Cast<AUnitBase>(GetOwningActor());

	if(UnitBase && UnitBase->GetUnitState() != UnitData::Dead)
	{
		// Turn on Healthbar Visibility

		if(Data.EvaluatedData.Attribute == GetEffectDamageAttribute())
		{

			// Assume DamageAmount is the amount of damage to apply
			float DamageAmount = Data.EvaluatedData.Magnitude;

			if (GetHealth() <= 0)
			{
				UnitBase->SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);
				UnitBase->DeadEffectsAndEvents();
				return;
			}
			float OldHealth = GetHealth();
			// Check and apply damage to Shield first
			if(DamageAmount < 0)
			{
				if(GetShield() > 0)
				{
					float OldShield = GetShield();

					SpawnIndicator(-1*DamageAmount, FLinearColor::Red, FLinearColor::White, 0.25f);
					SetAttributeShield(GetShield() + DamageAmount);
				
					if(OldShield < DamageAmount)
					{
						DamageAmount = DamageAmount - (OldShield - GetShield());
					}
					else
					{
						DamageAmount = 0;
					}
				}

				// Compute threshold crossings before applying damage to Health
				{
					const float NewHealthProjected = FMath::Clamp(GetHealth() + DamageAmount, 0.0f, GetMaxHealth());
					const float MaxH = GetMaxHealth();
					if (MaxH > 0.f && OldHealth != NewHealthProjected)
					{
						const float OldPct = OldHealth / MaxH;
						const float NewPct = NewHealthProjected / MaxH;
						// Downward crossings (damage)
						if (OldPct >= 0.50f && NewPct < 0.50f) { UnitBase->OnHealthThresholdCrossed(false, false, true, NewHealthProjected); }
						if (OldPct >= 0.25f && NewPct < 0.25f) { UnitBase->OnHealthThresholdCrossed(false, true,  false, NewHealthProjected); }
						// Upward crossings (edge case if shield overflow indirectly heals, included for completeness)
						if (OldPct <= 0.25f && NewPct > 0.25f) { UnitBase->OnHealthThresholdCrossed(true,  true,  false, NewHealthProjected); }
						if (OldPct <= 0.50f && NewPct > 0.50f) { UnitBase->OnHealthThresholdCrossed(true,  false, true, NewHealthProjected); }
					}
				}
				SpawnIndicator(-1*DamageAmount, FLinearColor::Red, FLinearColor::White, 0.25f);
				SetAttributeHealth(FMath::Max(GetHealth() + DamageAmount, 0.0f));

				if (GetHealth() <= 0)
				{
					UnitBase->SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);
					UnitBase->DeadEffectsAndEvents();
				}
			}else
			{
				if (OldHealth + DamageAmount <= GetMaxHealth())
					SpawnIndicator(DamageAmount, FLinearColor::Green, FLinearColor::White, 0.7f);

				// Compute threshold crossings before applying heal to Health
				{
					const float NewHealthProjected = FMath::Clamp(GetHealth() + DamageAmount, 0.0f, GetMaxHealth());
					const float MaxH = GetMaxHealth();
					if (MaxH > 0.f && OldHealth != NewHealthProjected)
					{
						const float OldPct = OldHealth / MaxH;
						const float NewPct = NewHealthProjected / MaxH;
						// Upward crossings (healing)
						if (OldPct <= 0.25f && NewPct > 0.25f) { UnitBase->OnHealthThresholdCrossed(true,  true,  false, NewHealthProjected); }
						if (OldPct <= 0.50f && NewPct > 0.50f) { UnitBase->OnHealthThresholdCrossed(true,  false, true, NewHealthProjected); }
						// Downward crossings shouldn't happen on heal, but keep checks symmetrical for safety
						if (OldPct >= 0.50f && NewPct < 0.50f) { UnitBase->OnHealthThresholdCrossed(false, false, true, NewHealthProjected); }
						if (OldPct >= 0.25f && NewPct < 0.25f) { UnitBase->OnHealthThresholdCrossed(false, true,  false, NewHealthProjected); }
					}
				}
				SetAttributeHealth(FMath::Max(GetHealth() + DamageAmount, 0.0f));
			}

			if (OldHealth + DamageAmount <= GetMaxHealth())
			{
				UnitBase->UpdateEntityHealth(OldHealth + DamageAmount);
			}
			
			UnitBase->UpdateWidget();
		}

		if(Data.EvaluatedData.Attribute == GetEffectShieldAttribute() && GetHealth() > 0)
		{
			float ShieldAmount = Data.EvaluatedData.Magnitude;
			//SpawnIndicator(ShieldAmount, FLinearColor::Blue, FLinearColor::White, 0.7f);
			float OldShield = GetShield();
			
			if (OldShield + ShieldAmount <= GetMaxShield())
			{
				UnitBase->UpdateEntityHealth(GetHealth());
				SpawnIndicator(ShieldAmount, FLinearColor::Blue, FLinearColor::White, 0.7f);
			}
			SetAttributeShield(OldShield + ShieldAmount);
		
			UnitBase->UpdateWidget();
		}
	}

	
	// Call the superclass version for other attributes
	Super::PostGameplayEffectExecute(Data);
	
	
}



void UAttributeSetBase::SpawnIndicator(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset) // FVector TargetLocation
{
	AUnitBase* UnitBase = Cast<AUnitBase>(GetOwningActor());

	UnitBase->SpawnDamageIndicator(Damage, HighColor, LowColor, ColorOffset);
}

void UAttributeSetBase::OnRep_EffectDamage(const FGameplayAttributeData& OldEffectDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, EffectDamage, OldEffectDamage);
}

void UAttributeSetBase::OnRep_EffectShield(const FGameplayAttributeData& OldEffectShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, EffectShield, OldEffectShield);
}

void UAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Health, OldHealth);
}

void UAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MaxHealth, OldMaxHealth);
}

void UAttributeSetBase::OnRep_HealthRegeneration(const FGameplayAttributeData& OldHealthRegeneration)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, HealthRegeneration, OldHealthRegeneration);
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
}

void UAttributeSetBase::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Shield, OldShield);
}

void UAttributeSetBase::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MaxShield, OldMaxShield);
}

void UAttributeSetBase::OnRep_ShieldRegeneration(const FGameplayAttributeData& OldShieldRegeneration)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, ShieldRegeneration, OldShieldRegeneration);
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

void UAttributeSetBase::OnRep_RunSpeed(const FGameplayAttributeData& OldRunSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, RunSpeed, OldRunSpeed);
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

void UAttributeSetBase::OnRep_Willpower(const FGameplayAttributeData& OldWillpower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Willpower, OldWillpower);
}

void UAttributeSetBase::OnRep_Haste(const FGameplayAttributeData& OldHaste)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Haste, OldHaste);
}

void UAttributeSetBase::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Armor, OldArmor);
}

void UAttributeSetBase::OnRep_MagicResistance(const FGameplayAttributeData& OldMagicResistance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, MagicResistance, OldMagicResistance);
}

void UAttributeSetBase::OnRep_BaseHealth(const FGameplayAttributeData& OldBaseHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, BaseHealth, OldBaseHealth);
}

void UAttributeSetBase::OnRep_BaseAttackDamage(const FGameplayAttributeData& OldBaseAttackDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, BaseAttackDamage, OldBaseAttackDamage);
}

void UAttributeSetBase::OnRep_BaseRunSpeed(const FGameplayAttributeData& OldBaseRunSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, BaseRunSpeed, OldBaseRunSpeed);
}

void UAttributeSetBase::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, Mana, OldMana);
}

void UAttributeSetBase::OnRep_SpellSize(const FGameplayAttributeData& OldSpellSize)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, SpellSize, OldSpellSize);
}

void UAttributeSetBase::OnRep_SpellForce(const FGameplayAttributeData& OldSpellForce)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, SpellForce, OldSpellForce);
}

void UAttributeSetBase::OnRep_SpellCapacity(const FGameplayAttributeData& OldSpellCapacity)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAttributeSetBase, SpellCapacity, OldSpellCapacity);
}

void UAttributeSetBase::SetAttributeMaxHealth(float NewValue)
{
    SetMaxHealth(NewValue);
}

void UAttributeSetBase::SetAttributeHealthRegeneration(float NewValue)
{
    SetHealthRegeneration(NewValue);
}

void UAttributeSetBase::SetAttributeMaxShield(float NewValue)
{
    SetMaxShield(NewValue);
}

void UAttributeSetBase::SetAttributeShieldRegeneration(float NewValue)
{
    SetShieldRegeneration(NewValue);
}

void UAttributeSetBase::SetAttributeAttackDamage(float NewValue)
{
    SetAttackDamage(NewValue);
}

void UAttributeSetBase::SetAttributeRange(float NewValue)
{
    SetRange(NewValue);
}

void UAttributeSetBase::SetAttributeRunSpeed(float NewValue)
{
    SetRunSpeed(NewValue);
}

void UAttributeSetBase::SetAttributeIsAttackedSpeed(float NewValue)
{
    SetIsAttackedSpeed(NewValue);
}


void UAttributeSetBase::SetAttributeRunSpeedScale(float NewValue)
{
    SetRunSpeedScale(NewValue);
}

void UAttributeSetBase::SetAttributeProjectileScaleActorDirectionOffset(float NewValue)
{
    SetProjectileScaleActorDirectionOffset(NewValue);
}

void UAttributeSetBase::SetAttributeProjectileSpeed(float NewValue)
{
    SetProjectileSpeed(NewValue);
}

void UAttributeSetBase::SetAttributeStamina(float NewValue)
{
    SetStamina(NewValue);
}

void UAttributeSetBase::SetAttributeAttackPower(float NewValue)
{
    SetAttackPower(NewValue);
}

void UAttributeSetBase::SetAttributeWillpower(float NewValue)
{
    SetWillpower(NewValue);
}

void UAttributeSetBase::SetAttributeHaste(float NewValue)
{
    SetHaste(NewValue);
}

void UAttributeSetBase::SetAttributeArmor(float NewValue)
{
    SetArmor(NewValue);
}

void UAttributeSetBase::SetAttributeMagicResistance(float NewValue)
{
    SetMagicResistance(NewValue);
}

void UAttributeSetBase::SetAttributeBaseHealth(float NewValue)
{
    SetBaseHealth(NewValue);
}

void UAttributeSetBase::SetAttributeBaseAttackDamage(float NewValue)
{
    SetBaseAttackDamage(NewValue);
}

void UAttributeSetBase::SetAttributeBaseRunSpeed(float NewValue)
{
    SetBaseRunSpeed(NewValue);
}

void UAttributeSetBase::SetAttributeMana(float NewValue)
{
	SetMana(NewValue); 
}

void UAttributeSetBase::SetAttributeSpellSize(float NewValue)
{
	SetSpellSize(NewValue);
}

void UAttributeSetBase::SetAttributeSpellForce(float NewValue)
{
	SetSpellForce(NewValue);
}

void UAttributeSetBase::SetAttributeSpellCapacity(float NewValue)
{
	SetSpellCapacity(NewValue);
}

