// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

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
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category=RTSUnitTemplate)
	UListView* MyListView;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	TSubclassOf<UUserWidget> ListItemClass;
	
	virtual void NativeConstruct() override;

	UFUNCTION()
	void UpdateListView();

private:
	FTimerHandle UpdateTimerHandle;
};
