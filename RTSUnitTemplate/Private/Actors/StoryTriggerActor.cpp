// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/StoryTriggerActor.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"
#include "Widgets/StoryWidgetBase.h"
#include "TimerManager.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"

AStoryTriggerActor::AStoryTriggerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(45.f, 45.f, 90.f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
    TriggerBox->SetHiddenInGame(true);
}

void AStoryTriggerActor::BeginPlay()
{
    Super::BeginPlay();

    // Load row from DataTable: either a random row (if UseRandomRow) or by StoryRowId.
    // Bei einer Zeilenfolge (StoryRowIds) entfaellt das - die wird erst beim Ausloesen gelesen.
    if (StoryDataTable && StoryRowIds.Num() == 0)
    {
        static const FString ContextString(TEXT("StoryTriggerActor_Load"));
        const FStoryWidgetTable* Row = nullptr;
        if (UseRandomRow)
        {
            const TArray<FName> RowNames = StoryDataTable->GetRowNames();
            if (RowNames.Num() > 0)
            {
                const int32 Index = FMath::RandRange(0, RowNames.Num() - 1);
                Row = StoryDataTable->FindRow<FStoryWidgetTable>(RowNames[Index], ContextString, true);
            }
        }
        else if (!StoryRowId.IsNone())
        {
            Row = StoryDataTable->FindRow<FStoryWidgetTable>(StoryRowId, ContextString, true);
        }

        if (Row)
        {
            StoryWidgetClass = Row->StoryWidgetClass;
            TriggerSound = Row->TriggerSound;
            StoryText = Row->StoryText;
            StoryImage = Row->StoryImage;
            StoryMaterial = Row->StoryMaterial;
            StoryImageSoft = Row->StoryImageSoft;
            StoryMaterialSoft = Row->StoryMaterialSoft;
            // ScreenOffsetX = Row->ScreenOffsetX;
            // ScreenOffsetY = Row->ScreenOffsetY;
            WidgetLifetimeSeconds = Row->WidgetLifetimeSeconds;
            bTillAudioEnds = Row->bTillAudioEnds;
            AudioEndExtraDelay = Row->AudioEndExtraDelay;
        }
    }

    if (TriggerBox)
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStoryTriggerActor::OnOverlapBegin);
    }
}

void AStoryTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bTriggerOnce && bHasTriggered)
    {
        return;
    }

    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
    if (!Unit)
    {
        return;
    }

    // Team gate: only trigger for units with matching team id
    if (Unit->TeamId != TriggerTeamId)
    {
        return;
    }

    // Local player team gate: only show for players whose controller team matches
    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    AControllerBase* LocalRTSController = LocalPC ? Cast<AControllerBase>(LocalPC) : nullptr;
    if (!LocalRTSController || LocalRTSController->SelectableTeamId != TeamId)
    {
        // Do not show UI/sound for other teams' players
        return;
    }

    bHasTriggered = true;

    // Disable further overlaps to save resources
    if (TriggerBox && bTriggerOnce)
    {
        TriggerBox->SetGenerateOverlapEvents(false);
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    StoryEinreihen();
}


void AStoryTriggerActor::TriggerStory()
{
    if (bTriggerOnce && bHasTriggered)
    {
        return;
    }

    bHasTriggered = true;

    // Von Hand ausgeloest heisst: kein Team-Tor. Wer den Aufruf setzt, weiss, was er will.
    // Die Auslaeseflaeche wird trotzdem stillgelegt, damit die Szene nicht spaeter noch einmal
    // durch Betreten kommt.
    if (TriggerBox && bTriggerOnce)
    {
        TriggerBox->SetGenerateOverlapEvents(false);
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    StoryEinreihen();
}


void AStoryTriggerActor::StoryEinreihen()
{
    // Mehrere Zeilen? Dann jede einzeln einreihen. Die Warteschlange spielt sie nacheinander
    // ab und haelt immer nur ein Widget offen - es spricht also nie mehr als eine Figur.
    if (StoryRowIds.Num() > 0 && StoryDataTable)
    {
        static const FString Kontext(TEXT("StoryTriggerActor_Folge"));
        int32 Eingereiht = 0;
        for (const FName& ZeilenId : StoryRowIds)
        {
            if (ZeilenId.IsNone())
            {
                continue;
            }
            if (const FStoryWidgetTable* Zeile = StoryDataTable->FindRow<FStoryWidgetTable>(ZeilenId, Kontext, true))
            {
                ZeileEinreihen(*Zeile);
                ++Eingereiht;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[Story] '%s': Zeile '%s' fehlt in '%s' - uebersprungen."),
                       *GetName(), *ZeilenId.ToString(), *StoryDataTable->GetName());
            }
        }

        if (Eingereiht > 0)
        {
            OnStoryTriggered.Broadcast();
            return;
        }

        // Keine einzige Zeile gefunden: lieber die gemerkte Einzelzeile zeigen als gar nichts.
        UE_LOG(LogTemp, Warning, TEXT("[Story] '%s': keine der %d Zeilen gefunden - Rueckfall auf die Einzelzeile."),
               *GetName(), StoryRowIds.Num());
    }

    // Altes Verhalten: die eine in BeginPlay gemerkte Zeile.
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
            {
                FStoryQueueItem Item;
                Item.WidgetClass = StoryWidgetClass;
                Item.Text = StoryText;
                Item.Image = StoryImage.Get();
                Item.Material = StoryMaterial.Get();
                Item.ImageSoft = StoryImageSoft;
                Item.MaterialSoft = StoryMaterialSoft;
                Item.LifetimeSeconds = WidgetLifetimeSeconds;
                Item.Sound = TriggerSound;
                Item.bTillAudioEnds = bTillAudioEnds;
                Item.AudioEndExtraDelay = AudioEndExtraDelay;
                Item.TriggeringSource = this;
                Queue->EnqueueStory(Item);
                OnStoryTriggered.Broadcast();
            }
        }
    }
}


void AStoryTriggerActor::EnqueueStoryRows(UObject* WorldContextObject, UDataTable* StoryTable,
                                          const TArray<FName>& RowIds,
                                          TSubclassOf<UStoryWidgetBase> FallbackWidgetClass)
{
	if (!StoryTable || RowIds.Num() == 0)
	{
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UStoryTriggerQueueSubsystem* Queue = GI ? GI->GetSubsystem<UStoryTriggerQueueSubsystem>() : nullptr;
	if (!Queue)
	{
		return;
	}

	static const FString Kontext(TEXT("StoryTriggerActor_Statisch"));
	for (const FName& ZeilenId : RowIds)
	{
		if (ZeilenId.IsNone())
		{
			continue;
		}

		const FStoryWidgetTable* Zeile = StoryTable->FindRow<FStoryWidgetTable>(ZeilenId, Kontext, true);
		if (!Zeile)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Story] Zeile '%s' fehlt in '%s' - uebersprungen."),
			       *ZeilenId.ToString(), *StoryTable->GetName());
			continue;
		}

		FStoryQueueItem Item;
		Item.WidgetClass = Zeile->StoryWidgetClass ? Zeile->StoryWidgetClass : FallbackWidgetClass;
		Item.Text = Zeile->StoryText;
		Item.Image = Zeile->StoryImage;
		Item.Material = Zeile->StoryMaterial;
		Item.ImageSoft = Zeile->StoryImageSoft;
		Item.MaterialSoft = Zeile->StoryMaterialSoft;
		Item.LifetimeSeconds = Zeile->WidgetLifetimeSeconds;
		Item.Sound = Zeile->TriggerSound;
		Item.bTillAudioEnds = Zeile->bTillAudioEnds;
		Item.AudioEndExtraDelay = Zeile->AudioEndExtraDelay;
		Queue->EnqueueStory(Item);
	}
}


void AStoryTriggerActor::ZeileEinreihen(const FStoryWidgetTable& Zeile)
{
    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UStoryTriggerQueueSubsystem* Queue = GI ? GI->GetSubsystem<UStoryTriggerQueueSubsystem>() : nullptr;
    if (!Queue)
    {
        return;
    }

    FStoryQueueItem Item;
    // Die Widgetklasse darf in der Tabelle leer bleiben - dann gilt die am Aktor eingestellte.
    Item.WidgetClass = Zeile.StoryWidgetClass ? Zeile.StoryWidgetClass : StoryWidgetClass;
    Item.Text = Zeile.StoryText;
    Item.Image = Zeile.StoryImage;
    Item.Material = Zeile.StoryMaterial;
    Item.ImageSoft = Zeile.StoryImageSoft;
    Item.MaterialSoft = Zeile.StoryMaterialSoft;
    Item.LifetimeSeconds = Zeile.WidgetLifetimeSeconds;
    Item.Sound = Zeile.TriggerSound;
    Item.bTillAudioEnds = Zeile.bTillAudioEnds;
    Item.AudioEndExtraDelay = Zeile.AudioEndExtraDelay;
    Item.TriggeringSource = this;
    Queue->EnqueueStory(Item);
}


void AStoryTriggerActor::RemoveActiveWidget()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RemoveWidgetTimer);
    }

    if (ActiveWidget)
    {
        ActiveWidget->RemoveFromParent();
        ActiveWidget = nullptr;
    }
}
