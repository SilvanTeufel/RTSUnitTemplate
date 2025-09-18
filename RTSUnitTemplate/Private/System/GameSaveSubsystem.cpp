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
        AUnitBase** FoundPtr = nullptr;

        if (SavedUnit.UnitIndex != INDEX_NONE)
        {
            FoundPtr = UnitsByIndex.Find(SavedUnit.UnitIndex);
        }
        if (!FoundPtr)
        {
            FoundPtr = UnitsByName.Find(SavedUnit.ActorName);
        }

        if (FoundPtr && *FoundPtr)
        {
            AUnitBase* Unit = *FoundPtr;
            Unit->SetActorLocation(SavedUnit.Location);
            Unit->SetActorRotation(SavedUnit.Rotation);

            if (ALevelUnit* LevelUnit = Cast<ALevelUnit>(Unit))
            {
                // Level- und Attributsdaten direkt anwenden
                LevelUnit->LevelData = SavedUnit.LevelData;
                LevelUnit->LevelUpData = SavedUnit.LevelUpData;
                if (LevelUnit->Attributes)
                {
                    LevelUnit->Attributes->UpdateAttributes(SavedUnit.AttributeSaveData);
                }
            }

            // Mass-Entität auf die neue Actor-Position synchronisieren
            if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit))
            {
                MassUnit->SetTranslationLocation(SavedUnit.Location);

                // Zusätzlich den MoveTarget und AI-Storage auf die geladene Position setzen,
                // damit die Einheit nicht zum alten Ziel weiterläuft.
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
            }
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
