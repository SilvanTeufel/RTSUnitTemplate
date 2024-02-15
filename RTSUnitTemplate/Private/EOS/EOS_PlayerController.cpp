// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EOS/EOS_PlayerController.h"
#include "Engine/World.h"
#include "EOS/EOS_GameInstance.h"

void AEOS_PlayerController::OnNetCleanup(UNetConnection* Connection)
{
	UEOS_GameInstance* GameInstanceRef = Cast<UEOS_GameInstance>(GetWorld()->GetGameInstance());
	if(GameInstanceRef)
	{
		GameInstanceRef->DestroySession();
	}
	Super::OnNetCleanup(Connection);

	
}
