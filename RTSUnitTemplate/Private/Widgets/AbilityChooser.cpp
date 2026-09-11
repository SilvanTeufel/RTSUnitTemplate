// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/AbilityChooser.h"
#include "System/AbilityTemplateSubsystem.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "GAS/GameplayAbilityBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Widgets/AbilityButton.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UAbilityChooser::NativeConstruct()
{
    Super::NativeConstruct();
    UsedAbilityPointsTextArray.Empty();
    InitializeButtonArray(ButtonPreFixes[0], OffensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[1], DefensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[2], AttackAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[3], ThrowAbilityButtons);
    InitializeAbilityIconArray("Ability", SelectAbilityIcons, SelectAbilityButtons);
    SetVisibility(ESlateVisibility::Hidden);
}

void UAbilityChooser::InitWidget(ACustomControllerBase* InController)
{
    if (InController)
    {
        ControllerBase = InController;
        StartUpdateTimer();
        UE_LOG(LogTemp, Log, TEXT("UTaggedUnitSelector Initialized Successfully!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UTaggedUnitSelector was given an invalid controller!"));
    }
}


void UAbilityChooser::SetOwnerActor(AAbilityUnit* NewOwner)
{
    OwnerAbilityUnit = NewOwner;
}

void UAbilityChooser::StartUpdateTimer()
{
    // Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
    GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UAbilityChooser::UpdateAbilityDisplay, UpdateInterval, true);
}

void UAbilityChooser::StopTimer()
{
    // Check if the timer is currently active before attempting to clear it
    if (GetWorld()->GetTimerManager().IsTimerActive(UpdateTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
    }
}


void UAbilityChooser::UpdateAbilityDisplay()
{
    // Die Tier-Knoepfe bei jedem Takt nachziehen: ob eine Klasse noch Punkte hat, aendert
    // sich waehrend der Partie staendig.
    RefreshTierButtons();
    RefreshSlotButtons();

    if (OwnerAbilityUnit)
    {
        SetAbilityIcons();
        SetVisibleAbilityButtonCount();
        
        if (OffensiveAbilityText)
        {
            OffensiveAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->OffensiveAbilityID))));
        }
        if (DefensiveAbilityText)
        {
            DefensiveAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->DefensiveAbilityID))));
        }
        if (AttackAbilityText)
        {
            AttackAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->AttackAbilityID))));
        }
        if (ThrowAbilityText)
        {
            ThrowAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->ThrowAbilityID))));
        }
        
        if (AvailableAbilityPointsText)
        {
            AvailableAbilityPointsText->SetText(FText::AsNumber(OwnerAbilityUnit->LevelData.AbilityPoints));
        }

        if (UsedAbilityPointsText)
        {
            UsedAbilityPointsText->SetText(FText::AsNumber(OwnerAbilityUnit->LevelData.UsedAbilityPoints));
        }

        // Update UsedAbilityPointsTextArray
        for (int32 Index = 0; Index < UsedAbilityPointsTextArray.Num(); Index++)
        {
            if (UsedAbilityPointsTextArray[Index])
            {
                int32 UsedPoints = OwnerAbilityUnit->LevelData.UsedAbilityPointsArray.IsValidIndex(Index) ? OwnerAbilityUnit->LevelData.UsedAbilityPointsArray[Index] : 0;
                UsedAbilityPointsTextArray[Index]->SetText(FText::AsNumber(UsedPoints));
            }
        }
    }
}

FString UAbilityChooser::GetEnumValueAsString(const FString& EnumName, int32 EnumValue)
{
    UEnum* Enum = FindObject<UEnum>(nullptr, *EnumName, EFindObjectFlags::ExactClass);
    if (!Enum)
    {
        return FString("Invalid");
    }
    return Enum->GetNameByValue(EnumValue).ToString();
}

void UAbilityChooser::InitializeButtonArray(const FString& ButtonPrefix, TArray<UButton*>& ButtonArray)
{
    ButtonArray.Empty();
    for (int32 Index = 0; Index < ButtonInitCount; ++Index) // Assuming you have 4 buttons per category
    {
        FString ButtonName = FString::Printf(TEXT("%s_%d"), *ButtonPrefix, Index);
        UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName)));

        if (Button)
        {
            ButtonArray.Add(Button);
        }
    }
    
    FString TextBlockName = FString::Printf(TEXT("UsedAbilityPointArray_%s"), *ButtonPrefix); // Modify this based on your actual naming convention
    UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
    if (TextBlock)
    {
        UsedAbilityPointsTextArray.Add(TextBlock);
    }
}


void UAbilityChooser::ClearAbilityArray()
{
    if (!OwnerAbilityUnit || !ControllerBase)
    {
        return;
    }

    // Pick which array to clear in-place using a reference
    TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities =
        (ControllerBase->AbilityArrayIndex == 1) ? OwnerAbilityUnit->SecondAbilities :
        (ControllerBase->AbilityArrayIndex == 2) ? OwnerAbilityUnit->ThirdAbilities  :
        (ControllerBase->AbilityArrayIndex == 3) ? OwnerAbilityUnit->FourthAbilities :
                            OwnerAbilityUnit->DefaultAbilities;

    // Clear the selected array
    Abilities.Empty();
    
    // If you need to do anything else after clearing, do it here...
   // UE_LOG(LogTemp, Log, TEXT("Cleared Ability array #%d"), ControllerBase->AbilityArrayIndex);
}




void UAbilityChooser::SetAbilityIcons()
{
    // If there are no units at all, bail early
    if (!OwnerAbilityUnit) return;

    
    // Figure out how many icons we can actually set
    const int32 Count = OwnerAbilityUnit->SelectableAbilities.Num();
    
    // Iterate and set each icon
    for (int32 i = 0; i < Count; i++)
    {
        UTexture2D* AbilityIcon = OwnerAbilityUnit->SelectableAbilities[i]->GetDefaultObject<UGameplayAbilityBase>()->AbilityIcon;
        // Safety checks: ensure the array slot in UnitIcons is valid,
        // and that the Unit has a valid Texture2D in UnitIcon
        if (SelectAbilityIcons[i])
        {
            // Set the brush of the UImage to the texture from your AUnitBase
            SelectAbilityIcons[i]->SetBrushFromTexture(AbilityIcon, true);
        }
        else
        {
           // UE_LOG(LogTemp, Warning, TEXT("Could not set icon for unit index %d"), i);
        }
    }
}

void UAbilityChooser::SetVisibleAbilityButtonCount()
{

    if (!OwnerAbilityUnit) return;

    // Figure out how many icons we can actually set
    const int32 Count = OwnerAbilityUnit->SelectableAbilities.Num();

    const int32 MaxIndex = FMath::Min(SelectAbilityButtons.Num(), 
                                     SelectAbilityIcons.Num());

    //UE_LOG(LogTemp, Warning, TEXT("SelectAbilityButtons.Num() %d"), SelectAbilityButtons.Num());
    //UE_LOG(LogTemp, Warning, TEXT("SelectAbilityIcons.Num() %d"), SelectAbilityIcons.Num());
    //UE_LOG(LogTemp, Warning, TEXT("SelectableAbilityCount %d"), SelectableAbilityCount);
    for (int32 i = 0; i < MaxIndex; i++)
    {
        if (SelectAbilityButtons.IsValidIndex(i) && 
            SelectAbilityIcons.IsValidIndex(i))
        {
            if (i >= Count)
            {
                //UE_LOG(LogTemp, Warning, TEXT("Hide %d"), i);
                SelectAbilityButtons[i]->SetVisibility(ESlateVisibility::Hidden);
                SelectAbilityIcons[i]->SetVisibility(ESlateVisibility::Hidden);
            }
            else
            {
                //UE_LOG(LogTemp, Warning, TEXT("Visible %d"), i);
                SelectAbilityButtons[i]->SetVisibility(ESlateVisibility::Visible);
                SelectAbilityIcons[i]->SetVisibility(ESlateVisibility::Visible);
            }
        }
    }
}

void UAbilityChooser::InitializeAbilityIconArray(const FString& Prefix, TArray<UImage*>& IconArray, TArray<UAbilityButton*>& ButtonArray)
{

    ButtonArray.Empty();
    for (int32 Index = 0; Index < SelectableAbilityCount; ++Index) // Assuming you have 4 buttons per category
    {
        FString ButtonName = FString::Printf(TEXT("%s_%d"), *Prefix, Index);
        UAbilityButton* Button = Cast<UAbilityButton>(GetWidgetFromName(FName(*ButtonName)));
   
        if (Button)
        {
            Button->OnClicked.AddUniqueDynamic(Button, &UAbilityButton::OnClick);
            Button->Id = Index;
            ButtonArray.Add(Button);
        }
    }
    
    // Clear the array to start fresh
    IconArray.Empty();

    
    // Loop through however many icons you plan to fetch per category
    for (int32 Index = 0; Index < SelectableAbilityCount; ++Index)
    {
        // Construct the widget name based on a prefix + index
        FString IconName = FString::Printf(TEXT("%sIcon_%d"), *Prefix, Index);
        
        // Get the widget by name and cast it to UImage
        UImage* FoundIcon = Cast<UImage>(GetWidgetFromName(FName(*IconName)));
        if (FoundIcon)
        {
            IconArray.Add(FoundIcon);
        }
    }
}

void UAbilityChooser::SetSelectedTier(int32 Tier)
{
	SelectedTier = FMath::Clamp(Tier, 1, 4);
	// Dem Controller mitteilen, welche Klasse gemeint ist - die Slot-Knoepfe laufen ueber
	// AWidgetController::SpendAbilityPointsByTag und wuessten es sonst nicht.
	if (ControllerBase)
	{
		ControllerBase->ChooserTierTag = GetSelectedTierTag();
	}
	UpdateAbilityDisplay();
	RefreshTierButtons();
	RefreshSlotButtons();
}

void UAbilityChooser::WendeAuswahlrahmenAn(UButton* Knopf, bool bGewaehlt)
{
	if (!Knopf)
	{
		return;
	}

	FButtonStyle Stil = Knopf->GetStyle();

	// RoundedBox, weil NUR diese Zeichenart einen Rahmen kennt. Ein Bild- oder Kastenpinsel
	// ignoriert OutlineSettings stillschweigend - eine der Fallen, die "sieht aus wie vorher"
	// erzeugen, ohne dass irgendwo ein Fehler steht.
	auto Setze = [this, bGewaehlt](FSlateBrush& Pinsel, float Helligkeit)
	{
		Pinsel.DrawAs = ESlateBrushDrawType::RoundedBox;
		Pinsel.TintColor = FSlateColor(SlotFillColor * Helligkeit);

		Pinsel.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Pinsel.OutlineSettings.CornerRadii = FVector4(SlotCornerRadius, SlotCornerRadius,
			SlotCornerRadius, SlotCornerRadius);
		Pinsel.OutlineSettings.Width = bGewaehlt ? SelectionOutlineWidth : 1.f;

		FLinearColor Rahmen = bGewaehlt ? AccentColor : FLinearColor(0.18f, 0.20f, 0.24f, 1.f);
		Rahmen.A = 1.f;
		Pinsel.OutlineSettings.Color = FSlateColor(Rahmen);
	};

	// Der Grund bleibt in beiden Zustaenden dunkel; unterschieden wird ueber den Rahmen. Beim
	// Ueberfahren und Druecken wird der Grund etwas heller, damit der Knopf sich noch anfuehlt.
	Setze(Stil.Normal,  1.0f);
	Setze(Stil.Hovered, 1.6f);
	Setze(Stil.Pressed, 2.2f);

	// Ohne eigenen Disabled-Pinsel wird ein gesperrter Knopf in diesem Projekt UNSICHTBAR statt
	// grau - dieselbe Falle wie bei den Faehigkeitenknoepfen im HUD.
	Stil.Disabled = Stil.Normal;
	Stil.Disabled.TintColor = FSlateColor(FLinearColor(0.10f, 0.11f, 0.13f, 0.60f));

	Knopf->SetStyle(Stil);
}

void UAbilityChooser::RefreshTierButtons()
{
	// Ueber GetWidgetFromName statt BindWidget: die Leiste gibt es nur in den beiden
	// Fraktions-Widgets. So laeuft der Code in allen anderen Choosern einfach ins Leere,
	// statt beim Kompilieren ein fehlendes Pflichtfeld zu melden.
	for (int32 Tier = 1; Tier <= 4; ++Tier)
	{
		if (UWidget* Knopf = GetWidgetFromName(*FString::Printf(TEXT("TierButton_%d"), Tier)))
		{
			// Immer bedienbar (02.09.2026): der Chooser ist eine Vorlage, keine Ausgabe von
			// Punkten. Frueher stand hier IsTierSpendable(Tier) - dann war die Klasse genau so
			// lange nicht waehlbar, wie keine ihrer Einheiten gerade Punkte hatte.
			Knopf->SetIsEnabled(true);

			// Auch hier war die gewaehlte Klasse bisher nicht zu erkennen - derselbe Rahmen wie
			// bei den Slots, damit beide Auswahlen dieselbe Sprache sprechen.
			if (UButton* AlsKnopf = Cast<UButton>(Knopf))
			{
				WendeAuswahlrahmenAn(AlsKnopf, Tier == SelectedTier);
			}

			if (UTextBlock* Beschriftung = Cast<UTextBlock>(
					GetWidgetFromName(*FString::Printf(TEXT("TierLabel_%d"), Tier))))
			{
				Beschriftung->SetColorAndOpacity(FSlateColor(Tier == SelectedTier
					? FLinearColor::White
					: FLinearColor(0.55f, 0.60f, 0.66f, 1.f)));
			}
		}
	}

	if (UTextBlock* Hinweis = Cast<UTextBlock>(GetWidgetFromName(TEXT("TierHintLabel"))))
	{
		Hinweis->SetText(TierHintText);
	}
}

FGameplayTag UAbilityChooser::GetSelectedTierTag() const
{
	// RequestGameplayTag ohne ErrorIfNotFound: die vier Tags stehen in DefaultGameplayTags.ini,
	// ein fehlender waere aber kein Grund fuer eine Zusicherung.
	return FGameplayTag::RequestGameplayTag(
		FName(*FString::Printf(TEXT("Units.Tier.%d"), FMath::Clamp(SelectedTier, 1, 4))), false);
}

bool UAbilityChooser::IsTierSpendable(int32 Tier) const
{
	if (!ControllerBase)
	{
		return false;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
		FName(*FString::Printf(TEXT("Units.Tier.%d"), FMath::Clamp(Tier, 1, 4))), false);
	return ControllerBase->TierHasUnitsWithAbilityPoints(Tag);
}

void UAbilityChooser::SpendAbilityForSelectedTier(EGASAbilityInputID AbilityID, int32 AbilityIndex)
{
	if (!ControllerBase)
	{
		return;
	}
	const FGameplayTag Tag = GetSelectedTierTag();
	if (!Tag.IsValid())
	{
		return;
	}
	ControllerBase->Server_SpendAbilityPointsForTier(Tag, AbilityID, AbilityIndex);
}


void UAbilityChooser::RefreshSlotButtons()
{
	static const TCHAR* Gruppen[] = { TEXT("Offensive"), TEXT("Defensive"),
	                                  TEXT("Attack"), TEXT("Throw") };
	if (!OwnerAbilityUnit)
	{
		return;
	}

	for (int32 G = 0; G < 4; ++G)
	{
		// Reihenfolge wie in SpendAbilityPoints: 0 Offensive, 1 Defensive, 2 Attack, 3 Throw.
		const TArray<TSubclassOf<UGameplayAbilityBase>>* Quelle = nullptr;
		EGASAbilityInputID Gewaehlt = EGASAbilityInputID::None;
		switch (G)
		{
		case 0: Quelle = &OwnerAbilityUnit->OffensiveAbilities; Gewaehlt = OwnerAbilityUnit->OffensiveAbilityID; break;
		case 1: Quelle = &OwnerAbilityUnit->DefensiveAbilities; Gewaehlt = OwnerAbilityUnit->DefensiveAbilityID; break;
		case 2: Quelle = &OwnerAbilityUnit->AttackAbilities;    Gewaehlt = OwnerAbilityUnit->AttackAbilityID;    break;
		default: Quelle = &OwnerAbilityUnit->ThrowAbilities;    Gewaehlt = OwnerAbilityUnit->ThrowAbilityID;     break;
		}

		// Die Hervorhebung gehoert zur gewaehlten TIERKLASSE, nicht zur angeklickten Einheit:
		// sonst zeigen alle vier Reiter dasselbe, weil die Einheit beim Umschalten dieselbe
		// bleibt. Gibt es fuer die Klasse noch keine Vorlage, ist nichts hervorgehoben.
		if (ControllerBase && GetWorld())
		{
			if (const UAbilityTemplateSubsystem* Vorlagen =
					GetWorld()->GetSubsystem<UAbilityTemplateSubsystem>())
			{
				const FTierAbilityTemplate* Vorlage =
					Vorlagen->Hole(ControllerBase->SelectableTeamId, GetSelectedTierTag());
				Gewaehlt = Vorlage ? Vorlage->Hole(G) : EGASAbilityInputID::None;
			}
		}

		for (int32 i = 0; i < 4; ++i)
		{
			UImage* Icon = Cast<UImage>(GetWidgetFromName(
				*FString::Printf(TEXT("SlotIcon_%s_%d"), Gruppen[G], i)));
			UWidget* Knopf = GetWidgetFromName(
				*FString::Printf(TEXT("%s_%d"), Gruppen[G], i));
			if (!Icon && !Knopf)
			{
				continue; // Widget ohne die neuen Slots - nichts zu tun.
			}

			const UGameplayAbilityBase* CDO = nullptr;
			if (Quelle->IsValidIndex(i) && (*Quelle)[i])
			{
				CDO = (*Quelle)[i]->GetDefaultObject<UGameplayAbilityBase>();
			}

			// Nur Knoepfe zeigen, hinter denen auch eine Faehigkeit liegt. Leere Slots
			// waren bisher als Platzhalter sichtbar und liessen sich sinnlos anklicken.
			if (Knopf)
			{
				Knopf->SetVisibility(CDO ? ESlateVisibility::Visible
				                         : ESlateVisibility::Collapsed);
			}
			if (Icon)
			{
				Icon->SetVisibility(CDO ? ESlateVisibility::Visible
				                        : ESlateVisibility::Collapsed);
			}
			if (!CDO)
			{
				continue; // nichts weiter zu setzen
			}

			if (Icon)
			{
				if (CDO && CDO->AbilityIcon)
				{
					Icon->SetBrushFromTexture(CDO->AbilityIcon, false);
					Icon->SetColorAndOpacity(FLinearColor::White);
				}
				else
				{
					// Platzhalter behalten, nur einfaerben - so bleibt das Raster ruhig,
					// auch wenn ein Slot leer ist.
					Icon->SetColorAndOpacity(EmptySlotColor);
				}
			}

			// Die Ziffer verschwindet, sobald ein Icon da ist; ohne Faehigkeit bleibt sie
			// als Orientierung stehen. Die Textbloecke heissen in der Vorlage One/Two/Three/Four
			// mit angehaengtem Index ab dem zweiten Slot.
			{
				static const TCHAR* Basis[] = { TEXT("One"), TEXT("Two"),
				                                TEXT("Three"), TEXT("Four") };
				const FString ZifferName = (i == 0)
					? FString(Basis[G])
					: FString::Printf(TEXT("%s_%d"), Basis[G], i);
				if (UWidget* Ziffer = GetWidgetFromName(*ZifferName))
				{
					const bool bHatIcon = CDO && CDO->AbilityIcon;
					Ziffer->SetVisibility(bHatIcon ? ESlateVisibility::Collapsed
					                               : ESlateVisibility::HitTestInvisible);
				}
			}

			if (Knopf)
			{
				if (CDO)
				{
					FText Tip = CDO->ToolTipText;
					if (Tip.IsEmpty())
					{
						const FString Zusammen = CDO->AbilityName + CDO->Description.ToString();
						Tip = FText::FromString(Zusammen);
					}
					Knopf->SetToolTipText(Tip);
				}
				else
				{
					Knopf->SetToolTipText(NSLOCTEXT("RTSUnitTemplate", "EmptySlot",
						"Empty slot - no ability assigned."));
				}

				// Sichtbare Auswahl.
				//
				// Frueher standen hier nur Deckkraft 0,55 gegen 1,0 und eine Toenung - auf dem
				// vollen Raster aus sechzehn Knoepfen war der Unterschied kaum auszumachen.
				// Jetzt greifen vier Mittel gleichzeitig, damit die Wahl auch im Blickwinkel
				// erkennbar bleibt: gefuellter Hintergrund in der Fraktionsfarbe, volle
				// Deckkraft, weisses statt graues Icon und ein sichtbar groesserer Knopf.
				const EGASAbilityInputID DieserSlot = static_cast<EGASAbilityInputID>(
					static_cast<int32>(EGASAbilityInputID::AbilityOne) + i);
				const bool bIstGewaehlt = (Gewaehlt != EGASAbilityInputID::None)
					&& (Gewaehlt == DieserSlot);

				Knopf->SetRenderOpacity(bIstGewaehlt ? 1.0f : 0.40f);

				// Groesse: der gewaehlte Slot tritt aus dem Raster heraus. Der Drehpunkt liegt
				// in der Mitte, sonst wandert der Knopf beim Skalieren aus seiner Zelle.
				FWidgetTransform Verformung;
				Verformung.Scale = bIstGewaehlt ? FVector2D(1.15f, 1.15f) : FVector2D(1.f, 1.f);
				Knopf->SetRenderTransform(Verformung);
				Knopf->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

				if (UButton* AlsKnopf = Cast<UButton>(Knopf))
				{
					WendeAuswahlrahmenAn(AlsKnopf, bIstGewaehlt);

					// Der Inhalt wird NICHT mehr ueber den Knopf eingefaerbt.
					//
					// SetColorAndOpacity auf dem Knopf multipliziert auf ALLE Kinder durch, also
					// auch auf das Icon. Genau daran lag es, dass die Icons matt wirkten. Gedaempft
					// wird jetzt allein ueber die Deckkraft des Icons.
					AlsKnopf->SetColorAndOpacity(FLinearColor::White);
				}

				// Das Icon ist die Hauptsache: ausgewaehlt voll deckend, sonst deutlich gedaempft.
				if (Icon)
				{
					Icon->SetRenderOpacity(bIstGewaehlt ? 1.0f : 0.45f);
					Icon->SetColorAndOpacity(FLinearColor::White);
				}
			}
		}
	}
}
