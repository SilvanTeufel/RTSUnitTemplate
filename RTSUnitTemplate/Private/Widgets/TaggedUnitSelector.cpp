// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/TaggedUnitSelector.h"

#include "Controller/PlayerController/CustomControllerBase.h"
#include "Widgets/TaggedUnitButton.h" 

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