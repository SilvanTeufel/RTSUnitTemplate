// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "EOS/SessionItem.h"
#include "SessionList.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API USessionList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	UListView* MyListView;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Class")
	TSubclassOf<UUserWidget> ListItemClass;
	
	virtual void NativeConstruct() override;

	UFUNCTION()
	void UpdateListView();

private:
	FTimerHandle UpdateTimerHandle;
};
