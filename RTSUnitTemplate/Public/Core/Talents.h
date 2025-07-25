// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Talents.generated.h"


USTRUCT(BlueprintType)
struct FAttributeSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Health = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MaxHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float HealthRegeneration = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Shield = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MaxShield = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ShieldRegeneration = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float AttackDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Range = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float RunSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float IsAttackedSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float RunSpeedScale = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ProjectileScaleActorDirectionOffset = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ProjectileSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Stamina = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float AttackPower = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Willpower = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Haste = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Armor = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MagicResistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseAttackDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseRunSpeed = 0.0f;
};