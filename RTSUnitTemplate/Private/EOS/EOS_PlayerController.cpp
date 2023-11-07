// Fill out your copyright notice in the Description page of Project Settings.


#include "EOS/EOS_PlayerController.h"

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
