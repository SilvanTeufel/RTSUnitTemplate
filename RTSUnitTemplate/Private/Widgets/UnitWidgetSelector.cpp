// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitWidgetSelector.h"
#include "Characters/Unit/UnitBase.h"
#include "Containers/Set.h"
#include "GAS/GameplayAbilityBase.h"
#include "AbilitySystemComponent.h"



void UUnitWidgetSelector::NativeConstruct()
{
	Super::NativeConstruct();
	
	GetButtonsFromBP();
	SetButtonIds();
	SetVisibleButtonCount(ShowButtonCount);
	SetButtonLabelCount(ShowButtonCount);;
}


FText UUnitWidgetSelector::ReplaceRarityKeywords(
	FText OriginalText,
	FText NewPrimary,
	FText NewSecondary,
	FText NewTertiary,
	FText NewRare,
	FText NewEpic,
	FText NewLegendary)
{
	FString TextString = OriginalText.ToString();

	// Perform replacements using Unreal's string operations
	TextString.ReplaceInline(TEXT("Primary"), *NewPrimary.ToString());
	TextString.ReplaceInline(TEXT("Secondary"), *NewSecondary.ToString());
	TextString.ReplaceInline(TEXT("Tertiary"), *NewTertiary.ToString());
	TextString.ReplaceInline(TEXT("Rare"), *NewRare.ToString());
	TextString.ReplaceInline(TEXT("Epic"), *NewEpic.ToString());
	TextString.ReplaceInline(TEXT("Legendary"), *NewLegendary.ToString());

	return FText::FromString(TextString);
}

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
		
		bool bHasSelected = false;
		if (ControllerBase)
		{
			if (ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex))
			{
				if (ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex])
				{
					bHasSelected = true;
				}
			}
		}
		if (bHasSelected)
		{
			AUnitBase* CurrentUnit = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];
			if (ControllerBase->AbilityArrayIndex == 0 && CurrentUnit->DefaultAbilities.Num())
				ChangeAbilityButtonCount(CurrentUnit->DefaultAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 1 && CurrentUnit->SecondAbilities.Num())
				ChangeAbilityButtonCount(CurrentUnit->SecondAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 2 && CurrentUnit->ThirdAbilities.Num())
				ChangeAbilityButtonCount(CurrentUnit->ThirdAbilities.Num());
			else if (ControllerBase->AbilityArrayIndex == 3 && CurrentUnit->FourthAbilities.Num())
				ChangeAbilityButtonCount(CurrentUnit->FourthAbilities.Num());
			else
				ChangeAbilityButtonCount(0);
		}

		UpdateAbilityButtonsState();
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
	}
	else
	{
		const float Denom = (UnitBase->CastTime > KINDA_SMALL_NUMBER) ? UnitBase->CastTime : 1.f;
		float Percent = FMath::Clamp(UnitBase->UnitControlTimer / Denom, 0.f, 1.f);
		CurrentAbilityTimerBar->SetPercent(Percent);
		CurrentAbilityTimerBar->SetFillColorAndOpacity(CurrentAbilityTimerBarColor);
		// Hide the bar if casting hasn't started yet or has completed
		if (Percent <= 0.f || Percent >= 1.f)
		{
			CurrentAbilityTimerBar->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			CurrentAbilityTimerBar->SetVisibility(ESlateVisibility::Visible);
		}
	}

	FQueuedAbility CurrentSnapshot = UnitBase->GetCurrentSnapshot();
	if (CurrentSnapshot.AbilityClass)
	{
		// Get the default object to read its icon
		UGameplayAbilityBase* AbilityCDO = CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();

		if (AbilityCDO && AbilityCDO->AbilityIcon)
		{
			CurrentAbilityIcon->SetBrushFromTexture(
							AbilityCDO->AbilityIcon, true
						);
			
			CurrentAbilityButton->SetVisibility(ESlateVisibility::Visible);

		}
		else
		{
			// Hide the icon or set to a default if no ability or icon is set
			CurrentAbilityButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}else
	{
		// Hide the icon or set to a default if no ability or icon is set
		CurrentAbilityButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	
}

/**
 * STEP 3: The Client receives the data from the server and updates the UI.
 */
void UUnitWidgetSelector::SetWidgetCooldown(int32 AbilityIndex, float RemainingTime)
{
    if (RemainingTime > 0.f)
    {
        // Format to one decimal place for a smoother countdown look
        FString CooldownStr = FString::Printf(TEXT("%.0f"), RemainingTime); 
        if (AbilityCooldownTexts.IsValidIndex(AbilityIndex))
        {
            AbilityCooldownTexts[AbilityIndex]->SetText(FText::FromString(CooldownStr));
        }
    }
    else
    {
        // Cooldown is finished, clear the text or set to a "Ready" indicator
        if (AbilityCooldownTexts.IsValidIndex(AbilityIndex))
        {
            AbilityCooldownTexts[AbilityIndex]->SetText(FText::GetEmpty()); // Or FText::FromString(TEXT("✓"))
        }
    }
}


void UUnitWidgetSelector::UpdateAbilityCooldowns()
{

	if (!ControllerBase || !ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex))
		return;
	
	
	TArray<TSubclassOf<UGameplayAbilityBase>> AbilityArray = ControllerBase->GetAbilityArrayByIndex();
	

	AUnitBase* SelectedUnit = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];

	// Loop through each ability up to AbilityButtonCount
	for (int32 AbilityIndex = 0; AbilityIndex < MaxAbilityButtonCount; ++AbilityIndex)
	{
		if (!AbilityArray.IsValidIndex(AbilityIndex))
		{
			return;
		}

		UGameplayAbilityBase* Ability = AbilityArray[AbilityIndex]->GetDefaultObject<UGameplayAbilityBase>();
		if (!Ability)
		{
			return;
		}
		
		ControllerBase->Server_RequestCooldown(SelectedUnit, AbilityIndex, Ability);
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

	AUnitBase* Unit = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];

	
	if (!Unit) return;

	ControllerBase->DeQueAbility(Unit, ButtonIndex);

	// Refresh UI
	UpdateQueuedAbilityIcons();
}


void UUnitWidgetSelector::OnCurrentAbilityButtonClicked()
{
	if (!ControllerBase) return;
	if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;

	AUnitBase* UnitBase = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];
	
	ControllerBase->CancelCurrentAbility(UnitBase);
}


void UUnitWidgetSelector::StartUpdateTimer()
{
	// Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UUnitWidgetSelector::UpdateSelectedUnits, UpdateInterval, true);
}

void UUnitWidgetSelector::InitWidget(ACustomControllerBase* InController)
{
	if (InController)
	{
		ControllerBase = InController;
		StartUpdateTimer(); // Now it's safe to start the timer
		UE_LOG(LogTemp, Log, TEXT("UnitWidgetSelector Initialized Successfully!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnitWidgetSelector was given an invalid controller!"));
	}
}

void UUnitWidgetSelector::ChangeAbilityButtonCount(int Count)
{
	for (int32 i = 0; i < AbilityButtonWidgets.Num(); i++)
	{
		if (AbilityButtonWidgets[i])
			AbilityButtonWidgets[i]->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	for (int32 i = 0; i < Count; i++)
	{
		if (i < AbilityButtonWidgets.Num() && AbilityButtonWidgets[i])
			AbilityButtonWidgets[i]->SetVisibility(ESlateVisibility::Visible);
	}
}

void UUnitWidgetSelector::GetButtonsFromBP()
{
	AbilityQueWidgets.Empty();
	AbilityQueButtons.Empty();
	AbilityQueIcons.Empty();
	AbilityButtonWidgets.Empty();
	AbilityButtons.Empty();
	AbilityCooldownTexts.Empty();
	SelectButtonWidgets.Empty();
	SelectButtons.Empty();
	SingleSelectButtons.Empty();
	ButtonLabels.Empty();
	UnitIcons.Empty();

	for (int32 i = 0; i <= MaxQueButtonCount; i++)
	{
		FString WidgetName = FString::Printf(TEXT("AbilityQueButtonWidget_%d"), i);
		UUserWidget* Widget = Cast<UUserWidget>(GetWidgetFromName(FName(*WidgetName)));
		if (Widget)
		{
			AbilityQueWidgets.Add(Widget);
			
			FString AbilityQueButtonName = FString::Printf(TEXT("AbilityQueButton"));
			UButton* AbilityQueButton = Cast<UButton>(Widget->GetWidgetFromName(FName(*AbilityQueButtonName)));
			AbilityQueButtons.Add(AbilityQueButton);

			FString AbilityQueIconName = FString::Printf(TEXT("AbilityQueIcon"));
			UImage* IconImage = Cast<UImage>(Widget->GetWidgetFromName(FName(*AbilityQueIconName)));
			AbilityQueIcons.Add(IconImage);
		}
	}

	for (int32 i = 0; i <= MaxAbilityButtonCount; i++)
	{
		FString WidgetName = FString::Printf(TEXT("AbilityButtonWidget_%d"), i);
		UUserWidget* Widget = Cast<UUserWidget>(GetWidgetFromName(FName(*WidgetName)));
		if (Widget)
		{
			AbilityButtonWidgets.Add(Widget);
			
			FString AbilityButtonName = FString::Printf(TEXT("AbilityButton"));
			UButton* AbilityButton = Cast<UButton>(Widget->GetWidgetFromName(FName(*AbilityButtonName)));
			AbilityButtons.Add(AbilityButton);

			FString TextBlockName = FString::Printf(TEXT("AbilityCooldownText"));
			UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(FName(*TextBlockName)));
			AbilityCooldownTexts.Add(TextBlock);
		}
		
	}
	
	for (int32 i = 0; i <= MaxButtonCount; i++)
	{
		FString WidgetName = FString::Printf(TEXT("SelectButtonWidget_%d"), i);
		UUserWidget* Widget = Cast<UUserWidget>(GetWidgetFromName(FName(*WidgetName)));
		if (Widget)
		{
			SelectButtonWidgets.Add(Widget);
			
			FString ButtonName = FString::Printf(TEXT("SelectButton"));
			USelectorButton* Button = Cast<USelectorButton>(Widget->GetWidgetFromName(FName(*ButtonName)));
			SelectButtons.Add(Button);

			FString SingleButtonName = FString::Printf(TEXT("SingleSelectButton"));
			USelectorButton* SingleButton = Cast<USelectorButton>(Widget->GetWidgetFromName(FName(*SingleButtonName)));
			SingleSelectButtons.Add(SingleButton);

			FString TextBlockName = FString::Printf(TEXT("TextBlock"));
			UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(FName(*TextBlockName)));
			ButtonLabels.Add(TextBlock);

			FString IconName = FString::Printf(TEXT("UnitIcon"));
			UImage* IconImage = Cast<UImage>(Widget->GetWidgetFromName(FName(*IconName)));
			UnitIcons.Add(IconImage);
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

void UUnitWidgetSelector::SetVisibleButtonCount(int32 /*Count*/)
{
	// Show only one SelectButtonWidget per squad (SquadId > 0). Solo units (SquadId <= 0) always show.
	// We keep button Ids aligned with SelectedUnits indices so clicks still target the correct unit.
	TSet<int32> SeenSquads;
	int32 VisibleCount = 0;

	for (int32 i = 0; i < SelectButtonWidgets.Num(); i++)
	{
		bool bShow = false;
		bool bIndexValid = false;
		if (ControllerBase)
		{
			const int32 SelCount = ControllerBase->SelectedUnits.Num();
			if (i >= 0 && i < SelCount)
			{
				bIndexValid = true;
			}
		}
		if (bIndexValid)
		{
			AUnitBase* Unit = ControllerBase->SelectedUnits[i];
			if (Unit)
			{
				const int32 SquadId = Unit->SquadId;
				if (SquadId > 0)
				{
					if (!SeenSquads.Contains(SquadId))
					{
						SeenSquads.Add(SquadId);
						bShow = true; // first appearance of this squad
					}
				}
				else
				{
					bShow = true; // solo unit
				}
			}
		}

		if (SelectButtonWidgets[i])
		{
			SelectButtonWidgets[i]->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bShow) { ++VisibleCount; }
		}
	}

	// Toggle overall widget/name visibility based on whether anything is shown
	const bool bAnyVisible = VisibleCount > 0;
	if (Name)
	{
		Name->SetVisibility(bAnyVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	SetVisibility(bAnyVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UUnitWidgetSelector::SetButtonLabelCount(int32 Count)
{
	// Überprüfe, ob die Anzahl der Labels mit der Anzahl der Buttons übereinstimmt
	if (ButtonLabels.Num() != SelectButtons.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("Die Anzahl der Labels stimmt nicht mit der Anzahl der Buttons überein!"));
		return;
	}

	// Build labels using compact, sequential indices across visible groups
	const int32 SelCount = (ControllerBase) ? ControllerBase->SelectedUnits.Num() : 0;
	// Clear all labels first
	for (int32 i = 0; i < ButtonLabels.Num(); ++i)
	{
		if (ButtonLabels[i]) ButtonLabels[i]->SetText(FText::GetEmpty());
	}
	if (!ControllerBase || SelCount <= 0)
	{
		return;
	}

	TSet<int32> VisitedSelectionIndices; // SelectedUnits indices already grouped
	int32 NextCompactIndex = 0; // Sequential numbering across visible buttons

	for (int32 i = 0; i < SelCount; ++i)
	{
		if (VisitedSelectionIndices.Contains(i)) continue;
		AUnitBase* Unit = ControllerBase->SelectedUnits[i];
		if (!Unit) { VisitedSelectionIndices.Add(i); continue; }

		const int32 SquadId = Unit->SquadId;
		TArray<int32> MemberCompactIndices; // what to print on the label
		TSet<AUnitBase*> GroupUnitsSeen;     // deduplicate same unit within the squad group
		
		if (SquadId > 0)
		{
			// Collect all squad members from first occurrence onwards, but avoid counting duplicates
			for (int32 j = i; j < SelCount; ++j)
			{
				AUnitBase* U = ControllerBase->SelectedUnits[j];
				if (U && U->SquadId == SquadId)
				{
					// Mark this selection index as visited so we don't start a new group for it later
					VisitedSelectionIndices.Add(j);
					// Only increment compact indices once per unique unit pointer
					if (!GroupUnitsSeen.Contains(U))
					{
						GroupUnitsSeen.Add(U);
						MemberCompactIndices.Add(NextCompactIndex++);
					}
				}
			}
		}
		else
		{
			VisitedSelectionIndices.Add(i);
			// Solo unit: still guard against accidental duplicates of the same pointer
			if (!GroupUnitsSeen.Contains(Unit))
			{
				GroupUnitsSeen.Add(Unit);
				MemberCompactIndices.Add(NextCompactIndex++);
			}
		}

		// Set label only on the first (visible) entry of this group
		if (ButtonLabels.IsValidIndex(i) && ButtonLabels[i])
		{
			FString LabelText;
			for (int32 k = 0; k < MemberCompactIndices.Num(); ++k)
			{
				if (k > 0) { LabelText += TEXT(" / "); }
				LabelText += FString::FromInt(MemberCompactIndices[k]);
			}
			ButtonLabels[i]->SetText(FText::FromString(LabelText));
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

void UUnitWidgetSelector::UpdateAbilityButtonsState()
{
	if (!ControllerBase) return;
	if (!ControllerBase->SelectedUnits.IsValidIndex(ControllerBase->CurrentUnitWidgetIndex)) return;

	AUnitBase* Unit = ControllerBase->SelectedUnits[ControllerBase->CurrentUnitWidgetIndex];
	if (!Unit) return;

	const int32 TeamId = Unit->TeamId;
	UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
	TArray<TSubclassOf<UGameplayAbilityBase>> AbilityArray = ControllerBase->GetAbilityArrayByIndex();

	// Use the larger of the two arrays to ensure we cover all buttons and widgets
	int32 MaxIndex = FMath::Max(AbilityButtons.Num(), AbilityButtonWidgets.Num());

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		UButton* Btn = AbilityButtons.IsValidIndex(i) ? AbilityButtons[i] : nullptr;
		UUserWidget* Widget = AbilityButtonWidgets.IsValidIndex(i) ? AbilityButtonWidgets[i] : nullptr;
		
		if (!Btn && !Widget) continue;

		bool bEnable = false;
		if (AbilityArray.IsValidIndex(i) && AbilityArray[i])
		{
			UGameplayAbilityBase* AbilityCDO = AbilityArray[i]->GetDefaultObject<UGameplayAbilityBase>();
			if (AbilityCDO)
			{
				bool bCDO_Disabled = AbilityCDO->bDisabled;
				const FString RawKey = AbilityCDO->AbilityKey;
				const FString NormalizedKey = NormalizeAbilityKey(RawKey);

				bool bTeamKeyDisabled = false;
				bool bTeamForceEnabled = false;
				bool bOwnerKeyDisabled = false;
				bool bOwnerForceEnabled = false;
				const bool bHasKey = !NormalizedKey.IsEmpty();
				if (bHasKey)
				{
					bTeamKeyDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(RawKey, TeamId);
					bTeamForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForTeam(RawKey, TeamId);
					if (ASC)
					{
						bOwnerKeyDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForOwner(ASC, RawKey);
						bOwnerForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForOwner(ASC, RawKey);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[UI] UpdateAbilityButtonsState: Missing ASC for Unit %s"), *GetNameSafe(Unit));
					}
				}

				// Apply precedence: OwnerForce > OwnerDisable > TeamForce > (AssetDisabled or TeamDisable)
				if (bOwnerForceEnabled)
				{
					bEnable = true;
				}
				else if (bOwnerKeyDisabled)
				{
					bEnable = false;
				}
				else if (bTeamForceEnabled)
				{
					bEnable = true;
				}
				else if (bCDO_Disabled || bTeamKeyDisabled)
				{
					bEnable = false;
				}
				else
				{
					bEnable = true;
				}

				UE_LOG(LogTemp, Log, TEXT("[UI] AbilityBtnIndex=%d TeamId=%d RawKey='%s' NormKey='%s' bCDO_Disabled=%s bTeamKeyDisabled=%s bOwnerKeyDisabled=%s bTeamForce=%s bOwnerForce=%s -> SetEnabled=%s"),
					i,
					TeamId,
					*RawKey,
					*NormalizedKey,
					bCDO_Disabled ? TEXT("true") : TEXT("false"),
					bTeamKeyDisabled ? TEXT("true") : TEXT("false"),
					bOwnerKeyDisabled ? TEXT("true") : TEXT("false"),
					bTeamForceEnabled ? TEXT("true") : TEXT("false"),
					bOwnerForceEnabled ? TEXT("true") : TEXT("false"),
					bEnable ? TEXT("true") : TEXT("false"));
			}
		}

		if (Btn) Btn->SetIsEnabled(bEnable);
		if (Widget) Widget->SetIsEnabled(bEnable);
	}
}
