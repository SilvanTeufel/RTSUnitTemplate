#include "System/GameSaveSubsystem.h"
#include "HAL/FileManager.h"
#include "Save/RTSSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
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
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "AIController.h"
#include "Actors/WorkResource.h"

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

    // WorkAreas sammeln
    Save->WorkAreas.Empty();
    for (TActorIterator<AWorkArea> ItWA(World); ItWA; ++ItWA)
    {
        AWorkArea* WA = *ItWA;
        if (!WA) continue;

        FWorkAreaSaveData W;
        W.Tag = WA->Tag;
        W.WorkAreaClass = FSoftClassPath(WA->GetClass());
        W.Location = WA->GetActorLocation();
        W.Rotation = WA->GetActorRotation();
        W.Scale3D = WA->GetActorScale3D();

        W.TeamId = WA->TeamId;
        W.IsNoBuildZone = WA->IsNoBuildZone;
        W.Type = WA->Type;
        W.WorkResourceClass = WA->WorkResourceClass ? FSoftClassPath(*WA->WorkResourceClass) : FSoftClassPath();
        W.BuildingClass = WA->BuildingClass ? FSoftClassPath(*WA->BuildingClass) : FSoftClassPath();
        W.BuildingControllerClass = WA->BuildingController ? FSoftClassPath(*WA->BuildingController) : FSoftClassPath();
        W.BuildTime = WA->BuildTime;
        W.CurrentBuildTime = WA->CurrentBuildTime;
        W.AvailableResourceAmount = WA->AvailableResourceAmount;
        W.MaxAvailableResourceAmount = WA->MaxAvailableResourceAmount;
        W.BuildZOffset = WA->BuildZOffset;
        W.PlannedBuilding = WA->PlannedBuilding;
        W.StartedBuilding = WA->StartedBuilding;
        W.DestroyAfterBuild = WA->DestroyAfterBuild;
        W.ConstructionCost = WA->ConstructionCost;
        W.ResetStartBuildTime = WA->ResetStartBuildTime;
        W.ControlTimer = WA->ControlTimer;
        W.IsPaid = WA->IsPaid;
        W.AreaEffectClass = WA->AreaEffect ? FSoftClassPath(*WA->AreaEffect) : FSoftClassPath();

        Save->WorkAreas.Add(MoveTemp(W));
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
                SpawnClass = DefaultUnitClass ? DefaultUnitClass.Get() : AUnitBase::StaticClass();
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
                SpawnClass = DefaultUnitClass ? DefaultUnitClass.Get() : AUnitBase::StaticClass();
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

    // WorkAreas abgleichen
    TArray<AWorkArea*> ExistingAreas;
    TMap<FString, TArray<AWorkArea*>> ExistingByTag; // mehrere pro Tag zulassen
    for (TActorIterator<AWorkArea> ItWA(LoadedWorld); ItWA; ++ItWA)
    {
        AWorkArea* WA = *ItWA;
        if (!WA) continue;
        ExistingAreas.Add(WA);
        ExistingByTag.FindOrAdd(WA->Tag).Add(WA);
    }

    TSet<AWorkArea*> MatchedAreas;

    // Hilfsfunktion: wähle erstes nicht gematchtes aus Liste
    auto TakeUnusedWithSameTag = [&MatchedAreas](const TArray<AWorkArea*>& List) -> AWorkArea*
    {
        for (AWorkArea* Candidate : List)
        {
            if (Candidate && !MatchedAreas.Contains(Candidate))
            {
                return Candidate;
            }
        }
        return nullptr;
    };

    for (const FWorkAreaSaveData& SavedWA : SaveData->WorkAreas)
    {
        AWorkArea* WA = nullptr;

        // 1) Tag-Matching (nur wenn nicht leer)
        if (!SavedWA.Tag.IsEmpty())
        {
            if (TArray<AWorkArea*>* FoundList = ExistingByTag.Find(SavedWA.Tag))
            {
                WA = TakeUnusedWithSameTag(*FoundList);
            }
        }

        // 2) Fallback: per Distanz und (wenn möglich) Klasse matchen
        if (!WA)
        {
            UClass* SavedClass = SavedWA.WorkAreaClass.IsValid() ? SavedWA.WorkAreaClass.TryLoadClass<AWorkArea>() : nullptr;
            const float MaxDistSq = FMath::Square(200.f); // Toleranz

            float BestDistSq = TNumericLimits<float>::Max();
            AWorkArea* Best = nullptr;
            for (AWorkArea* Candidate : ExistingAreas)
            {
                if (!Candidate || MatchedAreas.Contains(Candidate)) continue;

                const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), SavedWA.Location);
                if (DistSq > MaxDistSq) continue;

                // Wenn Klasseninfo vorhanden, bevorzugt gleiche Klasse
                const bool ClassOk = (!SavedClass) || Candidate->GetClass() == SavedClass;
                if (!ClassOk) continue;

                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    Best = Candidate;
                }
            }

            WA = Best;
        }

        // 3) Spawnen falls kein Match gefunden
        if (!WA)
        {
            UClass* WAClass = SavedWA.WorkAreaClass.IsValid() ? SavedWA.WorkAreaClass.TryLoadClass<AWorkArea>() : nullptr;
            if (!WAClass)
            {
                WAClass = AWorkArea::StaticClass();
            }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            WA = LoadedWorld->SpawnActor<AWorkArea>(WAClass, SavedWA.Location, SavedWA.Rotation, Params);
            if (!WA)
            {
                UE_LOG(LogTemp, Warning, TEXT("ApplyLoadedData: Failed to spawn WorkArea with tag '%s'."), *SavedWA.Tag);
                continue;
            }
            ExistingAreas.Add(WA);
            ExistingByTag.FindOrAdd(SavedWA.Tag).Add(WA);
        }

        // Transform anwenden
        WA->SetActorLocation(SavedWA.Location);
        WA->SetActorRotation(SavedWA.Rotation);
        WA->SetActorScale3D(SavedWA.Scale3D);

        // Properties
        WA->Tag = SavedWA.Tag;
        WA->TeamId = SavedWA.TeamId;
        WA->IsNoBuildZone = SavedWA.IsNoBuildZone;
        WA->Type = SavedWA.Type;

        if (UClass* WR = SavedWA.WorkResourceClass.IsValid() ? SavedWA.WorkResourceClass.TryLoadClass<AWorkResource>() : nullptr)
        {
            WA->WorkResourceClass = WR;
        }
        if (UClass* BC = SavedWA.BuildingClass.IsValid() ? SavedWA.BuildingClass.TryLoadClass<ABuildingBase>() : nullptr)
        {
            WA->BuildingClass = BC;
        }
        if (UClass* BCC = SavedWA.BuildingControllerClass.IsValid() ? SavedWA.BuildingControllerClass.TryLoadClass<AAIController>() : nullptr)
        {
            WA->BuildingController = BCC;
        }

        WA->BuildTime = SavedWA.BuildTime;
        WA->CurrentBuildTime = SavedWA.CurrentBuildTime;
        WA->AvailableResourceAmount = SavedWA.AvailableResourceAmount;
        WA->MaxAvailableResourceAmount = SavedWA.MaxAvailableResourceAmount;
        WA->BuildZOffset = SavedWA.BuildZOffset;
        WA->PlannedBuilding = SavedWA.PlannedBuilding;
        WA->StartedBuilding = SavedWA.StartedBuilding;
        WA->DestroyAfterBuild = SavedWA.DestroyAfterBuild;
        WA->ConstructionCost = SavedWA.ConstructionCost;
        WA->ResetStartBuildTime = SavedWA.ResetStartBuildTime;
        WA->ControlTimer = SavedWA.ControlTimer;
        WA->IsPaid = SavedWA.IsPaid;

        if (UClass* AEC = SavedWA.AreaEffectClass.IsValid() ? SavedWA.AreaEffectClass.TryLoadClass<UGameplayEffect>() : nullptr)
        {
            WA->AreaEffect = AEC;
        }

        MatchedAreas.Add(WA);
    }

    // Nicht gespeicherte WorkAreas entfernen
    for (AWorkArea* WA : ExistingAreas)
    {
        if (WA && !MatchedAreas.Contains(WA))
        {
            WA->Destroy();
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
