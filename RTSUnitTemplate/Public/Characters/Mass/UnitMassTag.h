// MyUnitFragments.h
#pragma once

#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "UnitMassTag.generated.h"

USTRUCT()
struct FUnitMassTag : public FMassTag
{
	GENERATED_BODY()
};

// Position + movement are handled by built-in fragments like FMassMovementFragment, so no need to add here.

USTRUCT()
struct FUnitStatsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY() // UPROPERTY helps with reflection if needed later
	float MaxHealth = 100.0f;

	UPROPERTY()
	float CurrentHealth = 100.0f;

	UPROPERTY()
	float AttackDamage = 10.0f;

	UPROPERTY()
	float AttackRange = 300.0f;

	UPROPERTY()
	float MovementSpeed = 500.0f;
};

UENUM()
enum class EUnitState : uint8
{
	Idle,
	Moving,
	Attacking,
	Dead
};

USTRUCT()
struct FUnitStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	EUnitState CurrentState = EUnitState::Idle;

	// Could store target entity handle here if attacking/following
	UPROPERTY()
	FMassEntityHandle TargetEntity;
};