// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/UnitData.h"
#include "Core/Talents.h"
#include "Core/WorkerData.h"
#include "UObject/SoftObjectPath.h"
#include "Actors/WorkArea.h"
#include "Actors/WinLoseConfigActor.h"
#include "RTSSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FCameraSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FAbilitySaveData
{
    GENERATED_BODY()

    // Ability class to identify which ability this refers to
    UPROPERTY()
    FSoftClassPath AbilityClass;

    // Optional: key used by ability gating system
    UPROPERTY()
    FString AbilityKey;

    // Saved owner-level toggles
    UPROPERTY()
    bool bOwnerDisabled = false;

    UPROPERTY()
    bool bOwnerForceEnabled = false;
};

// Persisted per-node investment of the radial attribute tree (mirrors ALevelUnit::FAttributeTreeNodeState).
// Kept as a bespoke save struct so the save format stays decoupled from the gameplay class header.
USTRUCT(BlueprintType)
struct FAttributeTreeNodeSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FName NodeId = NAME_None;

    UPROPERTY()
    int32 Points = 0;
};

USTRUCT(BlueprintType)
struct FUnitSaveData
{
    GENERATED_BODY()

    // Optionaler identifizierender Index (falls vorhanden)
    UPROPERTY()
    int32 UnitIndex = INDEX_NONE;

    // Name des Actors als Fallback
    UPROPERTY()
    FString ActorName;

    // Soft-Klassenpfad der Einheit, um sie exakt wieder zu spawnen
    UPROPERTY()
    FSoftClassPath UnitClassPath;

    // Team-ID und Selektierbarkeit der Einheit
    UPROPERTY()
    uint8 TeamId = 0;

    UPROPERTY()
    bool bIsSelectable = true;

    // Gespeicherter Zustand und Placeholder-Zustand der Einheit
    UPROPERTY()
    TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;

    UPROPERTY()
    TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Idle;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    // Direkt gespeicherte Level- und Attribut-Daten (statt separater Savegames)
    UPROPERTY()
    FLevelData LevelData;

    UPROPERTY()
    FLevelUpData LevelUpData;

    UPROPERTY()
    FAttributeSaveData AttributeSaveData;

    // Per-node investment of the radial attribute talent tree (AttributeTreeWidget).
    // Restored as raw state: the resulting GAS attribute values are already captured in
    // AttributeSaveData, so this must NOT be re-invested on load.
    UPROPERTY()
    TArray<FAttributeTreeNodeSaveData> AttributeTreeNodes;

    // Der EIGENE Punktevorrat des Attributbaums. Seit dem 10.09.2026 nicht mehr identisch mit
    // LevelData.TalentPoints - ohne diese beiden Felder waeren die Punkte des Baums nach dem
    // Laden weg, waehrend die investierten Knoten oben stehen blieben.
    // Aeltere Spielstaende bringen die Felder nicht mit und laden sie als 0.
    UPROPERTY()
    int32 AttributeTreePoints = 0;

    UPROPERTY()
    int32 UsedAttributeTreePoints = 0;

    // Saved abilities states for this unit
    UPROPERTY()
    TArray<FAbilitySaveData> Abilities;

    UPROPERTY()
    TMap<FString, FString> SerializedModuleData;
};

/**
 * Laufzeitstand einer Siegbedingung (02.09.2026 ergaenzt).
 * Ohne das setzt ein Spielstand den Fortschritt zum Ziel zurueck: welcher Abschnitt gerade
 * laeuft und wieviel davon geschafft ist, stand bisher in keinem Slot.
 */
USTRUCT()
struct FWinLoseSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    int32 TeamId = 0;

    /** Name des Actors, um bei mehreren Konfigurationen die richtige wiederzufinden. */
    UPROPERTY()
    FString ActorName;

    UPROPERTY()
    int32 CurrentWinConditionIndex = 0;

    UPROPERTY()
    TArray<FTagProgress> TagProgress;
};

USTRUCT(BlueprintType)
struct FMapSwitchTagsForMap
{
    GENERATED_BODY()

    // Normalisierter Map-Key (Asset-Name, z. B. "LevelFive")
    UPROPERTY()
    FString MapKey;

    // Aktivierte Tags für diese Map
    UPROPERTY()
    TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct FWorkAreaSaveData
{
    GENERATED_BODY()

    // Identifikation
    UPROPERTY()
    FString Tag;

    // Klasse und Transform
    UPROPERTY()
    FSoftClassPath WorkAreaClass;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    FVector Scale3D = FVector(1.f, 1.f, 1.f);

    // Properties
    UPROPERTY()
    int32 TeamId = 0;

    UPROPERTY()
    bool IsNoBuildZone = false;

    UPROPERTY()
    TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkAreaData::Primary;

    UPROPERTY()
    FSoftClassPath WorkResourceClass;

    UPROPERTY()
    FSoftClassPath BuildingClass;

    UPROPERTY()
    FSoftClassPath BuildingControllerClass;

    UPROPERTY()
    float BuildTime = 5.f;

    UPROPERTY()
    float CurrentBuildTime = 0.0f;

    UPROPERTY()
    float AvailableResourceAmount = 0.f;

    UPROPERTY()
    float MaxAvailableResourceAmount = 0.f;

    UPROPERTY()
    float BuildZOffset = 0.f;

    UPROPERTY()
    bool PlannedBuilding = false;

    UPROPERTY()
    bool StartedBuilding = false;

    UPROPERTY()
    bool DestroyAfterBuild = true;

    UPROPERTY()
    FBuildingCost ConstructionCost;

    UPROPERTY()
    float ResetStartBuildTime = 25.f;

    UPROPERTY()
    float ControlTimer = 0.f;

    UPROPERTY()
    bool IsPaid = false;

    UPROPERTY()
    FSoftClassPath AreaEffectClass;
};

UCLASS()
class RTSUNITTEMPLATE_API URTSSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // Long Package Name der Map (z.B. "/Game/Maps/MyMap")
    UPROPERTY()
    FString SavedMapLongPackageName;

    // Zeitpunkt der Speicherung (UTC, Unix-Sekunden)
    UPROPERTY()
    int64 SavedUnixTimeSeconds = 0;

    // Kamera-Daten (Spieler 0)
    UPROPERTY()
    FCameraSaveData CameraData;

    // Einheiten-Daten
    UPROPERTY()
    TArray<FUnitSaveData> Units;

    // WorkAreas auf dem Feld
    UPROPERTY()
    TArray<FWorkAreaSaveData> WorkAreas;

    // Aktivierte MapSwitch-Tags pro Map (MapKey normalisiert: Assetname)
    UPROPERTY()
    TArray<FMapSwitchTagsForMap> MapEnabledSwitchTags;

    // Team resource economy (banked resources, capacities and worker distribution per team).
    // Server-authoritative copy taken from AResourceGameMode::TeamResources. Empty on saves made
    // before this field existed, so the loader must skip restore when empty.
    UPROPERTY()
    TArray<FResourceArray> TeamResources;

    // Fortschritt der Siegbedingungen je Team.
    UPROPERTY()
    TArray<FWinLoseSaveData> WinLoseStates;

    /**
     * Verstrichene Spielzeit. World->GetTimeSeconds() faengt beim Laden wieder bei 0 an -
     * eine Bedingung "ueberlebe N Sekunden" und der 60-s-Takt der Talentpunkte wuerden sonst
     * von vorn beginnen.
     */
    UPROPERTY()
    float SavedGameTimeSeconds = 0.f;
};
