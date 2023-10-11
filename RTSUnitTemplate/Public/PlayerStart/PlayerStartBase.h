// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
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
};
