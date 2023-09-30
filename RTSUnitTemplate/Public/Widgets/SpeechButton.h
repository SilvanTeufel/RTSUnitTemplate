// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/UnitData.h"
#include "Components/Button.h"
#include "SpeechButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoadDelegate, FString, name);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FClickDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSetTableDelegate, int, New_Text_Id);

UCLASS()
class USpeechButton : public UButton
{
	GENERATED_BODY()

public:
	USpeechButton();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int New_Text_Id = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FSetTableDelegate CallbackTableDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UUserWidget* SpeechBubble;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnClick();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTableDataId(int newTextID);
};