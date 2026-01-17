#include "Actors/SaveGameActor.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Widgets/SaveGameWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "TimerManager.h"

ASaveGameActor::ASaveGameActor()
{
    PrimaryActorTick.bCanEverTick = false;

    OverlapCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("OverlapCapsule"));
    RootComponent = OverlapCapsule;
    OverlapCapsule->SetCapsuleHalfHeight(90.0f);
    OverlapCapsule->SetCapsuleRadius(45.0f);

    bReplicates = true;
}

void ASaveGameActor::BeginPlay()
{
    Super::BeginPlay();

    OverlapCapsule->OnComponentBeginOverlap.AddDynamic(this, &ASaveGameActor::OnOverlapBegin);
    OverlapCapsule->OnComponentEndOverlap.AddDynamic(this, &ASaveGameActor::OnOverlapEnd);
}

void ASaveGameActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
    if (!Unit)
    {
        return;
    }

    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!LocalPC)
    {
        return;
    }

    ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(LocalPC);
    if (!CustomPC)
    {
        return;
    }

    // Nur anzeigen, wenn die Unit dem lokalen Team gehört
    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (WidgetCloseTimerHandle.IsValid())
        {
            GetWorldTimerManager().ClearTimer(WidgetCloseTimerHandle);
        }

        if (SaveGameWidgetClass && !ActiveWidget)
        {
            ActiveWidget = CreateWidget<USaveGameWidget>(LocalPC, SaveGameWidgetClass);
            if (ActiveWidget)
            {
                ActiveWidget->InitializeWidget(SaveSlotName, this);
                ActiveWidget->AddToViewport();

                if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(LocalPC))
                {
                    CameraPC->bIsCameraMovementHaltedByUI = true;
                }
            }
        }
    }
}

void ASaveGameActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
    if (!Unit) return;

    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!LocalPC) return;

    ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(LocalPC);
    if (!CustomPC)
    {
        return;
    }

    // Nur schließen, wenn die verlassende Unit dem lokalen Team gehört
    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (ActiveWidget)
        {
            GetWorldTimerManager().SetTimer(WidgetCloseTimerHandle, this, &ASaveGameActor::CloseWidget, 5.0f, false);
        }
    }
}

void ASaveGameActor::CloseWidget()
{
    if (WidgetCloseTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(WidgetCloseTimerHandle);
    }

    if (ActiveWidget)
    {
        ActiveWidget->RemoveFromParent();
        ActiveWidget = nullptr;
    }

    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(LocalPC))
    {
        CameraPC->bIsCameraMovementHaltedByUI = false;
    }
}
