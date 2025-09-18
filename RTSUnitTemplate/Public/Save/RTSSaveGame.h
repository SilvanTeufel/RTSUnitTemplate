#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/UnitData.h"
#include "Core/Talents.h"
#include "UObject/SoftObjectPath.h"
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

    // Aktivierte MapSwitch-Tags pro Map (MapKey normalisiert: Assetname)
    UPROPERTY()
    TArray<FMapSwitchTagsForMap> MapEnabledSwitchTags;
};
