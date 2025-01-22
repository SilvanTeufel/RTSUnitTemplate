// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitWidgetSelector.h"



void UUnitWidgetSelector::NativeConstruct()
{
	Super::NativeConstruct();
	
	GetButtonsFromBP();
	SetButtonIds();
	SetVisibleButtonCount(ShowButtonCount);
	SetButtonLabelCount(ShowButtonCount);
	ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());
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
		SetUnitIcons(ControllerBase->SelectedUnits);

		Update(ControllerBase->AbilityArrayIndex);
		
		if (!ControllerBase->SelectedUnits.Num()) return;
		if (ControllerBase->CurrentUnitWidgetIndex < 0 || ControllerBase->CurrentUnitWidgetIndex >= ControllerBase->SelectedUnits.Num()) return;
		if (!ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]) return;
		if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;
		
		if(ControllerBase && ControllerBase->SelectedUnits.Num() && ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex])
		{
			if (ControllerBase->AbilityArrayIndex == 0 && ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->DefaultAbilities.Num())
				ChangeAbilityButtonCount(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->DefaultAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 1 && ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->SecondAbilities.Num())
				ChangeAbilityButtonCount(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->SecondAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 2 && ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->ThirdAbilities.Num())
				ChangeAbilityButtonCount(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->ThirdAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 3 && ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->FourthAbilities.Num())
				ChangeAbilityButtonCount(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->FourthAbilities.Num());
			else
			    ChangeAbilityButtonCount(0);
			
			//Update(ControllerBase->AbilityArrayIndex);
		}

		UpdateAbilityCooldowns();
		UpdateCurrentAbility();
		UpdateQueuedAbilityIcons();
	}
}

void UUnitWidgetSelector::UpdateCurrentAbility()
{
	if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;
	AUnitBase* UnitBase = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];
	
	if (!UnitBase) return;

	if (UnitBase->GetUnitState() != UnitData::Casting)
	{
		CurrentAbilityTimerBar->SetVisibility(ESlateVisibility::Hidden);
	
	}else
	{
		CurrentAbilityTimerBar->SetVisibility(ESlateVisibility::Visible);
		CurrentAbilityTimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->CastTime);
		CurrentAbilityTimerBar->SetFillColorAndOpacity(CurrentAbilityTimerBarColor);
	}

	// Now update the current ability icon

		if (UnitBase->ActivatedAbilityInstance && 
			UnitBase->ActivatedAbilityInstance->AbilityIcon)
		{
			// Set the brush from the texture
			CurrentAbilityIcon->SetBrushFromTexture(
				UnitBase->ActivatedAbilityInstance->AbilityIcon, true
			);

	
			CurrentAbilityButton->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			// Hide the icon or set to a default if no ability or icon is set
			CurrentAbilityButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	
}
void UUnitWidgetSelector::UpdateAbilityCooldowns()
{

    if (!ControllerBase || !ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex))
        return;
	
	// ControllerBase->AbilityArrayIndex == 0 -> ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->DefaultAbilities
	// ControllerBase->AbilityArrayIndex == 1 -> ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->SecondAbilities	
	// ControllerBase->AbilityArrayIndex == 2 -> ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->ThirdAbilities
	// ControllerBase->AbilityArrayIndex == 3 -> ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]->FourthAbilities	
	AUnitBase* SelectedUnit = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];
	UAbilitySystemComponent* ASC = SelectedUnit->GetAbilitySystemComponent();
	if (!ASC)
		return;


	// Loop through each ability up to AbilityButtonCount
	for (int32 AbilityIndex = 0; AbilityIndex < MaxAbilityButtonCount; ++AbilityIndex)
	{
		TArray<TSubclassOf<UGameplayAbilityBase>> AbilityArray = ControllerBase->GetAbilityArrayByIndex();

	
		if (!AbilityArray.IsValidIndex(AbilityIndex)) // Adjust index as needed
			return;
	
		UGameplayAbilityBase* Ability = AbilityArray[AbilityIndex]->GetDefaultObject<UGameplayAbilityBase>();
		if (!Ability)
			return;
		
		UGameplayEffect* CooldownGEInstance =  Ability->GetCooldownGameplayEffect();
		if (!CooldownGEInstance)
			return;

		// Step 2: Extract the UClass* from the instance
		TSubclassOf<UGameplayEffect> CooldownGEClass = CooldownGEInstance->GetClass();
		if (!CooldownGEClass)
			return;
		
		       if (!CooldownGEClass)
                {
                    // If class extraction fails, clear the cooldown text
                    if (AbilityCooldownTexts.IsValidIndex(AbilityIndex))
                    {
                        AbilityCooldownTexts[AbilityIndex]->SetText(FText::GetEmpty());
                    }
                    continue;
                }
		// Create a query to find active cooldown effects matching the CooldownGEClass
		FGameplayEffectQuery CooldownQuery;
		CooldownQuery.EffectDefinition = CooldownGEClass;

		// Retrieve all active cooldown effects that match the query
		TArray<FActiveGameplayEffectHandle> ActiveCooldownHandles = ASC->GetActiveEffects(CooldownQuery);

	
		float RemainingTime = 0.f;

		if (ActiveCooldownHandles.Num() > 0)
		{
			// Assuming only one cooldown effect per ability, take the first handle
			FActiveGameplayEffectHandle ActiveHandle = ActiveCooldownHandles[0];
			const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(ActiveHandle);
			if (ActiveEffect)
			{
				// Get the current world time
				float CurrentTime = ASC->GetWorld()->GetTimeSeconds();
        
				// Calculate the remaining duration
				RemainingTime = ActiveEffect->GetTimeRemaining(CurrentTime);
			}
		}

		if (RemainingTime > 0.f)
		{
			//FString CooldownStr = FString::Printf(TEXT("%.1f"), FMath::RoundToFloat(RemainingTime * 10) / 10.0f);
			FString CooldownStr = FString::Printf(TEXT("%.0f"), RemainingTime);
			if (AbilityCooldownTexts.IsValidIndex(AbilityIndex))
			{
				AbilityCooldownTexts[AbilityIndex]->SetText(FText::FromString(CooldownStr));
			}
		}
		else
		{
			if (AbilityCooldownTexts.IsValidIndex(AbilityIndex))
			{
				FString CooldownRdy = FString::Printf(TEXT("-"));
				AbilityCooldownTexts[AbilityIndex]->SetText(FText::FromString(CooldownRdy));
			}
		}
	}
    
}


void UUnitWidgetSelector::UpdateQueuedAbilityIcons()
{
    if (!ControllerBase) return;
    if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;

    // Cast to your AGASUnit or whichever class holds the queue
    AGASUnit* GASUnit = Cast<AGASUnit>(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]);
    if (!GASUnit) return;

    // Get a snapshot of the queued abilities
    TArray<FQueuedAbility> QueuedAbilities = GASUnit->GetQueuedAbilities();

    // Loop over the “queue icons” you placed in your widget
    for (int32 i = 0; i < AbilityQueIcons.Num(); i++)
    {
        // If we have an icon widget, do something with it
        if (AbilityQueIcons[i])
        {
            if (QueuedAbilities.IsValidIndex(i))
            {
                // The queued ability
                const FQueuedAbility& QueuedAbility = QueuedAbilities[i];
                if (QueuedAbility.AbilityClass)
                {
                    // Get the default object to read its icon
                    UGameplayAbilityBase* AbilityCDO = QueuedAbility.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
                    if (AbilityCDO && AbilityCDO->AbilityIcon)
                    {
                        AbilityQueIcons[i]->SetBrushFromTexture(AbilityCDO->AbilityIcon, true);
                    }
                    else
                    {
                        // If there's no icon, set a blank or some fallback
                        AbilityQueIcons[i]->SetBrushFromTexture(nullptr);
                    }
                }
                else
                {
                    // No ability class? Clear or fallback
                    AbilityQueIcons[i]->SetBrushFromTexture(nullptr);
                }
            }
            else
            {
                // No queued ability at this slot, so clear the icon
                AbilityQueIcons[i]->SetBrushFromTexture(nullptr);
            }
        }

        // (Optional) If you want to adjust the visibility of Buttons as well:
        if (AbilityQueButtons.IsValidIndex(i) && AbilityQueButtons[i])
        {
            bool bHasAbility = QueuedAbilities.IsValidIndex(i) && QueuedAbilities[i].AbilityClass != nullptr;
            AbilityQueButtons[i]->SetVisibility(bHasAbility ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }
    }
}

void UUnitWidgetSelector::OnAbilityQueueButtonClicked(int32 ButtonIndex)
{
	// This will remove the *front* of the queue, ignoring ButtonIndex
	// if you want to truly remove only the front item from TQueue.
    
	if (!ControllerBase) return;
	if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;

	AGASUnit* GASUnit = Cast<AGASUnit>(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]);
	if (!GASUnit) return;
	
	if (GASUnit->DequeueAbility(ButtonIndex))
	{
		// success—front item removed
	}

	// Refresh UI
	UpdateQueuedAbilityIcons();
}


void UUnitWidgetSelector::OnCurrentAbilityButtonClicked()
{
	if (!ControllerBase) return;
	if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;

	AUnitBase* UnitBase = Cast<AUnitBase>(ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex]);
	if (!UnitBase) return;
	
	UnitBase->CancelCurrentAbility();
	UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
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
/*
	FString CurrentAbilityIconName = FString::Printf(TEXT("CurrentAbilityIcon"));
	if (UImage* ImageWidget = Cast<UImage>(GetWidgetFromName(FName(*CurrentAbilityIconName))))
		CurrentAbilityIcon = ImageWidget;
	//class UImage* CurrentAbilityIcon;

	FString CurrentAbilityButtonName = FString::Printf(TEXT("CurrentAbilityButton"));
	if (UButton* AbilityButton = Cast<UButton>(GetWidgetFromName(FName(*CurrentAbilityButtonName))))
		CurrentAbilityButton = AbilityButton;

		
	FString CurrentAbilityTimerBarName = FString::Printf(TEXT("CurrentAbilityTimerBar"));
	if (UProgressBar* AbilityProgressbar= Cast<UProgressBar>(GetWidgetFromName(FName(*CurrentAbilityTimerBarName))))
		CurrentAbilityTimerBar = AbilityProgressbar;
*/
	
	
	for (int32 i = 0; i <= MaxQueButtonCount; i++)
	{
		FString AbilityQueButtonName = FString::Printf(TEXT("AbilityQueButtons_%d"), i);
		UButton* AbilityQueButton = Cast<UButton>(GetWidgetFromName(FName(*AbilityQueButtonName)));
		if (AbilityQueButton)
		{
			AbilityQueButtons.Add(AbilityQueButton);
		}

		FString AbilityQueIconName = FString::Printf(TEXT("AbilityQueIcons_%d"), i);
		if (UImage* Image = Cast<UImage>(GetWidgetFromName(FName(*AbilityQueIconName))))
		{
			AbilityQueIcons.Add(Image);
		}
	}
	
	for (int32 i = 0; i <= MaxAbilityButtonCount; i++)
	{
		FString AbilityButtonName = FString::Printf(TEXT("Button_Ability_%d"), i);
		UButton* AbilityButton = Cast<UButton>(GetWidgetFromName(FName(*AbilityButtonName)));
		if (AbilityButton)
		{
			AbilityButtons.Add(AbilityButton);
		}

		FString TextBlockName = FString::Printf(TEXT("AbilityCooldownText_%d"), i);
		UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
		if (TextBlock)
		{
			AbilityCooldownTexts.Add(TextBlock);
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

		FString IconName = FString::Printf(TEXT("UnitIcon_%d"), i);
		if (UImage* Image = Cast<UImage>(GetWidgetFromName(FName(*IconName))))
		{
			UnitIcons.Add(Image);
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
		if (SelectButtons[i] && SingleSelectButtons[i])
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

void UUnitWidgetSelector::SetUnitIcons(TArray<AUnitBase*>& Units)
{
	// If there are no units at all, bail early
	if (Units.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("No Units Available!"));
		return;
	}
    
	// Figure out how many icons we can actually set
	const int32 Count = FMath::Min(Units.Num(), UnitIcons.Num());
    
	// Iterate and set each icon
	for (int32 i = 0; i < Count; i++)
	{
		// Safety checks: ensure the array slot in UnitIcons is valid,
		// and that the Unit has a valid Texture2D in UnitIcon
		if (UnitIcons[i] && Units[i] && Units[i]->UnitIcon)
		{
			// Set the brush of the UImage to the texture from your AUnitBase
			UnitIcons[i]->SetBrushFromTexture(Units[i]->UnitIcon, true);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Could not set icon for unit index %d"), i);
		}
	}
}