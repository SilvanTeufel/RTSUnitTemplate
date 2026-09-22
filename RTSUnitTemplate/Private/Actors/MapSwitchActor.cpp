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
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarMapSwitchDiag(
	TEXT("rts.mapswitch.diag"),
	0,
	TEXT("1 = meldet alle 5 s, ob Einheiten am Knoten stehen, ueberlappen und zur Mannschaft des Spielers gehoeren."),
	ECVF_Default);


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

                // Siehe UnlockOnMap: derselbe Besuch kann zusaetzlich eine Tuer auf einer
                // anderen Karte oeffnen - typisch die Sternenkarte, von der aus man spaeter
                // wieder hierher zurueck will.
                if (!UnlockOnMap.IsNull() && UnlockSwitchTag != NAME_None)
                {
                    const FString UnlockMapName = UnlockOnMap.ToSoftObjectPath().GetLongPackageName();
                    Subsystem->MarkSwitchEnabledForMap(UnlockMapName, UnlockSwitchTag);
                    UE_LOG(LogTemp, Warning,
                        TEXT("[MapSwitch] '%s' schaltet zusaetzlich '%s' auf der Karte '%s' frei."),
                        *GetName(), *UnlockSwitchTag.ToString(), *UnlockMapName);
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

    // Naehe selbst pruefen, statt auf Ueberlappungsereignisse zu warten.
    //
    // Der Held wird ueber TActorIterator gesucht statt ueber Ueberlappungen abgewartet.
    //
    // Die Kapsel der allermeisten Einheiten ist abgeschaltet - seit dem 21.09.2026 direkt im
    // Blueprint, nicht mehr ueber Laufzeitcode. Nur die direkt gesteuerten Einheiten tragen
    // QueryOnly. Eine abgeschaltete Kapsel erzeugt keine Ueberlappung - der Knoten sieht solche
    // Einheiten also grundsaetzlich nicht.
    //
    // Gemessen auf Level_3a: vier eigene Einheiten im Knoten, die vorderste mit 2 uu
    // Hoehenunterschied mittendrin, Kapsel NoCollision, "ueberlappt laut Aktor 0" - und der
    // Knoten meldete ueber viele Takte unveraendert genau EINEN Ueberlappenden, naemlich den, der
    // beim Versetzen des Knotens zufaellig ueberstrichen worden war.
    //
    // Die Helden-Ausnahme greift dort zusaetzlich nicht, weil die PLATZIERTE Einheit den Tag
    // Character.CameraUnit gar nicht traegt (der Blueprint schon, die Instanz nicht). Das liesse
    // sich je Karte nachtragen - dann haengt das Portal aber weiter daran, dass jede Karte diesen
    // Tag richtig setzt, und normale Einheiten koennten es nie ausloesen. Der Abstandstest hier
    // ist von Tags, Kollision und Bewegungsart unabhaengig.
    //
    // Aufwand: ein Durchlauf ueber die Einheiten je Knoten und Viertelsekunde. Der Knoten ist ein
    // einzelner Aktor je Ziel, nicht ein Bestandteil jeder Einheit.
    if (OverlapCapsule)
    {
        OverlapRecheckTime += DeltaTime;
        if (OverlapRecheckTime >= OverlapRecheckInterval)
        {
            OverlapRecheckTime = 0.f;
            CheckUnitsInRange();
        }
    }

    // DIAGNOSE (bleibt stehen bis abbestellt), schaltbar ueber rts.mapswitch.diag 1.
    //
    // Der Knoten loest auf manchen Karten nicht aus. Es gibt genau drei moegliche Stellen, und
    // diese Zeile trennt sie: kommt ueberhaupt eine Einheit nah genug heran, meldet die Kapsel
    // sie als Ueberlappung, und stimmt die Mannschaft mit der des Spielers ueberein.
    if (CVarMapSwitchDiag.GetValueOnGameThread() != 0)
    {
        MapSwitchDiagTime += DeltaTime;
        if (MapSwitchDiagTime >= 5.f)
        {
            MapSwitchDiagTime = 0.f;

            const float Reichweite = OverlapCapsule ? OverlapCapsule->GetScaledCapsuleRadius() + 400.f : 400.f;
            const FVector Hier = GetActorLocation();

            int32 Nah = 0;
            int32 NahMitTeam = 0;
            FString Beispiel;
            int32 SpielerTeam = -1;
            if (const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (const ACustomControllerBase* CPC = Cast<ACustomControllerBase>(PC))
                {
                    SpielerTeam = CPC->SelectableTeamId;
                }
            }

            for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
            {
                const FVector D = It->GetActorLocation() - Hier;
                if (FVector2D(D.X, D.Y).Size() > Reichweite)
                {
                    continue;
                }
                ++Nah;
                if (It->TeamId == SpielerTeam) ++NahMitTeam;
                if (Beispiel.IsEmpty())
                {
                    // Warum eine Einheit nicht als Ueberlappung gefuehrt wird, hat genau zwei
                    // moegliche Gruende: ihre Kapsel ist abgeschaltet, oder die Buchhaltung der
                    // Engine kennt sie nicht. Beides steht hier nebeneinander.
                    const UCapsuleComponent* Kapsel = It->GetCapsuleComponent();
                    Beispiel = FString::Printf(TEXT("%s (Team %d, dZ %.0f, Kapsel %s, ueberlappt laut Aktor %d, Tags %s)"),
                        *It->GetName(), It->TeamId, D.Z,
                        Kapsel ? *UEnum::GetValueAsString(Kapsel->GetCollisionEnabled()) : TEXT("keine"),
                        IsOverlappingActor(*It) ? 1 : 0,
                        *It->UnitTags.ToStringSimple());
                }
            }

            TArray<AActor*> Ueberlappend;
            if (OverlapCapsule)
            {
                OverlapCapsule->GetOverlappingActors(Ueberlappend, AUnitBase::StaticClass());
            }

            UE_LOG(LogTemp, Warning,
                TEXT("[MapSwitch-Diag] '%s': Spielerteam %d | %d Einheiten in %.0f uu (davon %d eigene) | %d als Ueberlappung gemeldet | aktiv %d | erste: %s"),
                *GetName(), SpielerTeam, Nah, Reichweite, NahMitTeam, Ueberlappend.Num(), bIsEnabled ? 1 : 0,
                Beispiel.IsEmpty() ? TEXT("-") : *Beispiel);

            // rts.mapswitch.diag 2 setzt EINMAL eine eigene Einheit auf den Knoten.
            //
            // Ohne Maus laeuft niemand hin, und ohne jemanden davor laesst sich nicht
            // unterscheiden, ob der Knoten defekt ist oder nur nie betreten wird. Der Sprung
            // beantwortet genau diese Frage; er passiert nur auf ausdrueckliche Ansage.
            if (CVarMapSwitchDiag.GetValueOnGameThread() >= 2 && !bDiagTeleportDone && SpielerTeam >= 0)
            {
                for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
                {
                    if (It->TeamId != SpielerTeam)
                    {
                        continue;
                    }
                    bDiagTeleportDone = true;

                    // Bewusst der KNOTEN wandert, nicht die Einheit: Einheiten stehen unter
                    // Mass-Kontrolle, ein SetActorLocation auf sie wird im naechsten Bild wieder
                    // ueberschrieben (nachgemessen: fuenf Sekunden spaeter stand sie wieder am
                    // alten Platz). Der Knoten gehoert niemandem und bleibt, wo man ihn hinsetzt.
                    const FVector Ziel = It->GetActorLocation();
                    UE_LOG(LogTemp, Warning, TEXT("[MapSwitch-Diag] setze den Knoten von (%.0f,%.0f,%.0f) auf '%s' (Team %d) bei (%.0f,%.0f,%.0f)"),
                        Hier.X, Hier.Y, Hier.Z, *It->GetName(), It->TeamId, Ziel.X, Ziel.Y, Ziel.Z);
                    SetActorLocation(Ziel, false, nullptr, ETeleportType::TeleportPhysics);
                    if (OverlapCapsule)
                    {
                        OverlapCapsule->UpdateOverlaps();
                    }
                    break;
                }
                if (!bDiagTeleportDone)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[MapSwitch-Diag] KEINE Einheit mit Team %d auf der Karte - der Knoten kann nie ausloesen."), SpielerTeam);
                    bDiagTeleportDone = true;
                }
            }
        }
    }

    if (!FMath::IsNearlyZero(RotationSpeed))
    {
        CurrentAngle += RotationSpeed * DeltaTime * (PI / 180.f);

        FVector NewLocation = CenterPoint;
        NewLocation.X += RotationRadius * FMath::Cos(CurrentAngle);
        NewLocation.Y += RotationRadius * FMath::Sin(CurrentAngle);

        SetActorLocation(NewLocation);
    }
}
void AMapSwitchActor::CheckUnitsInRange()
{
    UWorld* World = GetWorld();
    if (!World || !OverlapCapsule)
    {
        return;
    }

    const FVector Center = OverlapCapsule->GetComponentLocation();
    const float Radius = OverlapCapsule->GetScaledCapsuleRadius();
    const float HalfHeight = OverlapCapsule->GetScaledCapsuleHalfHeight();

    TSet<TWeakObjectPtr<AUnitBase>> Jetzt;

    for (TActorIterator<AUnitBase> It(World); It; ++It)
    {
        AUnitBase* Unit = *It;
        if (!IsValid(Unit))
        {
            continue;
        }

        // Ausgewaehlt wird ueber die KOLLISION, nicht ueber einen Tag.
        //
        // Nutzervorgabe vom 21.09.2026: "Es soll bei jeder Einheit triggern, aber nur bei
        // einigen ist die Kollision ueberhaupt eingeschaltet." Damit entscheidet dieselbe
        // Stelle ueber das Portal, die auch sonst ueber Kollision entscheidet - wer eine
        // abgeschaltete Kapsel hat, loest nichts aus, und wer eine hat, loest aus. Aendern sich
        // die Kollisionsregeln, wandert das Portalverhalten von selbst mit.
        //
        // Warum ueberhaupt selbst pruefen statt auf Ueberlappungen zu warten: die Pose der
        // Einheiten wird von Mass gesetzt, ohne dass die Ueberlappungsbuchhaltung der Engine
        // mitlaeuft. Gemessen auf Level_3a stand der Held mit 2 uu Hoehenunterschied mitten im
        // Knoten und wurde ueber viele Takte nie als Ueberlappung gefuehrt.
        const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
        if (!Capsule || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        // Dieselbe Form wie die Kapsel: waagerecht der Radius, senkrecht die halbe Hoehe.
        const FVector D = Unit->GetActorLocation() - Center;
        if (FVector2D(D.X, D.Y).SizeSquared() > FMath::Square(Radius) || FMath::Abs(D.Z) > HalfHeight)
        {
            continue;
        }

        Jetzt.Add(Unit);
        if (!UnitsInRange.Contains(Unit))
        {
            OnOverlapBegin(OverlapCapsule, Unit, nullptr, 0, false, FHitResult());
        }
    }

    for (const TWeakObjectPtr<AUnitBase>& Vorher : UnitsInRange)
    {
        if (Vorher.IsValid() && !Jetzt.Contains(Vorher))
        {
            OnOverlapEnd(OverlapCapsule, Vorher.Get(), nullptr, 0);
        }
    }

    UnitsInRange = MoveTemp(Jetzt);
}

void AMapSwitchActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);

    // DIAGNOSE (bleibt stehen bis abbestellt): meldet JEDE Ueberlappung samt Grund, an dem sie
    // gegebenenfalls scheitert. Die drei Ausstiege darunter sehen von aussen alle gleich aus.
    if (CVarMapSwitchDiag.GetValueOnGameThread() != 0)
    {
        // Die KOMPONENTE mitmelden, nicht nur den Aktor: bei abgeschalteter Kapsel muss die
        // Ueberlappung von einer anderen Komponente kommen, und ohne ihren Namen sucht man sie
        // durch die ganze Komponentenliste.
        UE_LOG(LogTemp, Warning, TEXT("[MapSwitch-Diag] Ueberlappung von '%s' ueber Komponente '%s' (%s) - als Einheit erkannt: %d"),
            OtherActor ? *OtherActor->GetName() : TEXT("nullptr"),
            OtherComp ? *OtherComp->GetName() : TEXT("keine"),
            OtherComp ? *UEnum::GetValueAsString(OtherComp->GetCollisionEnabled()) : TEXT("-"),
            Unit ? 1 : 0);
    }

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

    if (CVarMapSwitchDiag.GetValueOnGameThread() != 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MapSwitch-Diag] '%s' hat Team %d, der Spieler %d - Treffer: %d"),
            *Unit->GetName(), Unit->TeamId, CustomPC->SelectableTeamId,
            Unit->TeamId == CustomPC->SelectableTeamId ? 1 : 0);
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