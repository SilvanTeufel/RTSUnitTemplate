// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitWidgetSelector.h"


void UUnitWidgetSelector::NativeConstruct()
{
	Super::NativeConstruct();
	
	GetButtonsFromBP();
	SetButtonIds();
	SetVisibleButtonCount(ShowButtonCount);
	SetButtonLabelCount(ShowButtonCount);
	ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	/*
	if (ControllerBase)
	{
		ControllerBase->SetInputMode(FInputModeGameAndUI());
		ControllerBase->bShowMouseCursor = true;
	}*/
	StartUpdateTimer();
}
/*
void UUnitWidgetSelector::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if(ControllerBase)
	{
		SetVisibleButtonCount(ControllerBase->SelectedUnitCount);
		SetButtonLabelCount(ControllerBase->SelectedUnitCount);
	}
}*/
void UUnitWidgetSelector::UpdateSelectedUnits()
{
	if(ControllerBase)
	{
		SetVisibleButtonCount(ControllerBase->SelectedUnitCount);
		SetButtonLabelCount(ControllerBase->SelectedUnitCount);

		if(ControllerBase && ControllerBase->SelectedUnits.Num() && ControllerBase->SelectedUnits[0] && ControllerBase->SelectedUnits[0]->DefaultAbilities.Num())
			ChangeAbilityButtonCount(ControllerBase->SelectedUnits[0]->DefaultAbilities.Num());
	}
}

void UUnitWidgetSelector::StartUpdateTimer()
{
	// Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UUnitWidgetSelector::UpdateSelectedUnits, UpdateInterval, true);
}

void UUnitWidgetSelector::ChangeAbilityButtonCount(int Count)
{
	for (int32 i = 0; i < AbilityButtons.Num(); i++)
	{
		if(AbilityButtons[i])
			AbilityButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	for (int32 i = 0; i < Count; i++)
	{
		if(i < AbilityButtons.Num() && AbilityButtons[i])
			AbilityButtons[i]->SetVisibility(ESlateVisibility::Visible);
	}
}

void UUnitWidgetSelector::GetButtonsFromBP()
{
	for (int32 i = 0; i <= MaxAbilityButtonCount; i++)
	{
		FString AbilityButtonName = FString::Printf(TEXT("Button_Ability_%d"), i);
		UButton* AbilityButton = Cast<UButton>(GetWidgetFromName(FName(*AbilityButtonName)));
		if (AbilityButton)
		{
			AbilityButtons.Add(AbilityButton);
		}
	}
	
	for (int32 i = 0; i <= MaxButtonCount; i++)
	{
		FString ButtonName = FString::Printf(TEXT("SelectorButton_%d"), i);
		USelectorButton* Button = Cast<USelectorButton>(GetWidgetFromName(FName(*ButtonName)));
		if (Button)
		{
			SelectButtons.Add(Button);
		}

		FString SingleButtonName = FString::Printf(TEXT("SingleSelectorButton_%d"), i);
		USelectorButton* SingleButton = Cast<USelectorButton>(GetWidgetFromName(FName(*SingleButtonName)));
		if (SingleButton)
		{
			SingleSelectButtons.Add(SingleButton);
		}

		FString TextBlockName = FString::Printf(TEXT("TextBlock_%d"), i);
		UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
		if (TextBlock)
		{
			ButtonLabels.Add(TextBlock);
		}
	}
}

void UUnitWidgetSelector::SetButtonColours(int AIndex)
{
	FLinearColor GreyColor =  FLinearColor( 0.33f , 0.33f , 0.33f, 1.f);
	FLinearColor BlueColor =  FLinearColor( 0.f , 0.f , 1.f, 0.5f);
	for (int32 i = 0; i < SelectButtons.Num(); i++)
	{
		if (SelectButtons[i] && i == AIndex)
		{
			SelectButtons[i]->SetColorAndOpacity(GreyColor);
			SelectButtons[i]->SetBackgroundColor(BlueColor);
		}else if(SelectButtons[i] && i != AIndex)
		{
			SelectButtons[i]->SetColorAndOpacity(BlueColor);
			SelectButtons[i]->SetBackgroundColor(GreyColor);
		}
	}
}

void UUnitWidgetSelector::SetButtonIds()
{
	for (int32 i = 0; i < SelectButtons.Num(); i++)
	{
		if (SelectButtons[i])
		{
			SelectButtons[i]->Id = i;
			SelectButtons[i]->Selector = this;
			SelectButtons[i]->SelectUnit = false;
			
			// Bind the OnClick event
			SelectButtons[i]->OnClicked.AddUniqueDynamic(SelectButtons[i], &USelectorButton::OnClick);
		}
	}

	for (int32 i = 0; i < SingleSelectButtons.Num(); i++)
	{
		if (SingleSelectButtons[i])
		{
			SingleSelectButtons[i]->Id = i;
			SingleSelectButtons[i]->Selector = this;
			SingleSelectButtons[i]->SelectUnit = true;

			// Bind the OnClick event
			SingleSelectButtons[i]->OnClicked.AddUniqueDynamic(SingleSelectButtons[i], &USelectorButton::OnClick);
		}
	}
}


void UUnitWidgetSelector::SetVisibleButtonCount(int32 Count)
{

	
	for (int32 i = 0; i < SelectButtons.Num(); i++)
	{
		if (SelectButtons[i])
		{
			if (i >= Count)
			{
				SelectButtons[i]->SetVisibility(ESlateVisibility::Hidden);
				SingleSelectButtons[i]->SetVisibility(ESlateVisibility::Hidden);
			}
			else
			{
				
				SelectButtons[i]->SetVisibility(ESlateVisibility::Visible);
				SingleSelectButtons[i]->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
	
	if(!Count)
	{
		Name->SetVisibility(ESlateVisibility::Hidden);
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		Name->SetVisibility(ESlateVisibility::Visible);
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UUnitWidgetSelector::SetButtonLabelCount(int32 Count)
{
	// Überprüfe, ob die Anzahl der Labels mit der Anzahl der Buttons übereinstimmt

	
	if (ButtonLabels.Num() != SelectButtons.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("Die Anzahl der Labels stimmt nicht mit der Anzahl der Buttons überein!"));
		return;
	}

	// Iteriere über die Button-Labels und beschrifte sie von 1 bis Count
	for (int32 i = 0; i < ButtonLabels.Num(); i++)
	{
		if (SelectButtons[i])
		{
			if (i < Count)
			{
				FString LabelText = FString::Printf(TEXT("%d"), SelectButtons[i]->Id);
				ButtonLabels[i]->SetText(FText::FromString(LabelText));
			}
			else
			{
				ButtonLabels[i]->SetText(FText::GetEmpty());
			}
		}
	}
}

