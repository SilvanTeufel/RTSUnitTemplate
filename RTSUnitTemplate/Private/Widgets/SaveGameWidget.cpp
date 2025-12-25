#include "Widgets/SaveGameWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Actors/SaveGameActor.h"
#include "System/GameSaveSubsystem.h"
#include "Misc/DateTime.h"
#include "Widgets/SaveSlotClickHandler.h"
#include "Widgets/SaveSlotClickHandler.h"
#include "Blueprint/WidgetTree.h"

void USaveGameWidget::InitializeWidget(const FString& InSlotName, ASaveGameActor* InOwningActor)
{
    SlotName = InSlotName;
    OwningActor = InOwningActor;

    if (DialogText)
    {
        DialogText->SetText(FText::FromString(TEXT("Save or Load game: choose a slot name, then Save or Load.")));
    }

    if (SlotNameTextBox)
    {
        SlotNameTextBox->SetText(FText::FromString(SlotName));
    }

    if (YesButton) YesButton->SetVisibility(ESlateVisibility::Visible);
    if (NoButton) NoButton->SetVisibility(ESlateVisibility::Visible);
    if (LoadButton) LoadButton->SetVisibility(ESlateVisibility::Visible);
}

void USaveGameWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (YesButton)
    {
        YesButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnYesClicked);
    }

    if (NoButton)
    {
        NoButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnNoClicked);
    }

    if (LoadButton)
    {
        LoadButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnLoadClicked);
    }

    // Liste jetzt befüllen, da BindWidget-Referenzen gültig sind
    PopulateSavesList();
}

void USaveGameWidget::PopulateSavesList()
{
    if (!SavedSlotsList)
    {
        return;
    }

    SavedSlotsList->ClearChildren();
    // Alte Handler verwerfen, damit keine toten Delegates verbleiben
    SlotClickHandlers.Reset();

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
            {
                const TArray<FString> Slots = SaveSubsystem->GetAllSaveSlots();
                for (const FString& ASlot : Slots)
                {
                    FString MapAssetName;
                    FString MapLong;
                    int64 Unix = 0;
                    const bool bHasSummary = SaveSubsystem->LoadSaveSummary(ASlot, MapAssetName, MapLong, Unix);

                    FString When = TEXT("Unknown time");
                    if (bHasSummary && Unix > 0)
                    {
                        const FDateTime Dt = FDateTime::FromUnixTimestamp(Unix);
                        When = Dt.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
                    }

                    const FString Display = FString::Printf(TEXT("%s  |  Map: %s  |  %s"),
                        *ASlot,
                        bHasSummary ? *MapAssetName : TEXT("<unknown>"),
                        *When);

                    // Button mit Textinhalt erstellen (optional benutzerdefinierte Klassen)
                    UClass* ButtonClassToUse = SaveSlotButtonClass ? SaveSlotButtonClass.Get() : UButton::StaticClass();
                    UClass* LabelClassToUse = SaveSlotLabelClass ? SaveSlotLabelClass.Get() : UTextBlock::StaticClass();

                    UButton* Button = nullptr;
                    UTextBlock* Label = nullptr;

                    if (WidgetTree)
                    {
                        Button = WidgetTree->ConstructWidget<UButton>(ButtonClassToUse);
                        Label = WidgetTree->ConstructWidget<UTextBlock>(LabelClassToUse);
                    }
                    else
                    {
                        Button = NewObject<UButton>(this, ButtonClassToUse);
                        Label = NewObject<UTextBlock>(this, LabelClassToUse);
                    }

                    if (!Button)
                    {
                        Button = NewObject<UButton>(this);
                    }
                    if (!Label)
                    {
                        Label = NewObject<UTextBlock>(this);
                    }

                    Label->SetText(FText::FromString(Display));
                    Button->SetContent(Label);

                    // Click-Handler erzeugen und lebendig halten
                    USaveSlotClickHandler* Handler = NewObject<USaveSlotClickHandler>(this);
                    Handler->Init(this, ASlot);
                    SlotClickHandlers.Add(Handler);

                    // Klick delegieren
                    Button->OnClicked.AddDynamic(Handler, &USaveSlotClickHandler::HandleClick);

                    // Zur Liste hinzufügen
                    SavedSlotsList->AddChild(Button);
                }
            }
        }
    }
}

void USaveGameWidget::OnYesClicked()
{
    FString FinalSlot = SlotName;
    if (SlotNameTextBox)
    {
        const FString Entered = SlotNameTextBox->GetText().ToString().TrimStartAndEnd();
        if (!Entered.IsEmpty())
        {
            FinalSlot = Entered;
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
            {
                SaveSubsystem->SaveCurrentGame(FinalSlot);
            }
        }
    }

    RemoveFromParent();
}

void USaveGameWidget::OnNoClicked()
{
    RemoveFromParent();
}

void USaveGameWidget::OnLoadClicked()
{
    FString FinalSlot = SlotName;
    if (SlotNameTextBox)
    {
        const FString Entered = SlotNameTextBox->GetText().ToString().TrimStartAndEnd();
        if (!Entered.IsEmpty())
        {
            FinalSlot = Entered;
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
            {
                SaveSubsystem->LoadGameFromSlot(FinalSlot);
            }
        }
    }

    RemoveFromParent();
}
