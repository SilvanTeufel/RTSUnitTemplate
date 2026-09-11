// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/MapSwitchActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "Controller/PlayerController/CameraControllerBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MapSwitchActor"

#include "Net/UnrealNetwork.h"

void AMapSwitchActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMapSwitchActor, bIsEnabled);
    DOREPLIFETIME(AMapSwitchActor, CenterPoint);
    DOREPLIFETIME(AMapSwitchActor, RotationRadius);
    DOREPLIFETIME(AMapSwitchActor, RotationSpeed);
    DOREPLIFETIME(AMapSwitchActor, CurrentAngle);
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
    SetReplicateMovement(true);

    MarkerDisplayText = LOCTEXT("DefaultMarkerName", "Default Marker Name");
}

FName AMapSwitchActor::GetDestinationSwitchTagToEnable() const
{
    return DestinationSwitchTagToEnable;
}

void AMapSwitchActor::BuildDestinationStates(TArray<FMapSwitchDestinationState>& OutStates) const
{
    OutStates.Reset();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Unlock state hangs off the map this actor stands in - exactly where
    // AWinLoseConfigActor::DestinationSwitchTagToEnable writes it on a win.
    const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString*/ true);
    const UMapSwitchSubsystem* Subsystem = nullptr;
    if (const UGameInstance* GI = World->GetGameInstance())
    {
        Subsystem = GI->GetSubsystem<UMapSwitchSubsystem>();
    }

    if (Destinations.Num() == 0)
    {
        // Legacy: emit the single-target fields as exactly one entry, so the five planets that
        // were already authored behave no differently than before.
        FMapSwitchDestinationState Legacy;
        Legacy.MapLongPackageName = TargetMap.IsNull() ? FString() : TargetMap.ToSoftObjectPath().GetLongPackageName();
        Legacy.DisplayName = LevelDisplayName;
        Legacy.DestinationSwitchTagToEnable = DestinationSwitchTagToEnable;
        Legacy.bUnlocked = bIsEnabled;
        OutStates.Add(Legacy);
        return;
    }

    for (const FMapSwitchDestination& Entry : Destinations)
    {
        if (Entry.TargetMap.IsNull())
        {
            continue;
        }

        FMapSwitchDestinationState State;
        State.MapLongPackageName = Entry.TargetMap.ToSoftObjectPath().GetLongPackageName();
        State.DisplayName = Entry.LevelDisplayName;
        State.DestinationSwitchTagToEnable = Entry.DestinationSwitchTagToEnable;

        if (Entry.bUnlockedByDefault || Entry.RequiredSwitchTag == NAME_None)
        {
            State.bUnlocked = true;
        }
        else if (Subsystem)
        {
            State.bUnlocked = Subsystem->IsSwitchEnabledForMap(CurrentLevelName, Entry.RequiredSwitchTag);
        }

        // bIsEnabled still gates the whole actor. A planet the player cannot see must not hand
        // out a level, even when one of its rows would be open on its own.
        State.bUnlocked = State.bUnlocked && bIsEnabled;

        OutStates.Add(State);
    }
}

void AMapSwitchActor::TravelToDestination(const FMapSwitchDestinationState& State)
{
    if (!State.bUnlocked || State.MapLongPackageName.IsEmpty())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UMapSwitchSubsystem* Subsystem = GI->GetSubsystem<UMapSwitchSubsystem>())
            {
                if (State.DestinationSwitchTagToEnable != NAME_None)
                {
                    Subsystem->MarkSwitchEnabledForMap(State.MapLongPackageName, State.DestinationSwitchTagToEnable);
                }
            }
        }
    }

    StartMapSwitch();

    if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
    {
        PC->Server_TravelToMap(State.MapLongPackageName, State.DestinationSwitchTagToEnable);
    }

    CloseWidget();
}

void AMapSwitchActor::BeginPlay()
{
    Super::BeginPlay();

    // 1. Initialer Wert aus der Kapsel (Default)
    CachedMinimapRadius = OverlapCapsule ? OverlapCapsule->GetScaledCapsuleRadius() : 45.f;

    // 2. Suche nach einem StaticMesh (bevorzugt, falls im Blueprint zugewiesen)
    if (const UStaticMeshComponent* MeshComp = FindComponentByClass<UStaticMeshComponent>())
    {
        if (MeshComp->GetStaticMesh())
        {
            // Nutzt den Kugel-Radius der Mesh-Bounds als visuelle Repräsentation
            CachedMinimapRadius = MeshComp->Bounds.SphereRadius;
        }
    }

    // Sicherheits-Fallback
    if (CachedMinimapRadius <= 0.f) CachedMinimapRadius = 45.f;
    
    OverlapCapsule->OnComponentBeginOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapBegin);
    OverlapCapsule->OnComponentEndOverlap.AddDynamic(this, &AMapSwitchActor::OnOverlapEnd);
    
    if(UMapMarkerWidget* MarkerWidget = Cast<UMapMarkerWidget>(MarkerWidgetComponent->GetUserWidgetObject()))
    {
        MarkerWidget->SetMarkerText(MarkerDisplayText);
    }
    
    if (HasAuthority() && SwitchTag != NAME_None)
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


    if (HasAuthority() && !FMath::IsNearlyZero(RotationSpeed))
    {
        CurrentAngle = FMath::RandRange(0.f, 2.f * PI);
    }

    if (!FMath::IsNearlyZero(RotationSpeed))
    {
        FVector NewLocation = CenterPoint;
        NewLocation.X += RotationRadius * FMath::Cos(CurrentAngle);
        NewLocation.Y += RotationRadius * FMath::Sin(CurrentAngle);

        SetActorLocation(NewLocation);
    }
}


void AMapSwitchActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!FMath::IsNearlyZero(RotationSpeed))
    {
        CurrentAngle += RotationSpeed * DeltaTime * (PI / 180.f);

        FVector NewLocation = CenterPoint;
        NewLocation.X += RotationRadius * FMath::Cos(CurrentAngle);
        NewLocation.Y += RotationRadius * FMath::Sin(CurrentAngle);

        SetActorLocation(NewLocation);
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

    if (Unit->TeamId == CustomPC->SelectableTeamId)
    {
        if (WidgetCloseTimerHandle.IsValid())
        {
            GetWorldTimerManager().ClearTimer(WidgetCloseTimerHandle);
        }

        if (MapSwitchWidgetClass && !ActiveWidget)
        {
            ActiveWidget = CreateWidget<UMapSwitchWidget>(LocalPC, MapSwitchWidgetClass);
            if (ActiveWidget)
            {
                TArray<FMapSwitchDestinationState> States;
                BuildDestinationStates(States);

                // A list only pays off from two destinations up; with one it stays the familiar
                // yes/no dialog, so the four existing planets look exactly as they did.
                if (States.Num() > 1 && ActiveWidget->SupportsDestinationList())
                {
                    ActiveWidget->InitializeWidgetWithDestinations(States, this);
                }
                else
                {
                    FString MapToTravel = TargetMap.IsNull() ? "" : TargetMap.ToSoftObjectPath().GetLongPackageName();
                    FText Caption = LevelDisplayName;
                    bool bOpen = bIsEnabled;
                    if (States.Num() == 1)
                    {
                        MapToTravel = States[0].MapLongPackageName;
                        Caption = States[0].DisplayName;
                        bOpen = States[0].bUnlocked;
                    }
                    ActiveWidget->InitializeWidget(MapToTravel, this, bOpen, Caption);
                }
                ActiveWidget->AddToViewport();

                if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(LocalPC))
                {
                    CameraPC->bIsCameraMovementHaltedByUI = true;
                }
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
            GetWorldTimerManager().SetTimer(WidgetCloseTimerHandle, this, &AMapSwitchActor::CloseWidget, 5.0f, false);
        }
    }
}

void AMapSwitchActor::CloseWidget()
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