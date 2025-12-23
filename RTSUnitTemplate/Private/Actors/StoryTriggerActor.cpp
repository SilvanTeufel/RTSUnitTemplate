#include "Actors/StoryTriggerActor.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"
#include "Widgets/StoryWidgetBase.h"
#include "TimerManager.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Engine/GameInstance.h"

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

    // Load row from DataTable: either a random row (if UseRandomRow) or by StoryRowId
    if (StoryDataTable)
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
            ScreenOffsetX = Row->ScreenOffsetX;
            ScreenOffsetY = Row->ScreenOffsetY;
            WidgetLifetimeSeconds = Row->WidgetLifetimeSeconds;
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

    // Instead of playing immediately, enqueue into the StoryTriggerQueueSubsystem to ensure sequential display
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
                Item.OffsetX = ScreenOffsetX;
                Item.OffsetY = ScreenOffsetY;
                Item.LifetimeSeconds = WidgetLifetimeSeconds;
                Item.Sound = TriggerSound;
                Queue->EnqueueStory(Item);
            }
        }
    }
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
