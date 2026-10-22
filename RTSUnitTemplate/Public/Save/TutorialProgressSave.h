// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TutorialProgressSave.generated.h"

/** How often each tutorial started by itself on this machine (ATutorialActor::MaxAutoStarts). */
UCLASS()
class RTSUNITTEMPLATE_API UTutorialProgressSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("TutorialProgress");

	/** Key: ATutorialActor::ProgressKey (default: map name). */
	UPROPERTY()
	TMap<FString, int32> AutoStarts;
};
