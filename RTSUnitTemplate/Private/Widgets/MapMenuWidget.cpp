#include "Widgets/MapMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Controller/PlayerController/CameraControllerBase.h"

void UMapMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Map1Button)
	{
		Map1Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap1Clicked);
	}

	if (Map2Button)
	{
		Map2Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap2Clicked);
	}

	if (ExitButton)
	{
		ExitButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnExitClicked);
	}
}

void UMapMenuWidget::OnMap1Clicked()
{
	if (bAlreadyClicked) return;

	FString MapToTravel = Map1Name.IsNull() ? "" : Map1Name.ToSoftObjectPath().GetLongPackageName();
	if (MapToTravel.IsEmpty()) return;

	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->Server_TravelToMap(MapToTravel, NAME_None);
	}
}

void UMapMenuWidget::OnMap2Clicked()
{
	if (bAlreadyClicked) return;

	FString MapToTravel = Map2Name.IsNull() ? "" : Map2Name.ToSoftObjectPath().GetLongPackageName();
	if (MapToTravel.IsEmpty()) return;

	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->Server_TravelToMap(MapToTravel, NAME_None);
	}
}

void UMapMenuWidget::OnExitClicked()
{
	if (bAlreadyClicked) return;
	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}
