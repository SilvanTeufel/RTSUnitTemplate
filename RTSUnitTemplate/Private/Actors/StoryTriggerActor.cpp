#include "Actors/StoryTriggerActor.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"
#include "Widgets/StoryWidgetBase.h"
#include "TimerManager.h"

AStoryTriggerActor::AStoryTriggerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TriggerCapsule"));
    RootComponent = TriggerCapsule;

    TriggerCapsule->SetCapsuleHalfHeight(90.f);
    TriggerCapsule->SetCapsuleRadius(45.f);
    TriggerCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerCapsule->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    TriggerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
    TriggerCapsule->SetGenerateOverlapEvents(true);
    TriggerCapsule->SetHiddenInGame(true);
}

void AStoryTriggerActor::BeginPlay()
{
    Super::BeginPlay();

    if (TriggerCapsule)
    {
        TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &AStoryTriggerActor::OnOverlapBegin);
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
    if (Unit->TeamId != TeamId)
    {
        return;
    }

    bHasTriggered = true;

    // Disable further overlaps to save resources
    if (TriggerCapsule && bTriggerOnce)
    {
        TriggerCapsule->SetGenerateOverlapEvents(false);
        TriggerCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Play sound at trigger location (server or client fine for one-shot non-replicated FX)
    if (TriggerSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, TriggerSound, GetActorLocation());
    }

    // Create widget locally for the player's screen
    if (StoryWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            UStoryWidgetBase* Widget = CreateWidget<UStoryWidgetBase>(PC, StoryWidgetClass);
            if (Widget)
            {
                // Center + offset positioning
                int32 SizeX = 0, SizeY = 0;
                PC->GetViewportSize(SizeX, SizeY);
                const FVector2D CenterPos(0.5f * SizeX + ScreenOffsetX, 0.5f * SizeY + ScreenOffsetY);

                Widget->AddToViewport();
                Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
                Widget->SetPositionInViewport(CenterPos, false);

                Widget->StartStory(StoryText, StoryImage.Get());

                // Store and start removal timer if requested
                ActiveWidget = Widget;
                if (WidgetLifetimeSeconds > 0.f)
                {
                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimer(RemoveWidgetTimer, this, &AStoryTriggerActor::RemoveActiveWidget, WidgetLifetimeSeconds, false);
                    }
                }
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
