// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/PlayerController/WidgetController.h"
#include "NavigationSystem.h" // Include this for navigation functions
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Core/UnitData.h"
#include "AIController.h"
#include "Landscape.h"
#include "Actors/EffectArea.h"
#include "Actors/MissileRain.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "NavMesh/NavMeshPath.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameModes/ResourceGameMode.h"
#include "Engine/Engine.h"


float AWidgetController::GetResource(int TeamId, EResourceType RType)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) return 0;
	
	return GameMode->GetResource(TeamId, RType);
}

void AWidgetController::ModifyResource_Implementation(EResourceType ResourceType, int32 TeamId, float Amount){
	
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) return;
	
	GameMode->ModifyResource(ResourceType, TeamId, Amount);
}

void AWidgetController::LoadLevel_Implementation(const FString& SlotName)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			//SelectedUnits[i]->LoadLevelDataAndAttributes(SlotName);
			SelectedUnits[i]->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::SaveLevel_Implementation(const FString& SlotName)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			//SelectedUnits[i]->SaveLevelDataAndAttributes(SlotName);
			SelectedUnits[i]->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LevelUp_Implementation()
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			SelectedUnits[i]->LevelUp();
		}
	}
}



void AWidgetController::ResetTalents_Implementation()
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			SelectedUnits[i]->ResetTalents();
		}
	}
}

void AWidgetController::HandleInvestment_Implementation(int32 InvestmentState)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			switch (InvestmentState)
			{
			case 0:
				{
					SelectedUnits[i]->InvestPointIntoStamina();
				}
				break;
			case 1:
				{
					SelectedUnits[i]->InvestPointIntoAttackPower();
				}
				break;
			case 2:
				{
					SelectedUnits[i]->InvestPointIntoWillPower();
				}
				break;
			case 3:
				{
					SelectedUnits[i]->InvestPointIntoHaste();
				}
				break;
			case 4:
				{
					SelectedUnits[i]->InvestPointIntoArmor();
				}
				break;
			case 5:
				{
					SelectedUnits[i]->InvestPointIntoMagicResistance();
				}
				break;
			case 6:
				{
					UE_LOG(LogTemp, Warning, TEXT("None"));
				}
				break;
			default:
				break;
				}
		}
	}
}

void AWidgetController::SaveLevelUnit_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			//Unit->SaveLevelDataAndAttributes(SlotName);
			Unit->SaveAbilityAndLevelData(SlotName);
		}
	}
}


void AWidgetController::LoadLevelUnit_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if(Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
			//Unit->LoadLevelDataAndAttributes(SlotName);
			Unit->LoadAbilityAndLevelData(SlotName);
	}
}

void AWidgetController::LoadLevelUnitByTag_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	FGameplayTagContainer TargetUnitTags;

	// Find the target unit and get its tags
	int TeamId = 0;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and load level data
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LevelUpUnit_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->LevelUp();
		}
	}
}

void AWidgetController::LevelUpUnitByTag_Implementation(const int32 UnitIndex)
{
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and level up
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->LevelUp();
		}
	}
}

void AWidgetController::ResetTalentsUnit_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->ResetTalents();
		}
	}
}

void AWidgetController::ResetTalentsUnitByTag_Implementation(const int32 UnitIndex)
{
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and reset talents
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->ResetTalents();
		}
	}
}

void AWidgetController::HandleInvestmentUnit_Implementation(const int32 UnitIndex, int32 InvestmentState)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			switch (InvestmentState)
			{
			case 0:
				{
					Unit->InvestPointIntoStamina();
				}
				break;
			case 1:
				{
					Unit->InvestPointIntoAttackPower();
				}
				break;
			case 2:
				{
					Unit->InvestPointIntoWillPower();
				}
				break;
			case 3:
				{
					Unit->InvestPointIntoHaste();
				}
				break;
			case 4:
				{
					Unit->InvestPointIntoArmor();
				}
				break;
			case 5:
				{
					Unit->InvestPointIntoMagicResistance();
				}
				break;
			case 6:
				{
					UE_LOG(LogTemp, Warning, TEXT("None"));
				}
				break;
			default:
				break;
			}
		}
	}
}


void AWidgetController::HandleInvestmentUnitByTag(const int32 UnitIndex, int32 InvestmentState)
{

	// Lambda to handle the investment
	auto Invest = [&](AUnitBase* Unit) {
		switch (InvestmentState)
		{
		case 0: Unit->InvestPointIntoStamina(); break;
		case 1: Unit->InvestPointIntoAttackPower(); break;
		case 2: Unit->InvestPointIntoWillPower(); break;
		case 3: Unit->InvestPointIntoHaste(); break;
		case 4: Unit->InvestPointIntoArmor(); break;
		case 5: Unit->InvestPointIntoMagicResistance(); break;
		case 6: UE_LOG(LogTemp, Warning, TEXT("None")); break;
		default: break;
		}
	};
	
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags; // FGameplayTagContainer UnitTags; Get the Tags from here
			TeamId = Unit->TeamId;
			break;
		}
	}

	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Invest(Unit);
		}
	}
}

void AWidgetController::SpendAbilityPoints_Implementation(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->SpendAbilityPoints(AbilityID, Ability);
		}
	}
}

void AWidgetController::SpendAbilityPointsByTag_Implementation(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex)
{
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and spend ability points
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->SpendAbilityPoints(AbilityID, Ability);
		}
	}
}

void AWidgetController::ResetAbility_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->ResetAbility();
		}
	}
}

void AWidgetController::ResetAbilityByTag_Implementation(const int32 UnitIndex)
{
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and reset ability
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->ResetAbility();
		}
	}
}

void AWidgetController::SaveAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LoadAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LoadAbilityByTag_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	FGameplayTagContainer TargetUnitTags;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			TargetUnitTags = Unit->UnitTags;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and load ability data
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(TargetUnitTags) && (Unit->TeamId == TeamId))
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::AddWorkerToResource_Implementation(EResourceType ResourceType, int TeamId)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, 1); // Assuming this function exists in GameMode
	}
}

void AWidgetController::RemoveWorkerFromResource_Implementation(EResourceType ResourceType, int TeamId)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, -1); // Assuming this function exists in GameMode
	}
}