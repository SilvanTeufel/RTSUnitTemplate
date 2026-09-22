// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/TaggedUnitSelector.h"
#include "GameStates/UpgradeGameState.h"

#include "Controller/PlayerController/CustomControllerBase.h"
#include "Widgets/TaggedUnitButton.h"
#include "Components/Button.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Widgets/AttributeTreeWidget.h"
#include "Widgets/AbilityChooser.h"
#include "Characters/Unit/LevelUnit.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "EngineUtils.h"

void UTaggedUnitSelector::NativeConstruct()
{
    Super::NativeConstruct();

    // Die beiden Knoepfe sind optional - ein Blueprint ohne sie ist kein Fehler.
    if (AttributeTreeToggleButton)
    {
        AttributeTreeToggleButton->OnClicked.AddUniqueDynamic(this, &UTaggedUnitSelector::HandleAttributeTreeToggleClicked);
    }
    if (AbilityChooserToggleButton)
    {
        AbilityChooserToggleButton->OnClicked.AddUniqueDynamic(this, &UTaggedUnitSelector::HandleAbilityChooserToggleClicked);
    }
    if (WinConditionButton)
    {
        WinConditionButton->OnClicked.AddUniqueDynamic(this, &UTaggedUnitSelector::HandleWinConditionClicked);
    }
}

void UTaggedUnitSelector::ShowWinCondition()
{
    if (AExtendedCameraBase* Camera = GetOwningCamera())
    {
        Camera->ShowWinConditionWidget(WinConditionDisplaySeconds);
    }
}

void UTaggedUnitSelector::HandleWinConditionClicked()
{
    ShowWinCondition();
}

bool UTaggedUnitSelector::HasSpendableAttributePoints() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Nur das eigene Team. SelectableTeamId ist dieselbe Quelle, aus der auch der
    // Attributbaum seine Anzeigeeinheit waehlt - sonst pulst der Knopf wegen fremder Punkte.
    int32 MyTeam = -1;
    if (const ACameraControllerBase* Camera = Cast<ACameraControllerBase>(GetOwningPlayer()))
    {
        MyTeam = Camera->SelectableTeamId;
    }

    // Hat das Team ueberhaupt etwas zu vergeben? Ohne das pulst der Knopf, obwohl nichts
    // bezahlt werden kann.
    const AUpgradeGameState* GameStateRef = World->GetGameState<AUpgradeGameState>();
    if (!GameStateRef || GameStateRef->GetTeamAttributeTreePoints(MyTeam) <= 0)
    {
        return false;
    }

    for (TActorIterator<ALevelUnit> It(World); It; ++It)
    {
        const ALevelUnit* Unit = *It;
        if (!IsValid(Unit) || !Unit->AttributeTreeDataTable)
        {
            continue;
        }
        if (MyTeam >= 0 && Unit->TeamId != MyTeam)
        {
            continue;
        }
        // Zwei Bedingungen, seit der Vorrat dem Team gehoert: das TEAM muss einen Punkt haben
        // (oben geprueft), und es muss eine Einheit geben, zu der ein noch nicht voller Knoten
        // passt. Ohne die zweite pulste der Knopf dauerhaft, sobald irgendwo Punkte lagen.
        if (Unit->HasInvestableAttributeTreeNode())
        {
            return true;
        }
    }
    return false;
}

void UTaggedUnitSelector::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!AttributeTreeToggleButton)
    {
        return;
    }

    // Die Aktorsuche gedrosselt, der Puls selbst weich.
    CheckTime += InDeltaTime;
    if (CheckTime >= AttributePulseCheckInterval)
    {
        CheckTime = 0.f;
        bPointsSpendable = HasSpendableAttributePoints();
    }

    if (!bPointsSpendable)
    {
        // Nur zuruecksetzen, wenn noetig - sonst schreibt das jeden Frame ins Widget.
        if (!FMath::IsNearlyEqual(PulseTime, 0.f))
        {
            PulseTime = 0.f;
            AttributeTreeToggleButton->SetRenderScale(FVector2D(1.f, 1.f));
            AttributeTreeToggleButton->SetRenderOpacity(1.f);
        }
        return;
    }

    PulseTime += InDeltaTime;
    const float Period = FMath::Max(0.05f, AttributePulseInterval);
    const float Wave = 0.5f * (1.f + FMath::Sin(2.f * PI * PulseTime / Period));

    const float Scale = 1.f + Wave * AttributePulseScale;
    AttributeTreeToggleButton->SetRenderScale(FVector2D(Scale, Scale));
    AttributeTreeToggleButton->SetRenderOpacity(0.70f + 0.30f * Wave);
}

AExtendedCameraBase* UTaggedUnitSelector::GetOwningCamera() const
{
    const APlayerController* PC = GetOwningPlayer();
    return PC ? Cast<AExtendedCameraBase>(PC->GetPawn()) : nullptr;
}

void UTaggedUnitSelector::ToggleAttributeTree()
{
    AExtendedCameraBase* Camera = GetOwningCamera();
    if (!Camera || !Camera->AttributeTreeWidget)
    {
        return;
    }

    const bool bVisible = Camera->AttributeTreeWidget->GetVisibility() != ESlateVisibility::Collapsed
        && Camera->AttributeTreeWidget->GetVisibility() != ESlateVisibility::Hidden;

    Camera->AttributeTreeWidget->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UTaggedUnitSelector::ToggleAbilityChooser()
{
    AExtendedCameraBase* Camera = GetOwningCamera();
    if (!Camera || !Camera->AbilityChooserWidget)
    {
        return;
    }

    const bool bVisible = Camera->AbilityChooserWidget->GetVisibility() != ESlateVisibility::Collapsed
        && Camera->AbilityChooserWidget->GetVisibility() != ESlateVisibility::Hidden;

    Camera->AbilityChooserWidget->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UTaggedUnitSelector::HandleAttributeTreeToggleClicked()
{
    ToggleAttributeTree();
}

void UTaggedUnitSelector::HandleAbilityChooserToggleClicked()
{
    ToggleAbilityChooser();
}

void UTaggedUnitSelector::InitWidget(ACustomControllerBase* InController)
{
    if (InController)
    {
        ControllerBase = InController;

        TMap<UTaggedUnitButton*, FGameplayTag> ButtonToTagMap = 
        {
            { TagButtonCtrl1, ControllerBase->KeyTagCtrl1 },
            { TagButtonCtrl2, ControllerBase->KeyTagCtrl2 },
            { TagButtonCtrl3, ControllerBase->KeyTagCtrl3 },
            { TagButtonCtrl4, ControllerBase->KeyTagCtrl4 },
            { TagButtonCtrl5, ControllerBase->KeyTagCtrl5 },
            { TagButtonCtrl6, ControllerBase->KeyTagCtrl6 },

            { TagButtonAlt1,  ControllerBase->KeyTagAlt1 },
            { TagButtonAlt2,  ControllerBase->KeyTagAlt2 },
            { TagButtonAlt3,  ControllerBase->KeyTagAlt3 },
            { TagButtonAlt4,  ControllerBase->KeyTagAlt4 },
            { TagButtonAlt5,  ControllerBase->KeyTagAlt5 },
            { TagButtonAlt6,  ControllerBase->KeyTagAlt6 },

            { TagButtonCtrlQ, ControllerBase->KeyTagCtrlQ },
            { TagButtonCtrlW, ControllerBase->KeyTagCtrlW },
            { TagButtonCtrlE, ControllerBase->KeyTagCtrlE },
            { TagButtonCtrlR, ControllerBase->KeyTagCtrlR }
        };

        // 2. Iterate over the map.
        for (const TPair<UTaggedUnitButton*, FGameplayTag>& Pair : ButtonToTagMap)
        {
            UTaggedUnitButton* Button = Pair.Key;
            const FGameplayTag& Tag = Pair.Value;

            if (Button)
            {
                // 3. Assign the tag to the button.
                Button->UnitTagToSelect = Tag;
            
                // 4. Bind the delegate as before.
                Button->OnClicked.AddDynamic(this, &UTaggedUnitSelector::HandleTaggedUnitButtonClicked);
            }
        }
        UE_LOG(LogTemp, Log, TEXT("UTaggedUnitSelector Initialized Successfully!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UTaggedUnitSelector was given an invalid controller!"));
    }
}

// This ONE function now handles clicks from all 16 buttons!
void UTaggedUnitSelector::HandleTaggedUnitButtonClicked(FGameplayTag UnitTag)
{
    UE_LOG(LogTemp, Log, TEXT("Tagged button clicked for tag: %s"), *UnitTag.ToString());

    if (ControllerBase && UnitTag.IsValid())
    {
        ControllerBase->SelectUnitsWithTag(UnitTag, ControllerBase->SelectableTeamId);
    }
}