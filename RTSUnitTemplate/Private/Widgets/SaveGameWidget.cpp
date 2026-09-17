// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

    if (NewGameButton)
    {
        NewGameButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnNewGameClicked);
    }
    if (ResetButton)
    {
        ResetButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnResetClicked);
    }
    if (ResetConfirmYesButton)
    {
        ResetConfirmYesButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnResetConfirmYesClicked);
    }
    if (ResetConfirmNoButton)
    {
        ResetConfirmNoButton->OnClicked.AddDynamic(this, &USaveGameWidget::OnResetConfirmNoClicked);
    }
    // Das Bestaetigungsfeld startet geschlossen - sonst stuende die Rueckfrage schon da,
    // bevor jemand danach gefragt hat.
    ShowResetConfirm(false);

    // Liste jetzt befüllen, da BindWidget-Referenzen gültig sind
    PopulateSavesList();
}

void USaveGameWidget::ShowResetConfirm(bool bShow)
{
    if (ResetConfirmPanel)
    {
        ResetConfirmPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (!bShow)
    {
        ResetArmedAt = -1.0;
    }
}

void USaveGameWidget::OnNewGameClicked()
{
    // Keine Rueckfrage: es wird nichts geloescht. Der neue Spielstand kommt neben die
    // bisherigen, er ist nur der juengste. Eine Sicherheitsabfrage vor einer Handlung, die
    // nichts zerstoert, erzieht nur dazu, Abfragen wegzuklicken.
    FString Angelegt;
    if (const UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameSaveSubsystem* Sub = GI->GetSubsystem<UGameSaveSubsystem>())
            {
                Angelegt = Sub->StartNewGame(FString());
            }
        }
    }

    PopulateSavesList();
    if (!Angelegt.IsEmpty())
    {
        SlotName = Angelegt;
        if (SlotNameTextBox)
        {
            SlotNameTextBox->SetText(FText::FromString(Angelegt));
        }
    }

    if (DialogText)
    {
        DialogText->SetText(FText::FromString(Angelegt.IsEmpty()
            ? TEXT("New game could not be created.")
            : FString::Printf(TEXT("New game started ('%s'). Your existing saves are untouched."), *Angelegt)));
    }
}

void USaveGameWidget::OnResetClicked()
{
    if (ResetConfirmPanel)
    {
        // Der gebaute Weg: Rueckfrage zeigen, sonst nichts. Geloescht wird erst im Ja-Knopf.
        if (ResetConfirmText)
        {
            ResetConfirmText->SetText(FText::FromString(
                TEXT("Delete ALL saved games? This cannot be undone.")));
        }
        ShowResetConfirm(true);
        return;
    }

    // NOTNAGEL, wenn im Widget-Blueprint kein Bestaetigungsfeld gebaut wurde.
    //
    // Dann ersetzt ein zweiter Druck die Rueckfrage. Der Knopf darf auf keinen Fall beim ERSTEN
    // Klick loeschen - ein vergessenes Popup wuerde sonst aus einer Sicherheitsabfrage eine
    // Falle machen, und der Verlust faellt erst auf, wenn die Spielstaende schon weg sind.
    const double Jetzt = FPlatformTime::Seconds();
    if (ResetArmedAt > 0.0 && (Jetzt - ResetArmedAt) <= ResetArmSeconds)
    {
        OnResetConfirmYesClicked();
        return;
    }

    ResetArmedAt = Jetzt;
    if (DialogText)
    {
        DialogText->SetText(FText::FromString(
            TEXT("Delete ALL saved games? Press again to confirm.")));
    }
}

void USaveGameWidget::OnResetConfirmNoClicked()
{
    ShowResetConfirm(false);
    if (DialogText)
    {
        DialogText->SetText(FText::FromString(TEXT("Cancelled. Nothing was deleted.")));
    }
}

void USaveGameWidget::OnResetConfirmYesClicked()
{
    ShowResetConfirm(false);
    ResetArmedAt = -1.0;

    int32 Geloescht = 0;
    if (const UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameSaveSubsystem* Sub = GI->GetSubsystem<UGameSaveSubsystem>())
            {
                Geloescht = Sub->ResetAllProgress();
            }
        }
    }

    // Die Liste zeigt sonst weiter Slots, die es nicht mehr gibt.
    PopulateSavesList();
    if (SlotNameTextBox)
    {
        SlotNameTextBox->SetText(FText::GetEmpty());
    }
    SlotName.Reset();

    if (DialogText)
    {
        DialogText->SetText(FText::FromString(FString::Printf(
            TEXT("%d saved games deleted. Unlocks reset."), Geloescht)));
    }
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

                    if (!bHasSummary)
                    {
                        continue;
                    }

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

    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
}

void USaveGameWidget::OnNoClicked()
{
    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
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

    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
}
