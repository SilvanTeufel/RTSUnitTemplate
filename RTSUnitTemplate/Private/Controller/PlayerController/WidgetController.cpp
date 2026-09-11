// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/PlayerController/WidgetController.h"
#include "System/AbilityTemplateSubsystem.h"
#include "Actors/WinLoseConfigActor.h"
#include "GameModes/RTSGameModeBase.h"
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
	if (!RTSGameMode) return;
	
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
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if(Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
			Unit->LoadAbilityAndLevelData(SlotName);
	}
}

void AWidgetController::LoadLevelUnitByTag_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;

	// Find the target unit and get its tags
	int TeamId = 0;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and load level data
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTag && (Unit->TeamId == TeamId))
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LevelUpUnit_Implementation(const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
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
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex) // && IsLocalController()
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and level up
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTag && (Unit->TeamId == TeamId))
		{
			Unit->LevelUp();
		}
	}
}

void AWidgetController::ResetTalentsUnit_Implementation(const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			Unit->ResetTalents();
		}
	}
}

void AWidgetController::ResetTalentsUnitByTag_Implementation(const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex) //  && IsLocalController()
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and reset talents
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTag && (Unit->TeamId == TeamId))
		{
			Unit->ResetTalents();
		}
	}
}

void AWidgetController::HandleInvestmentUnit_Implementation(const int32 UnitIndex, int32 InvestmentState)
{
	if (!RTSGameMode) return;
	
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

void AWidgetController::HandleInvestmentUnitByTag(const int32 UnitIndex, int32 InvestmentState) // const int32 UnitIndex,
{
	HandleInvestmentUnitByTagServer(UnitIndex, InvestmentState);
}

void AWidgetController::HandleInvestmentUnitByTagServer_Implementation(const int32 UnitIndex, int32 InvestmentState) // const int32 UnitIndex,
{

	if (!RTSGameMode) return;
	
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

	FGameplayTag TargetTags;
	int TeamId = 0;
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			TargetTags = Unit->TalentTag; // FGameplayTagContainer UnitTags; Get the Tags from here
			TeamId = Unit->TeamId;
			break;
		}
	}
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTags && (Unit->TeamId == TeamId))
		{
			Invest(Unit);
		}
	}

}

void AWidgetController::SpendAbilityPoints_Implementation(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			Unit->SpendAbilityPoints(AbilityID, Ability);
		}
	}
}

void AWidgetController::SpendAbilityPointsByTag_Implementation(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Hat der Chooser eine Tierklasse gewaehlt, gilt DIESE - sonst waere die Auswahl fuer
	// alle vier Reiter dieselbe, weil der TalentTag an der angeklickten Einheit haengt.
	if (ChooserTierTag.IsValid())
	{
		TargetTag = ChooserTierTag;
	}

	// Vergabe ohne Punktvorgabe (02.09.2026): der AbilityChooser ist eine Vorlage je Klasse,
	// keine Ausgabe von Punkten. Frueher wurden punktlose Einheiten stillschweigend
	// uebersprungen - dann galt die Wahl fuer einen Teil der Klasse und fuer den Rest nicht.
	int32 Gesetzt = 0, Unveraendert = 0;
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		// Beide Tag-Begriffe zulassen: der TalentTag beschreibt den Einheitentyp, die
		// Tierklasse steht in UnitTags. ALevelUnit::DoesAttributeTreeNodeMatchUnit prueft
		// ebenfalls beides.
		if (Unit && (Unit->TalentTag == TargetTag || Unit->UnitTags.HasTag(TargetTag))
			&& (Unit->TeamId == TeamId))
		{
			if (Unit->ApplyAbilityFromTemplate(AbilityID, Ability)) ++Gesetzt;
			else ++Unveraendert;
		}
	}

	// Die Wahl merken, damit UAbilityTemplateProcessor sie auf spaeter gebaute Einheiten
	// nachtraegt. Ohne das musste der Spieler nach jeder neuen Einheit erneut klicken.
	if (UWorld* Welt = GetWorld())
	{
		if (UAbilityTemplateSubsystem* Vorlagen = Welt->GetSubsystem<UAbilityTemplateSubsystem>())
		{
			Vorlagen->Merken(TeamId, TargetTag, Ability, AbilityID);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Tiervergabe] Team %d Tag=%s Slot=%d: %d Einheiten gesetzt, %d schon so."),
		TeamId, *TargetTag.ToString(), Ability, Gesetzt, Unveraendert);
}

void AWidgetController::ResetAbility_Implementation(const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			Unit->ResetAbility();
		}
	}
}

void AWidgetController::ResetAbilityByTag_Implementation(const int32 UnitIndex)
{
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and reset ability
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTag && (Unit->TeamId == TeamId))
		{
			Unit->ResetAbility();
		}
	}
}

void AWidgetController::SaveAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			Unit->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LoadAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	if (!RTSGameMode) return;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AWidgetController::LoadAbilityByTag_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	if (!RTSGameMode) return;
	
	FGameplayTag TargetTag;
	int TeamId = 0;
	// Find the target unit and get its tags
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex)
		{
			TargetTag = Unit->TalentTag;
			TeamId = Unit->TeamId;
			break;
		}
	}

	// Iterate through all units to find those with matching tags and load ability data
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->TalentTag == TargetTag && (Unit->TeamId == TeamId))
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

// ------------------------------------------------------------------------------------------
// Entwickler-Hilfe: die aktuelle Siegbedingung erfuellen (siehe UCheatWidget).
// ------------------------------------------------------------------------------------------
void AWidgetController::Server_CheatWinCurrentLevel_Implementation()
{
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Sieg nicht ausloesbar: kein RTSGameModeBase"));
		return;
	}

	ACameraControllerBase* CamPC = Cast<ACameraControllerBase>(this);
	AWinLoseConfigActor* Config =
		AWinLoseConfigActor::GetWinLoseConfigForTeam(GetWorld(), SelectableTeamId);
	if (!Config)
	{
		Config = GameMode->WinLoseConfigActor;
	}

	if (!CamPC || !Config)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Sieg nicht ausloesbar: PC=%d Config=%d"),
			CamPC ? 1 : 0, Config ? 1 : 0);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Siegbedingung erfuellt fuer Team %d"), SelectableTeamId);
	GameMode->TriggerWinLoseForPlayer(CamPC, true, Config);
}
