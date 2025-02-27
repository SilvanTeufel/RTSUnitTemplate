// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "Actors/Waypoint.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWaypoint* DefaultWaypoint;

	// Blueprint‐selectable custom PlayerController class (should be a child of ACameraControllerBase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate")
	TSubclassOf<class ACameraControllerBase> CustomPlayerControllerClass;

	// Blueprint‐selectable custom Pawn class (should be a child of ACameraBase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUnitTemplate")
	TSubclassOf<class ACameraBase> CustomPawnClass;
};
