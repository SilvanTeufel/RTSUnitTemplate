// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSwitchActor.generated.h"

class UCapsuleComponent;
class UUserWidget;
class AUnitBase;
class UMapSwitchWidget;
class UWidgetComponent;

/**
 * One level a switch actor can send the player to.
 *
 * A planet used to carry exactly one destination, which meant a second mission on the same planet
 * needed a second actor sitting inside the first one. Authoring several entries here keeps one
 * marker per planet and lets the missions unlock over the course of the campaign.
 */
USTRUCT(BlueprintType)
struct FMapSwitchDestination
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    TSoftObjectPtr<UWorld> TargetMap;

    /** Shown on the button. Empty falls back to the map's file name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FText LevelDisplayName;

    /** Tag switched on for the TARGET map once this entry has been travelled to. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName DestinationSwitchTagToEnable;

    /**
     * Gate. The entry stays greyed out until this tag is enabled for the map the actor stands in.
     * The tag is set by the WinLoseConfigActor of whichever level unlocks this one - see
     * AWinLoseConfigActor::DestinationSwitchTagToEnable. None means "playable from the start".
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName RequiredSwitchTag;

    /** Ignores RequiredSwitchTag and is always offered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    bool bUnlockedByDefault = false;
};

/** A destination plus the answer to "may the player go there right now?". Handed to the widget. */
USTRUCT(BlueprintType)
struct FMapSwitchDestinationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
    FString MapLongPackageName;

    UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
    FName DestinationSwitchTagToEnable;

    UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
    bool bUnlocked = false;
};

UCLASS()
class RTSUNITTEMPLATE_API AMapSwitchActor : public AActor
{
    GENERATED_BODY()

public:
    AMapSwitchActor();

    virtual void Tick(float DeltaTime) override;
    
    UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
    FName GetDestinationSwitchTagToEnable() const;

    UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
    void StartMapSwitch();

    UFUNCTION()
    void CloseWidget();

    float GetMinimapRadius() const { return CachedMinimapRadius; }

    /**
     * Every destination this actor offers, each with its unlock state resolved against the
     * MapSwitchSubsystem. When Destinations is empty the legacy single-target fields are wrapped
     * into one entry, so actors authored before this existed behave exactly as before.
     */
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void BuildDestinationStates(TArray<FMapSwitchDestinationState>& OutStates) const;

    /** Travels to one of the entries from BuildDestinationStates. Ignores locked entries. */
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void TravelToDestination(const FMapSwitchDestinationState& State);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
    UCapsuleComponent* OverlapCapsule;

    /**
     * Abstand zwischen zwei Ueberlappungspruefungen in Sekunden.
     *
     * Der Knoten ist kein Reaktionstest - ein Viertel einer Sekunde ist fuer das Betreten eines
     * Portals nicht wahrnehmbar und kostet ein Vielfaches weniger als eine Pruefung je Bild.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float OverlapRecheckInterval = 0.25f;

    float OverlapRecheckTime = 0.f;

    /** Wer beim letzten Durchlauf im Knoten stand - daraus entstehen Ein- und Austritt. */
    TSet<TWeakObjectPtr<class AUnitBase>> UnitsInRange;

    /** Sucht die Einheiten im Knoten und meldet Ein- und Austritte. */
    void CheckUnitsInRange();

    /** Zeitgeber der Diagnose aus rts.mapswitch.diag. */
    float MapSwitchDiagTime = 0.f;

    /** Der Diagnose-Sprung aus rts.mapswitch.diag 2 passiert genau einmal. */
    bool bDiagTeleportDone = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
    UWidgetComponent* MarkerWidgetComponent;

    UPROPERTY()
    float CachedMinimapRadius = 45.f;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UMapSwitchWidget> MapSwitchWidgetClass;

    UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
    TSoftObjectPtr<UWorld> TargetMap;

    /**
     * Several levels behind one marker. Leave empty to keep using TargetMap / LevelDisplayName /
     * DestinationSwitchTagToEnable above; filling it in takes over completely.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    TArray<FMapSwitchDestination> Destinations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName SwitchTag;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName DestinationSwitchTagToEnable;

    /**
     * Zusaetzliche Freischaltung auf einer ANDEREN Karte.
     *
     * DestinationSwitchTagToEnable schreibt den Tag immer fuer die ZIELkarte
     * (MarkSwitchEnabledForMap mit State.MapLongPackageName). Gelesen wird ein Tag aber gegen die
     * Karte, auf der der fragende Aktor steht (IsSwitchEnabledForMap mit CurrentLevelName).
     * Beides trifft sich nur, wenn Ziel und Leser dieselbe Karte sind.
     *
     * Gemessen am 21.09.2026: der Hoehleneingang auf Level_3a schaltet fuer Level_3b frei, der
     * Planet Mandible liest aber gegen SolarSystem - das Bossziel blieb deshalb dauerhaft
     * gesperrt. Mit diesen zwei Feldern laesst sich der Tag dort setzen, wo er gebraucht wird.
     *
     * Leer gelassen aendert sich nichts.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    TSoftObjectPtr<UWorld> UnlockOnMap;

    /** Der Tag, der auf UnlockOnMap freigeschaltet wird. Siehe dort. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName UnlockSwitchTag;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    bool bIsEnabled = true;

    UPROPERTY(EditAnywhere, Category = "UI")
    FText MarkerDisplayText;

    // Optional display name for the target level, shown in the map-switch widget instead of the map's
    // file name. Leave empty to fall back to the level file name (FPaths::GetBaseFilename).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FText LevelDisplayName;
    
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (MakeEditWidget = true))
    FVector CenterPoint;
    
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float RotationRadius = 500.f;
    
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float RotationSpeed = 0.f;
    
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    UMapSwitchWidget* ActiveWidget;

    FTimerHandle WidgetCloseTimerHandle;

    UPROPERTY(Replicated)
    float CurrentAngle = 0.f;
};
