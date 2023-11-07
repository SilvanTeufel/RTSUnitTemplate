// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h" 
#include "EOS_GameInstance.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UEOS_GameInstance : public UGameInstance
{
	GENERATED_BODY()

private:
	TSharedPtr<const FUniqueNetId> StoredUserId;
public:
	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void LoginWithEOS(FString ID, FString Token, FString LoginType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="RTSUnitTemplate")
	FString GetPlayerUserName();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="RTSUnitTemplate")
	bool IsPlayerLoggedIn();
	
	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void FindSessionAndJoin();
	
	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void DestroySession();
	
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	UPROPERTY(BlueprintReadWrite)
	TArray<FString> SearchNames;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName RTSSessionName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName RTSSessionSearchKey;
	
	UFUNCTION(BlueprintCallable, Category = "Session")
	void SetRTSSessionName(const FText& NewSessionName)
	{
		RTSSessionName = FName(*NewSessionName.ToString());
		RTSSessionSearchKey = FName(*NewSessionName.ToString());
		// Log the new RTSSessionName value
		UE_LOG(LogTemp, Log, TEXT("RTSSessionName set to: %s"), *RTSSessionName.ToString());
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="EOSVariables")
	FString OpenLevelText;

	UFUNCTION(BlueprintCallable, Category="RTSUnitTemplate")
	void CreateEosSession(bool bIsDedicatedServer, bool bIsLanServer, int32 NumberOfPublicConnections);

	void LoginWithEos_Return(int32 LocalUserNum, bool bWasSuccess, const FUniqueNetId& UserId, const FString& Error);
	void OnFindSessionCompleted(bool bWasSucces);
	void OnCreateSessionCompleted(FName SessionName, bool bWasSuccesful);
	void OnDestroySessionCompleted(FName SessionName, bool bWasSuccesful);
	void OnJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};
