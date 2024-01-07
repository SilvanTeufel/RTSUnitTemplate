// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EOS/EOS_GameMode.h"
#include "EOS/EOS_GameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"  
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/NetConnection.h"
#include "EOS/EOS_GameInstance.h"


void AEOS_GameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
	
	//UEOS_GameInstance* GameInstance = Cast<UEOS_GameInstance>(GetGameInstance());
	//if (GameInstance)
	//{
		//GameInstance->SessionJoined = true;
	//}
	// Set SessionJoined = true; from UEOS_GameInstance
	Register(NewPlayer);
}

void AEOS_GameMode::Register(APlayerController* NewPlayer)
{
   // Super::PostLogin(NewPlayer);
	UE_LOG(LogTemp, Warning, TEXT("Register EOS"));
    if (NewPlayer)
    {
       

        if (NewPlayer->IsLocalController())
        {
            ULocalPlayer* LocalPlayerRef = NewPlayer->GetLocalPlayer();
            if (LocalPlayerRef)
            {
                UniqueNetIdRepl = LocalPlayerRef->GetPreferredUniqueNetId();
            }
        }

        // If we haven't obtained UniqueNetIdRepl from a local player, try to get it from a remote connection.
    	if (!UniqueNetIdRepl.IsValid())
    	{
    		UNetConnection* RemoteNetConnectionRef = Cast<UNetConnection>(NewPlayer->Player);
    		if (IsValid(RemoteNetConnectionRef))
    		{
    			UniqueNetIdRepl = RemoteNetConnectionRef->PlayerId;
    			if (!UniqueNetIdRepl.IsValid())
    			{
    				UE_LOG(LogTemp, Error, TEXT("RemoteNetConnection's PlayerId is invalid!"));
    			}
    		}
    		else
    		{
    			UE_LOG(LogTemp, Error, TEXT("Failed to cast to UNetConnection!"));
    		}
    	}

        // Ensure the UniqueNetIdRepl is valid before trying to get UniqueNetId.
        if (UniqueNetIdRepl.IsValid())
        {
            FUniqueNetIdPtr UniqueNetId = UniqueNetIdRepl.GetUniqueNetId();
            if (UniqueNetId)
            {
                IOnlineSubsystem* SubsystemRef = Online::GetSubsystem(NewPlayer->GetWorld());
                if (SubsystemRef)
                {
                    IOnlineSessionPtr SessionRef = SubsystemRef->GetSessionInterface();

                	
                	UGameInstance* GameInstance = GetGameInstance();
                	if(GameInstance)
                	{
                		UEOS_GameInstance* CustomGameInstance = Cast<UEOS_GameInstance>(GameInstance);
                		if(CustomGameInstance)
                		{
                			FName SessionName = CustomGameInstance->RTSSessionName;
                			bool bRegistrationSuccess = SessionRef->RegisterPlayer(SessionName, *UniqueNetId, false);
                			if (bRegistrationSuccess)
                			{
                				UE_LOG(LogTemp, Warning, TEXT("Registration Successful"));
                			}
                		}
                		else
                		{
                			UE_LOG(LogTemp, Error, TEXT("Failed to cast to YourCustomGameInstanceClass"));
                		}
                	}
                	else
                	{
                		UE_LOG(LogTemp, Error, TEXT("GameInstance is null"));
                	}
                	
                	
                	/*
                    if (SessionRef.IsValid())
                    {
                        bool bRegistrationSuccess = SessionRef->RegisterPlayer(FName("RTSSession"), *UniqueNetId, false);
                        if (bRegistrationSuccess)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Registration Successful"));
                        }
                    }*/
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("UniqueNetId is null!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to obtain a valid UniqueNetIdRepl!"));
        }
    }
}



