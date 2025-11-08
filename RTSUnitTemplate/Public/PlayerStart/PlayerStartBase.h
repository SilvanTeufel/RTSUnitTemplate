// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "Actors/Waypoint.h"
#include "Widgets/ResourceWidget.h"
#include "Widgets/TaggedUnitSelector.h"
#include "Widgets/UnitWidgetSelector.h"
#include "PlayerStartBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API APlayerStartBase : public APlayerStart
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= RTSUnitTemplate)
	int SelectableTeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= RTSUnitTemplate)
	bool bIsAi = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWaypoint* DefaultWaypoint;
	/*
	// Blueprint‐selectable custom PlayerController class (should be a child of ACameraControllerBase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate")
	TSubclassOf<class ACameraControllerBase> CustomPlayerControllerClass;

	// Blueprint‐selectable custom Pawn class (should be a child of ACameraBase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate")
	TSubclassOf<class ACameraBase> CustomPawnClass;
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* WaypointSound; // Replace in PlayerController

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* RunSound; // Replace in PlayerController

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* AbilitySound; // Replace in PlayerController

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* AttackSound; // Replace in PlayerController

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DropWorkAreaFailedSound; // Replace in PlayerController

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DropWorkAreaSound; // Replace in PlayerController
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	UUnitWidgetSelector* SelectorWidget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	UTaggedUnitSelector* TaggedUnitSelector;  // Replace in Pawn
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	UResourceWidget* ResourceWidget;  // Replace in Pawn
};
