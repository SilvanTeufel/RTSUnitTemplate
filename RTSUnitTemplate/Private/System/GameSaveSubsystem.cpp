#include "System/GameSaveSubsystem.h"

#include "AITestsCommon.h"
#include "Save/RTSSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Algo/Unique.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/LevelUnit.h"
#include "System/MapSwitchSubsystem.h"
#include "Misc/PackageName.h"
#include "GAS/AttributeSetBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassCommonFragments.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPath.h"

void UGameSaveSubsystem::SaveCurrentGame(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    URTSSaveGame* Save = Cast<URTSSaveGame>(UGameplayStatics::CreateSaveGameObject(URTSSaveGame::StaticClass()));
    if (!Save) return;

    // Map
    Save->SavedMapLongPackageName = World->GetOutermost()->GetName();

    // Zeitstempel
    Save->SavedUnixTimeSeconds = FDateTime::UtcNow().ToUnixTimestamp();

    // Kamera (Spieler 0)
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            Save->CameraData.Location = Pawn->GetActorLocation();
            Save->CameraData.Rotation = Pawn->GetActorRotation();
        }
    }

    // MapSwitch-Status exportieren
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMapSwitchSubsystem* MapSub = GI->GetSubsystem<UMapSwitchSubsystem>())
        {
            TMap<FString, TArray<FName>> Exported;
            MapSub->ExportStateForSave(Exported);

            Save->MapEnabledSwitchTags.Empty();
            Save->MapEnabledSwitchTags.Reserve(Exported.Num());
            for (TPair<FString, TArray<FName>>& Pair : Exported)
            {
                FMapSwitchTagsForMap Entry;
                Entry.MapKey = Pair.Key;
                Entry.Tags = MoveTemp(Pair.Value);
                Save->MapEnabledSwitchTags.Add(MoveTemp(Entry));
            }
        }
    }

    // Einheiten sammeln
    for (TActorIterator<AUnitBase> It(World); It; ++It)
    {
        AUnitBase* Unit = *It;
        if (!Unit) continue;

        FUnitSaveData Data;
        Data.ActorName = Unit->GetName();
        // Klasse der Einheit speichern
        Data.UnitClassPath = FSoftClassPath(Unit->GetClass());
        // Team und Selektierbarkeit speichern
        Data.TeamId = Unit->TeamId;
        Data.bIsSelectable = Unit->CanBeSelected;
        // Zustand speichern
        Data.UnitState = Unit->GetUnitState();
        Data.UnitStatePlaceholder = Unit->UnitStatePlaceholder;
        Data.Location = Unit->GetActorLocation();
        Data.Rotation = Unit->GetActorRotation();

        // Wenn ALevelUnit: UnitIndex, Level- und Attributsdaten direkt mitspeichern
        if (ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit))
        {
            Data.UnitIndex = LevelUnit->UnitIndex;
            // Level-Daten
            Data.LevelData = LevelUnit->LevelData;
            Data.LevelUpData = LevelUnit->LevelUpData;

            // Attribute-Daten auslesen
            if (LevelUnit->Attributes)
            {
                UAttributeSetBase* Attr = LevelUnit->Attributes;
                FAttributeSaveData AttrData;
                AttrData.Health = Attr->GetHealth();
                AttrData.MaxHealth = Attr->GetMaxHealth();
                AttrData.HealthRegeneration = Attr->GetHealthRegeneration();
                AttrData.Shield = Attr->GetShield();
                AttrData.MaxShield = Attr->GetMaxShield();
                AttrData.ShieldRegeneration = Attr->GetShieldRegeneration();
                AttrData.AttackDamage = Attr->GetAttackDamage();
                AttrData.Range = Attr->GetRange();
                AttrData.RunSpeed = Attr->GetRunSpeed();
                AttrData.IsAttackedSpeed = Attr->GetIsAttackedSpeed();
                AttrData.RunSpeedScale = Attr->GetRunSpeedScale();
                AttrData.ProjectileScaleActorDirectionOffset = Attr->GetProjectileScaleActorDirectionOffset();
                AttrData.ProjectileSpeed = Attr->GetProjectileSpeed();
                AttrData.Stamina = Attr->GetStamina();
                AttrData.AttackPower = Attr->GetAttackPower();
                AttrData.Willpower = Attr->GetWillpower();
                AttrData.Haste = Attr->GetHaste();
                AttrData.Armor = Attr->GetArmor();
                AttrData.MagicResistance = Attr->GetMagicResistance();
                AttrData.BaseHealth = Attr->GetBaseHealth();
                AttrData.BaseAttackDamage = Attr->GetBaseAttackDamage();
                AttrData.BaseRunSpeed = Attr->GetBaseRunSpeed();

                Data.AttributeSaveData = AttrData;
            }
        }

        Save->Units.Add(MoveTemp(Data));
    }

    UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

void UGameSaveSubsystem::LoadGameFromSlot(const FString& SlotName)
{
    URTSSaveGame* Save = Cast<URTSSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save) return;

    PendingLoadedSave = Save;
    PendingSlotName = SlotName;

    UWorld* World = GetWorld();
    if (!World) return;

    const FString CurrentLongName = World->GetOutermost()->GetName();
    if (!Save->SavedMapLongPackageName.IsEmpty() && Save->SavedMapLongPackageName != CurrentLongName)
    {
        // Zu gespeicherter Map reisen; danach wird OnPostLoadMapWithWorld aufgerufen.
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGameSaveSubsystem::OnPostLoadMapWithWorld);
        UGameplayStatics::OpenLevel(World, FName(*Save->SavedMapLongPackageName));
    }
    else
    {
        // Gleiche Map, direkt anwenden
        ApplyLoadedData(World, Save);
        PendingLoadedSave = nullptr;
        PendingSlotName.Reset();
    }
}

void UGameSaveSubsystem::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    if (PendingLoadedSave)
    {
        ApplyLoadedData(LoadedWorld, PendingLoadedSave);
        PendingLoadedSave = nullptr;
        PendingSlotName.Reset();
    }
}

void UGameSaveSubsystem::ApplyLoadedData(UWorld* LoadedWorld, URTSSaveGame* SaveData)
{
    if (!LoadedWorld || !SaveData) return;

    // MapSwitch-Status importieren
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMapSwitchSubsystem* MapSub = GI->GetSubsystem<UMapSwitchSubsystem>())
        {
            TMap<FString, TArray<FName>> Imported;
            for (const FMapSwitchTagsForMap& Entry : SaveData->MapEnabledSwitchTags)
            {
                Imported.Add(Entry.MapKey, Entry.Tags);
            }
            MapSub->ImportStateFromSave(Imported);
        }
    }

    // Einheiten abgleichen: per UnitIndex bevorzugt, sonst per Name
    TMap<int32, AUnitBase*> UnitsByIndex;
    TMap<FString, AUnitBase*> UnitsByName;
    TSet<AUnitBase*>          MatchedUnits;

    for (TActorIterator<AUnitBase> It(LoadedWorld); It; ++It)
    {
        AUnitBase* Unit = *It;
        if (!Unit) continue;
        UnitsByName.Add(Unit->GetName(), Unit);
        if (ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit))
        {
            UnitsByIndex.Add(LevelUnit->UnitIndex, Unit);
        }
    }

    for (const FUnitSaveData& SavedUnit : SaveData->Units)
    {
        AUnitBase* Unit = nullptr;

        if (SavedUnit.UnitIndex != INDEX_NONE)
        {
            if (AUnitBase** FoundIndex = UnitsByIndex.Find(SavedUnit.UnitIndex))
            {
                Unit = *FoundIndex;
            }
        }
        if (!Unit)
        {
            if (AUnitBase** FoundByName = UnitsByName.Find(SavedUnit.ActorName))
            {
                Unit = *FoundByName;
            }
        }

        if (!Unit)
        {
            // Einheit existiert nicht -> spawnen (bevorzugt gespeicherte Klasse)
            UClass* SpawnClass = nullptr;

            if (SavedUnit.UnitClassPath.IsValid())
            {
                SpawnClass = SavedUnit.UnitClassPath.TryLoadClass<AUnitBase>();
            }
            if (!SpawnClass)
            {
                SpawnClass = DefaultUnitClass ? *DefaultUnitClass : AUnitBase::StaticClass();
            }
            if (!SpawnClass)
            {
                UE_LOG(LogTemp, Warning, TEXT("ApplyLoadedData: No valid spawn class for '%s'."), *SavedUnit.ActorName);
                continue;
            }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            Unit = LoadedWorld->SpawnActor<AUnitBase>(SpawnClass, SavedUnit.Location, SavedUnit.Rotation, Params);
            if (!Unit)
            {
                UE_LOG(LogTemp, Warning, TEXT("ApplyLoadedData: Failed to spawn unit '%s'."), *SavedUnit.ActorName);
                continue;
            }

            // In Maps registrieren
            UnitsByName.Add(Unit->GetName(), Unit);
            if (ALevelUnit* SpawnedLevel = Cast<ALevelUnit>(Unit))
            {
                if (SavedUnit.UnitIndex != INDEX_NONE)
                {
                    SpawnedLevel->SetUnitIndex(SavedUnit.UnitIndex);
                    UnitsByIndex.Add(SavedUnit.UnitIndex, Unit);
                }
            }
        }

        // Prüfen, ob wir respawnen müssen: gespeicherter Dead-State oder aktueller Dead-Tag/State
        bool bRespawn = (SavedUnit.UnitState == UnitData::Dead) || (Unit->GetUnitState() == UnitData::Dead);
        if (AMassUnitBase* ExistingMass = Cast<AMassUnitBase>(Unit))
        {
            if (UMassEntitySubsystem* MassSubsystem = LoadedWorld->GetSubsystem<UMassEntitySubsystem>())
            {
                FMassEntityManager& EM = MassSubsystem->GetMutableEntityManager();
                const FMassEntityHandle Handle = (ExistingMass->MassActorBindingComponent)
                    ? ExistingMass->MassActorBindingComponent->GetEntityHandle()
                    : FMassEntityHandle();

                if (EM.IsEntityValid(Handle))
                {
                    // DeadTag prüfen
                    if (DoesEntityHaveTag(EM, Handle, FMassStateDeadTag::StaticStruct()))
                    {
                        bRespawn = true;
                    }
                }
            }
        }

        if (bRespawn)
        {
            // Einheit vollständig respawnen (Klasse aus Save bevorzugt)
            UClass* SpawnClass = nullptr;
            if (SavedUnit.UnitClassPath.IsValid())
            {
                SpawnClass = SavedUnit.UnitClassPath.TryLoadClass<AUnitBase>();
            }
            if (!SpawnClass)
            {
                SpawnClass = Unit->GetClass();
            }
            if (!SpawnClass)
            {
                SpawnClass = DefaultUnitClass ? *DefaultUnitClass : AUnitBase::StaticClass();
            }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            AUnitBase* NewUnit = LoadedWorld->SpawnActor<AUnitBase>(SpawnClass, SavedUnit.Location, SavedUnit.Rotation, Params);
            if (NewUnit)
            {
                // alte Instanz entfernen und Referenzen aktualisieren
                Unit->Destroy();
                Unit = NewUnit;

                // Maps aktualisieren
                UnitsByName.Add(Unit->GetName(), Unit);
                if (SavedUnit.UnitIndex != INDEX_NONE)
                {
                    if (ALevelUnit* NewLevel = Cast<ALevelUnit>(Unit))
                    {
                        NewLevel->SetUnitIndex(SavedUnit.UnitIndex);
                        UnitsByIndex.Add(SavedUnit.UnitIndex, Unit);
                    }
                }
            }
        }

        // Transform anwenden
        Unit->SetActorLocation(SavedUnit.Location);
        Unit->SetActorRotation(SavedUnit.Rotation);

        // Team/Selektierbarkeit anwenden
        Unit->TeamId = SavedUnit.TeamId;
        Unit->CanBeSelected = SavedUnit.bIsSelectable;

        // Zustand anwenden
        Unit->UnitStatePlaceholder = SavedUnit.UnitStatePlaceholder;
        Unit->SetUnitState(SavedUnit.UnitState);

        // Level-Daten anwenden (Attribute folgen danach)
        if (ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit))
        {
            LevelUnit->LevelData = SavedUnit.LevelData;
            LevelUnit->LevelUpData = SavedUnit.LevelUpData;
        }

        // Mass-Entität auf die neue Actor-Position synchronisieren, Targets anpassen und Tags setzen
        if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit))
        {
            MassUnit->SetTranslationLocation(SavedUnit.Location);

            if (UMassEntitySubsystem* MassSubsystem = LoadedWorld->GetSubsystem<UMassEntitySubsystem>())
            {
                FMassEntityManager& EM = MassSubsystem->GetMutableEntityManager();
                const FMassEntityHandle Handle = (MassUnit->MassActorBindingComponent)
                    ? MassUnit->MassActorBindingComponent->GetEntityHandle()
                    : FMassEntityHandle();

                if (EM.IsEntityValid(Handle))
                {
                    if (FMassMoveTargetFragment* MoveTargetFragmentPtr = EM.GetFragmentDataPtr<FMassMoveTargetFragment>(Handle))
                    {
                        MoveTargetFragmentPtr->Center = SavedUnit.Location;
                    }
                    if (FMassAIStateFragment* AiStatePtr = EM.GetFragmentDataPtr<FMassAIStateFragment>(Handle))
                    {
                        AiStatePtr->StoredLocation = SavedUnit.Location;
                    }
                }
            }
            
            // Korrekte Mass-Tags gemäß gespeichertem Zustand setzen
            MassUnit->SwitchEntityTagByState(SavedUnit.UnitState, SavedUnit.UnitStatePlaceholder);
        }

        // Jetzt alle gespeicherten Attribute anwenden
        if (ALevelUnit* LevelUnitForAttr = Cast<ALevelUnit>(Unit))
        {
            if (LevelUnitForAttr->Attributes)
            {
                LevelUnitForAttr->Attributes->UpdateAttributes(SavedUnit.AttributeSaveData);
            }
        }

        MatchedUnits.Add(Unit);
    }

    // Überzählige Einheiten entfernen (nicht im Save vorhanden)
    for (TActorIterator<AUnitBase> ItRemove(LoadedWorld); ItRemove; ++ItRemove)
    {
        AUnitBase* Existing = *ItRemove;
        if (!Existing) continue;
        if (!MatchedUnits.Contains(Existing))
        {
            Existing->Destroy();
        }
    }

    // Kamera (Spieler 0)
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(LoadedWorld, 0))
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->SetActorLocation(SaveData->CameraData.Location);
            Pawn->SetActorRotation(SaveData->CameraData.Rotation);
        }
    }
}

TArray<FString> UGameSaveSubsystem::GetAllSaveSlots() const
{
    TArray<FString> Result;
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(SaveDir, TEXT("*.sav")), true, false);
    for (const FString& File : Files)
    {
        Result.Add(FPaths::GetBaseFilename(File));
    }
    Result.Sort();
    Result.SetNum(Algo::Unique(Result));
    return Result;
}

bool UGameSaveSubsystem::LoadSaveSummary(const FString& SlotName, FString& OutMapAssetName, FString& OutLongPackageName, int64& OutUnixTime) const
{
    OutMapAssetName.Reset();
    OutLongPackageName.Reset();
    OutUnixTime = 0;

    USaveGame* SG = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
    URTSSaveGame* RTS = Cast<URTSSaveGame>(SG);
    if (!RTS) return false;

    OutLongPackageName = RTS->SavedMapLongPackageName;
    OutUnixTime = RTS->SavedUnixTimeSeconds;

    // Asset-Name aus Long-Package ableiten
    OutMapAssetName = FPackageName::GetLongPackageAssetName(OutLongPackageName);
    if (OutMapAssetName.IsEmpty())
    {
        OutMapAssetName = FPaths::GetBaseFilename(OutLongPackageName);
    }
    return true;
}
