#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/UnitData.h"
#include "Core/Talents.h"
#include "UObject/SoftObjectPath.h"
#include "Actors/WorkArea.h"
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

    // Saved abilities states for this unit
    UPROPERTY()
    TArray<FAbilitySaveData> Abilities;
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
};
