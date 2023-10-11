// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/RTSGameModeBase.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Characters/CameraBase.h"
#include "EngineUtils.h"
#include "Controller/CameraControllerBase.h"
#include "Hud/PathProviderHUD.h"


void ARTSGameModeBase::BeginPlay()
{
	Super::BeginPlay();


	FTimerHandle TimerHandle;
	SetTeamIds();
}

void ARTSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	SetTeamIds();
}

void ARTSGameModeBase::SetTeamId_Implementation(int Id, ACameraControllerBase* CameraControllerBase)
{

	if(CameraControllerBase)
	{
		CameraControllerBase->SetControlerTeamId(Id);
	}
	
}

void ARTSGameModeBase::SetTeamIds_Implementation()
{
	TArray<APlayerStartBase*> PlayerStarts;
	for (TActorIterator<APlayerStartBase> It(GetWorld()); It; ++It)
	{
		PlayerStarts.Add(*It);
	}

	int32 PlayerStartIndex = 0;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		
		AController* PlayerController = It->Get();
		ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(PlayerController);

		if (CameraControllerBase && PlayerStarts.IsValidIndex(PlayerStartIndex))
		{
			APlayerStartBase* CustomPlayerStart = PlayerStarts[PlayerStartIndex];
			SetTeamId_Implementation(CustomPlayerStart->SelectableTeamId, CameraControllerBase);
			PlayerStartIndex++;  // Move to the next PlayerStart for the next iteration
		}
	}
}