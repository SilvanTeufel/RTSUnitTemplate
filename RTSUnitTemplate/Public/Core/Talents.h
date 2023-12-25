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
	float Health;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MaxHealth;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float HealthRegeneration;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Shield;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MaxShield;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ShieldRegeneration;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float AttackDamage;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Range;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float RunSpeed;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float IsAttackedSpeed;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float RunSpeedScale;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ProjectileScaleActorDirectionOffset;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float ProjectileSpeed;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Stamina;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float AttackPower;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Willpower;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Haste;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float Armor;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float MagicResistance;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseHealth;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseAttackDamage;

	UPROPERTY(VisibleAnywhere, Category = "Attributes")
	float BaseRunSpeed;
};