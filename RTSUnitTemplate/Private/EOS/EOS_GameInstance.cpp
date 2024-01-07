// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EOS/EOS_GameInstance.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "EOS/EOS_GameMode.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"



void UEOS_GameInstance::LoginWithEOS(FString ID, FString Token, FString LoginType)
{
	IOnlineSubsystem* SubsystemRef = Online::GetSubsystem(this->GetWorld());
	if(SubsystemRef)
	{
		IOnlineIdentityPtr IdentityPointerRef = SubsystemRef->GetIdentityInterface();
		if(IdentityPointerRef)
		{
			FOnlineAccountCredentials AccountDetails;

			AccountDetails.Id = ID;
			AccountDetails.Token = Token;
			AccountDetails.Type = LoginType;
			IdentityPointerRef->OnLoginCompleteDelegates->AddUObject(this, &UEOS_GameInstance::LoginWithEos_Return);
			IdentityPointerRef->AutoLogin(0);
			IdentityPointerRef->Login(0, AccountDetails);
		}
	}

}

FString UEOS_GameInstance::GetPlayerUserName()
{
	IOnlineSubsystem* SubsystemRef = Online::GetSubsystem(this->GetWorld());
	if(SubsystemRef)
	{
		IOnlineIdentityPtr IdentityPointerRef = SubsystemRef->GetIdentityInterface();
		if(IdentityPointerRef)
		{
			if(IdentityPointerRef->GetLoginStatus(0) == ELoginStatus::LoggedIn)
			{
				return IdentityPointerRef->GetPlayerNickname(0);
			}
		}
	}
	return FString();
}

bool UEOS_GameInstance::IsPlayerLoggedIn()
{
	IOnlineSubsystem* SubsystemRef = Online::GetSubsystem(this->GetWorld());
	if(SubsystemRef)
	{
		IOnlineIdentityPtr IdentityPointerRef = SubsystemRef->GetIdentityInterface();
		if(IdentityPointerRef)
		{
				return IdentityPointerRef->GetLoginStatus(0) == ELoginStatus::LoggedIn;
		}
	}
	return false;
}

void UEOS_GameInstance::DestroySession()
{
	IOnlineSubsystem* SubsystemRef = IOnlineSubsystem::Get();
	if (SubsystemRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("SubsystemRef found."));
		IOnlineSessionPtr SessionPtrRef = SubsystemRef->GetSessionInterface();

		if (SessionPtrRef)
		{
			UE_LOG(LogTemp, Warning, TEXT("SessionPtrRef found."));
			SessionPtrRef->OnDestroySessionCompleteDelegates.AddUObject(this, &UEOS_GameInstance::OnDestroySessionCompleted);
			SessionPtrRef->DestroySession(RTSSessionName);
		}
	}
}

void UEOS_GameInstance::LoginWithEos_Return(int32 LocalUserNum, bool bWasSuccess, const FUniqueNetId& UserId,
											const FString& Error)
{
	UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!!!LoginWithEos_Return!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
	if(bWasSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login Success %d"), LocalUserNum);
		UE_LOG(LogTemp, Warning, TEXT("UserId: %s"), *UserId.ToString());
		// Store the UserId for later use
		StoredUserId = UserId.AsShared();
		UE_LOG(LogTemp, Warning, TEXT("StoredUserId: %s"), *StoredUserId->ToString());
		// Get the world from the game instance.
		UWorld* World = GetWorld();
		if (World)
		{
			// Cast the current game mode to AEOS_GameMode.
			AEOS_GameMode* EOSGameMode = Cast<AEOS_GameMode>(World->GetAuthGameMode());
			if (EOSGameMode)
			{
				// Get the player controller for the given LocalUserNum.
				APlayerController* NewPlayer = UGameplayStatics::GetPlayerController(World, LocalUserNum);
				if (NewPlayer)
				{
					// Call the Register function from the AEOS_GameMode.
					EOSGameMode->Register(NewPlayer);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Could not find PlayerController for LocalUserNum %d"), LocalUserNum);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Current GameMode is not of type AEOS_GameMode."));
			}
		}
	}else
	{
		UE_LOG(LogTemp, Error, TEXT("Login Fail Reason - %s"), *Error);
	}
}


void UEOS_GameInstance::CreateEosSession(bool bIsDedicatedServer, bool bIsLanServer, int32 NumberOfPublicConnections)
{
    IOnlineSubsystem* SubsystemRef = IOnlineSubsystem::Get();
    UE_LOG(LogTemp, Warning, TEXT("CreateEOSSession! "));
    if (SubsystemRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("SubsystemRef found."));
        IOnlineSessionPtr SessionPtrRef = SubsystemRef->GetSessionInterface();

        if (SessionPtrRef)
        {
        	
            UE_LOG(LogTemp, Warning, TEXT("SessionPtrRef found."));
            FOnlineSessionSettings SessionCreationInfo;
        	
        	SessionCreationInfo.bIsDedicated = bIsDedicatedServer;
        	SessionCreationInfo.bAllowInvites = true;
        /*
        	SessionCreationInfo.bAllowJoinInProgress = true;
        	SessionCreationInfo.bUsesStats                            = false;
        	SessionCreationInfo.bAllowJoinViaPresence                = true;
        	SessionCreationInfo.bAllowJoinViaPresenceFriendsOnly    = false;
			SessionCreationInfo.bAntiCheatProtected                = false;
        	SessionCreationInfo.bUseLobbiesVoiceChatIfAvailable    = false;
        	SessionCreationInfo.BuildUniqueId                        = 1;
        	*/
        	SessionCreationInfo.bIsLANMatch = bIsLanServer;
        	SessionCreationInfo.NumPublicConnections = NumberOfPublicConnections;
        	SessionCreationInfo.bAllowJoinViaPresence = true;
        	SessionCreationInfo.bAllowJoinInProgress = true;
        	SessionCreationInfo.bUseLobbiesIfAvailable = true;
        	SessionCreationInfo.bUsesPresence = true;
        	SessionCreationInfo.bShouldAdvertise = true;
        	SessionCreationInfo.Set(RTSSessionSearchKey, RTSSessionName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);

            SessionPtrRef->OnCreateSessionCompleteDelegates.AddUObject(this, &UEOS_GameInstance::OnCreateSessionCompleted);
        
        	AEOS_GameMode* EOSGameMode = Cast<AEOS_GameMode>(GetWorld()->GetAuthGameMode());
        	if (EOSGameMode)
        	{
        		
        			bool bCreationStarted = SessionPtrRef->CreateSession(0, RTSSessionName, SessionCreationInfo);
        			if (bCreationStarted)
        			{
        				SessionCreationExecuted = true;
        				UE_LOG(LogTemp, Warning, TEXT("Session creation process started! %d"), 	SessionCreationInfo.GetID(RTSSessionSearchKey));
        			}
        			else
        			{
        				UE_LOG(LogTemp, Error, TEXT("Failed to start session creation process!"));
        			}
        	
        	}
        }
    }
}



void UEOS_GameInstance::OnCreateSessionCompleted(FName SessionName, bool bWasSuccesful)
{
	if(bWasSuccesful)
	{
		// Convert SessionName to a string
		FString SessionNameString = SessionName.ToString();
		//SessionJoined = true;
		// Log the SessionName
		UE_LOG(LogTemp, Warning, TEXT("Creation was Successful! Session Name: %s"), *SessionNameString);
		GetWorld()->ServerTravel(OpenLevelText);
		//GetWorld()->ServerTravel(TEXT("/RTSUnitTemplate/RTSUnitTemplate/Level/LevelTwo_Multiplayer.LevelTwo_Multiplayer?listen"));

	}else
	{
		UE_LOG(LogTemp, Error, TEXT("Creation went wrong!"));
		QuitSession();
	}
	
}

void UEOS_GameInstance::OnDestroySessionCompleted(FName SessionName, bool bWasSuccesful)
{
	
}

void UEOS_GameInstance::FindSessionAndJoin()
{
	IOnlineSubsystem* SubsystemRef = IOnlineSubsystem::Get();
	if (SubsystemRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("SubsystemRef found."));
		IOnlineSessionPtr SessionPtrRef = SubsystemRef->GetSessionInterface();

		if (SessionPtrRef)
		{
			UE_LOG(LogTemp, Warning, TEXT("SessionPtrRef found."));
			SessionJoined = true;
			SessionSearch = MakeShareable(new FOnlineSessionSearch());
			//SessionSearch->QuerySettings.Set(TEXT("SEARCH_LOBBIES"), false, EOnlineComparisonOp::Equals);
			//SessionSearch->QuerySettings.SearchParams.Empty();
			//SessionSearch->QuerySettings.Set(FName(TEXT("__EOS_bListening")), false,EOnlineComparisonOp::Equals);
			//SessionSearch->QuerySettings.Set( FName(TEXT("SEARCH_KEYWORDS")),  FString(TEXT("String")), EOnlineComparisonOp::NotEquals);
			SessionSearch->MaxSearchResults = 5000;
			SessionSearch->QuerySettings.Set( RTSSessionSearchKey,  RTSSessionName.ToString(), EOnlineComparisonOp::Equals);
			SessionSearch->QuerySettings.Set( FName(TEXT("LOBBYSEARCH")),  true, EOnlineComparisonOp::Equals);
			//SessionSearch->bIsLanQuery = false;
			//SessionSearch->MaxSearchResults = 20;
			
			//SessionSearch->QuerySettings.SearchParams.Empty();
			SessionPtrRef->OnFindSessionsCompleteDelegates.AddUObject(this, &UEOS_GameInstance::OnFindSessionCompleted);
			SessionPtrRef->FindSessions(0, SessionSearch.ToSharedRef());
		}
	}
}

void UEOS_GameInstance::OnFindSessionCompleted(bool bWasSucces)
{
	UE_LOG(LogTemp, Warning, TEXT("OnFindSessionCompleted! "));
	if(bWasSucces)
	{
		// Assuming you have some sort of UI to display the found sessions to the user
		IOnlineSubsystem* SubsystemRef = IOnlineSubsystem::Get();
		UE_LOG(LogTemp, Warning, TEXT("OnFindSessionCompleted Successful! "));
		if (SubsystemRef)
		{
			UE_LOG(LogTemp, Warning, TEXT("SubsystemRef found."));
			IOnlineSessionPtr SessionPtrRef = SubsystemRef->GetSessionInterface();
			SessionPtrRef->ClearOnFindSessionsCompleteDelegates(this);
			
			if (SessionPtrRef)
			{
				UE_LOG(LogTemp, Warning, TEXT("SessionPtrRef found."));
				UE_LOG(LogTemp, Warning, TEXT("Number of sessions found: %d"), SessionSearch->SearchResults.Num());
		
				if(SessionSearch->SearchResults.Num())
				{
					/*
					for (int32 i = 0; i < SessionSearch->SearchResults.Num(); ++i)
					{
						SearchNames[i] = SessionSearch->SearchResults[i].GetSessionIdStr();
						UE_LOG(LogTemp, Log, TEXT("SearchNames[i] : %s"), *SearchNames[i]);
					}*/
					UE_LOG(LogTemp, Warning, TEXT("SearchResult is Valid."));
					// AFTER HERE IT IS JOIN!
					SessionPtrRef->OnJoinSessionCompleteDelegates.AddUObject(this, &UEOS_GameInstance::OnJoinSessionCompleted);
					SessionPtrRef->JoinSession(0, RTSSessionName, SessionSearch->SearchResults[0]);
				}else
				{
					UE_LOG(LogTemp, Warning, TEXT("Create your own Session?!?!?"));
					//CreateEosSession(false, false, 10);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Finding session failed. Lets Create one"));
		//CreateEosSession(false, false, 10);
	}
}

void UEOS_GameInstance::OnJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	switch(Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		{
		UE_LOG(LogTemp, Log, TEXT("Successfully joined session!!!!: %s"), *SessionName.ToString());
		
				UE_LOG(LogTemp, Warning, TEXT("Got PlayerController!"));
				FString ConnectInfo;
				IOnlineSubsystem* SubsystemRef = IOnlineSubsystem::Get();
				UE_LOG(LogTemp, Warning, TEXT("OnFindSessionCompleted Successful! "));
				if (SubsystemRef)
				{
					UE_LOG(LogTemp, Warning, TEXT("SubsystemRef found."));
					IOnlineSessionPtr SessionPtrRef = SubsystemRef->GetSessionInterface();

					if (SessionPtrRef)
					{
						SessionPtrRef->GetResolvedConnectString(RTSSessionName, ConnectInfo);
						UE_LOG(LogTemp, Warning, TEXT("ConnectInfo: %s"), *ConnectInfo);
						if(!ConnectInfo.IsEmpty())
						{
							UE_LOG(LogTemp, Warning, TEXT("Found Address!!!!!!"));
							if(APlayerController* PlayerControllerRef = UGameplayStatics::GetPlayerController(GetWorld(), 0))
							{
								UE_LOG(LogTemp, Warning, TEXT("Client Travel!!!!!!"));
								PlayerControllerRef->ClientTravel(ConnectInfo, TRAVEL_Absolute);
							}
						}
					}
				}
			
		// Navigate to the session map or perform any other necessary actions upon successful join
		}
		break;
        
	case EOnJoinSessionCompleteResult::SessionIsFull:
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to join session %s: Session is full."), *SessionName.ToString());
		}
		break;
        
		// ... handle other result types as necessary
        
		default:
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to join session %s: Unknown error."), *SessionName.ToString());
			}
		break;
	}
}

void UEOS_GameInstance::QuitSession()
{
	UE_LOG(LogTemp, Warning, TEXT("!QuitSession!!!"));
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("!QuitSession! OnlineSubsystem"));
		IOnlineSessionPtr Sessions = OnlineSubsystem->GetSessionInterface();
		if (Sessions.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("!QuitSession! Sessions.IsValid()"));
			if (Sessions->GetNamedSession(RTSSessionName))
			{
				UE_LOG(LogTemp, Warning, TEXT("!QuitSession! and Destory?!?"));
				Sessions->DestroySession(RTSSessionName, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEOS_GameInstance::OnDestroySessionComplete));
			}else
			{
				SessionJoined = false;
				SessionCreationExecuted = false;
			}
		}
	}
}

void UEOS_GameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("!OnDestroySessionComplete! 1?!?"));
		SessionJoined = false;
		SessionCreationExecuted = false;
		// Handle successful session destruction, e.g., navigate to main menu
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("!OnDestroySessionComplete! 1?!?"));
		SessionJoined = false;
		SessionCreationExecuted = false;
		// Handle failed session destruction, e.g., display error message
	}
}