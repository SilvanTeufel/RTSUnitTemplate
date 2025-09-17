#include "Actors/MapSwitchActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/MapMarkerWidget.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Widgets/MapSwitchWidget.h" // Include the widget header
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MapSwitchActor"

AMapSwitchActor::AMapSwitchActor()
{
    PrimaryActorTick.bCanEverTick = false;

    OverlapCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("OverlapCapsule"));
    RootComponent = OverlapCapsule;
    OverlapCapsule->SetCapsuleHalfHeight(90.0f);
    OverlapCapsule->SetCapsuleRadius(45.0f);

    MarkerWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MarkerWidgetComponent"));
    MarkerWidgetComponent->SetupAttachment(RootComponent);
    MarkerWidgetComponent->SetWidgetSpace(EWidgetSpace::World); // Widget exists in the 3D world
    MarkerWidgetComponent->SetDrawAtDesiredSize(true); // Prevents distortion
    MarkerWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 150.f)); // Position it above the capsule

    bReplicates = true;


    MarkerDisplayText = LOCTEXT("DefaultMarkerName", "Default Marker Name");
}

void AMapSwitchActor::BeginPlay()
{
    Super::BeginPlay();

    // Bind overlap events on both client and server to handle UI locally
    OverlapCapsule->OnComponentBeginOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapBegin);
    OverlapCapsule->OnComponentEndOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapEnd);

    // Set the text on the world-space marker widget
    if(UMapMarkerWidget* MarkerWidget = Cast<UMapMarkerWidget>(MarkerWidgetComponent->GetUserWidgetObject()))
    {
        MarkerWidget->SetMarkerText(MarkerDisplayText);
    }
}

void AMapSwitchActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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

    // We only want to show the widget if the overlapping unit belongs to the local player's team.
    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (MapSwitchWidgetClass && !ActiveWidget)
        {
            ActiveWidget = CreateWidget<UMapSwitchWidget>(LocalPC, MapSwitchWidgetClass);
            if (ActiveWidget)
            {
                FString MapToTravel = TargetMap.IsNull() ? "" : TargetMap.ToSoftObjectPath().GetLongPackageName();
                ActiveWidget->InitializeWidget(MapToTravel, this, bIsEnabled);
                ActiveWidget->AddToViewport();
            }
        }
    }
}

void AMapSwitchActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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

    // Only remove the widget if the unit leaving the overlap belongs to the local player's team
    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (ActiveWidget)
        {
            ActiveWidget->RemoveFromParent();
            ActiveWidget = nullptr;
        }
    }
}