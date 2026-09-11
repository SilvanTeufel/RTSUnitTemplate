// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hud/PathProviderHUD.h"
#include "Characters/Camera/CameraBase.h"
#include "Core/UnitData.h"
#include "GameFramework/PlayerController.h"
#include "Actors/EffectArea.h"
#include "Actors/UnitSpawnPlatform.h"
#include "ControllerBase.h"
#include "WidgetController.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AWidgetController : public AControllerBase
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------------------------------
	// Entwickler-Hilfe. Wird vom UCheatWidget benutzt, um die aktuelle Siegbedingung zu erfuellen.
	// Muss ueber den Server laufen: die Siegabwicklung sitzt im GameMode, und den gibt es auf
	// einem Client gar nicht - ein Direktaufruf waere im Netzspiel wirkungslos.
	// ------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RTSUnitTemplate|Cheat")
	void Server_CheatWinCurrentLevel();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SaveLevel(const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LoadLevel(const FString& SlotName);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LevelUp();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ResetTalents();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestment(int32 InvestmentState);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SaveLevelUnit(const int32 UnitIndex, const FString& SlotName);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LoadLevelUnit(const int32 UnitIndex, const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LoadLevelUnitByTag(const int32 UnitIndex, const FString& SlotName);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LevelUpUnit(const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void LevelUpUnitByTag(const int32 UnitIndex);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ResetTalentsUnit(const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void ResetTalentsUnitByTag(const int32 UnitIndex);

	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestmentUnit(const int32 UnitIndex, int32 InvestmentState);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestmentUnitByTagServer(const int32 UnitIndex,int32 InvestmentState);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void HandleInvestmentUnitByTag(const int32 UnitIndex,int32 InvestmentState);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpendAbilityPoints(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpendAbilityPointsByTag(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex);

	/** Die im AbilityChooser gewaehlte Tierklasse (Units.Tier.1-4), vom Widget gesetzt.
	 *
	 *  Ist sie gueltig, schreibt SpendAbilityPointsByTag in die Vorlage DIESER Klasse statt
	 *  in den TalentTag der angeklickten Einheit. Ohne das zeigten und setzten alle vier
	 *  Reiter dasselbe, weil die Einheit beim Umschalten dieselbe bleibt. */
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag ChooserTierTag;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ResetAbility(const int32 UnitIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ResetAbilityByTag(const int32 UnitIndex);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SaveAbility(const int32 UnitIndex, const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LoadAbility(const int32 UnitIndex, const FString& SlotName);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LoadAbilityByTag(const int32 UnitIndex, const FString& SlotName);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetResource(int TeamId, EResourceType RType);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ModifyResource(EResourceType ResourceType, int32 TeamId, float Amount);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void AddWorkerToResource(EResourceType ResourceType, int TeamId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveWorkerFromResource(EResourceType ResourceType, int TeamId);
};

