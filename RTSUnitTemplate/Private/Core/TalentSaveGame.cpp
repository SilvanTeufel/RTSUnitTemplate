// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/TalentSaveGame.h"

void UTalentSaveGame::PopulateAttributeSaveData(UAttributeSetBase* AttributeSet)
{
	if (AttributeSet == nullptr)
	{
		// Handle error as needed
		return;
	}

	// Copying values from AttributeSet to AttributeSaveData struct
	AttributeSaveData.Health = AttributeSet->GetHealth();
	AttributeSaveData.MaxHealth = AttributeSet->GetMaxHealth();
	AttributeSaveData.HealthRegeneration = AttributeSet->GetHealthRegeneration();
	AttributeSaveData.Shield = AttributeSet->GetShield();
	AttributeSaveData.MaxShield = AttributeSet->GetMaxShield();
	AttributeSaveData.ShieldRegeneration = AttributeSet->GetShieldRegeneration();
	AttributeSaveData.AttackDamage = AttributeSet->GetAttackDamage();
	AttributeSaveData.Range = AttributeSet->GetRange();
	AttributeSaveData.RunSpeed = AttributeSet->GetRunSpeed();
	AttributeSaveData.IsAttackedSpeed = AttributeSet->GetIsAttackedSpeed();
	AttributeSaveData.RunSpeedScale = AttributeSet->GetRunSpeedScale();
	AttributeSaveData.ProjectileScaleActorDirectionOffset = AttributeSet->GetProjectileScaleActorDirectionOffset();
	AttributeSaveData.ProjectileSpeed = AttributeSet->GetProjectileSpeed();
	AttributeSaveData.Stamina = AttributeSet->GetStamina();
	AttributeSaveData.AttackPower = AttributeSet->GetAttackPower();
	AttributeSaveData.Willpower = AttributeSet->GetWillpower();
	AttributeSaveData.Haste = AttributeSet->GetHaste();
	AttributeSaveData.Armor = AttributeSet->GetArmor();
	AttributeSaveData.MagicResistance = AttributeSet->GetMagicResistance();
	AttributeSaveData.BaseHealth = AttributeSet->GetBaseHealth();
	AttributeSaveData.BaseAttackDamage = AttributeSet->GetBaseAttackDamage();
	AttributeSaveData.BaseRunSpeed = AttributeSet->GetBaseRunSpeed();
}
