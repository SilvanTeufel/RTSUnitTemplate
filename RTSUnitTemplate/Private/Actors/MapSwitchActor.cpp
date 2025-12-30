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
#include "Engine/GameInstance.h"
#include "System/MapSwitchSubsystem.h"

#define LOCTEXT_NAMESPACE "MapSwitchActor"

#include "Net/UnrealNetwork.h"

void AMapSwitchActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMapSwitchActor, bIsEnabled);
}

AMapSwitchActor::AMapSwitchActor()
{
    PrimaryActorTick.bCanEverTick = true;

    OverlapCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("OverlapCapsule"));
    RootComponent = OverlapCapsule;
    OverlapCapsule->SetCapsuleHalfHeight(90.0f);
    OverlapCapsule->SetCapsuleRadius(45.0f);

    MarkerWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MarkerWidgetComponent"));
    MarkerWidgetComponent->SetupAttachment(RootComponent);
    MarkerWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    MarkerWidgetComponent->SetDrawAtDesiredSize(true);
    MarkerWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 150.f));

    bReplicates = true;

    MarkerDisplayText = LOCTEXT("DefaultMarkerName", "Default Marker Name");
}

FName AMapSwitchActor::GetDestinationSwitchTagToEnable() const
{
    return DestinationSwitchTagToEnable;
}

void AMapSwitchActor::BeginPlay()
{
    Super::BeginPlay();
    
    OverlapCapsule->OnComponentBeginOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapBegin);
    OverlapCapsule->OnComponentEndOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapEnd);
    
    if(UMapMarkerWidget* MarkerWidget = Cast<UMapMarkerWidget>(MarkerWidgetComponent->GetUserWidgetObject()))
    {
        MarkerWidget->SetMarkerText(MarkerDisplayText);
    }
    
    if (SwitchTag != NAME_None)
    {
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                if (UMapSwitchSubsystem* Subsystem = GI->GetSubsystem<UMapSwitchSubsystem>())
                {
                    const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString*/ true);
                    const bool bWasEnabled = Subsystem->IsSwitchEnabledForMap(CurrentLevelName, SwitchTag);
                    if (bWasEnabled)
                    {
                        bIsEnabled = true;
                    }
                }
            }
        }
    }


    if (!FMath::IsNearlyZero(RotationSpeed))
    {
        CurrentAngle = FMath::RandRange(0.f, 2.f * PI);
        
        FVector NewLocation = CenterPoint;
        NewLocation.X += RotationRadius * FMath::Cos(CurrentAngle);
        NewLocation.Y += RotationRadius * FMath::Sin(CurrentAngle);

        SetActorLocation(NewLocation);
    }
}


void AMapSwitchActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (FMath::IsNearlyZero(RotationSpeed))
    {
        return;
    }

    CurrentAngle += RotationSpeed * DeltaTime * (PI / 180.f);

    FVector NewLocation = CenterPoint;
    NewLocation.X += RotationRadius * FMath::Cos(CurrentAngle);
    NewLocation.Y += RotationRadius * FMath::Sin(CurrentAngle);

    SetActorLocation(NewLocation);
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

    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (ActiveWidget)
        {
            ActiveWidget->RemoveFromParent();
            ActiveWidget = nullptr;
        }
    }
}