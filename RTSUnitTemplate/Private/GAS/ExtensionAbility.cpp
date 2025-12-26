// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "GAS/ExtensionAbility.h"

#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Actors/WorkArea.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h" // FOverlapResult, FHitResult, FCollisionQueryParams, FCollisionObjectQueryParams
#include "CollisionShape.h"     // FCollisionShape
#include "CollisionQueryParams.h" // FCollisionQueryParams
#include "Engine/OverlapResult.h" // FOverlapResult
#include "GameFramework/PlayerController.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/ConstructionUnit.h"

UExtensionAbility::UExtensionAbility()
{
	AbilityName = TEXT("Build Extension\n\n");
	KeyboardKey = TEXT("X");
	Type = FText::FromString(TEXT("Build"));
	Description = FText::FromString(TEXT("Place an extension work area next to this building and start casting to construct it."));
}