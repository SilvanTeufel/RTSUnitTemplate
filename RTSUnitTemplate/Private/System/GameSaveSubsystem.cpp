// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "System/GameSaveSubsystem.h"
#include "GameStates/UpgradeGameState.h"
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
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayAbilityBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/GASUnit.h"
#include "GameModes/RTSGameModeBase.h"
#include "GameModes/ResourceGameMode.h"
#include "GameStates/ResourceGameState.h"

void UGameSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Die Freischaltungen erst holen, wenn die erste Karte steht.
    //
    // Hier direkt geht es nicht: waehrend Initialize() laufen die uebrigen
    // GameInstance-Subsysteme unter Umstaenden noch gar nicht, und ohne UMapSwitchSubsystem
    // gaebe es nichts, wohin der Zustand geschrieben werden koennte. PostLoadMapWithWorld
    // feuert dagegen sicher danach - und das Menue ist die erste Karte, die geladen wird.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGameSaveSubsystem::OnPostLoadMapWithWorld);
}

void UGameSaveSubsystem::Deinitialize()
{
    // Never leave a dangling PostLoadMapWithWorld binding across GameInstance / PIE teardown.
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    bPendingQuickSave = false;
    PendingLoadedSave = nullptr;
    PendingSlotName.Reset();

    Super::Deinitialize();
}

void UGameSaveSubsystem::SaveCurrentGame(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // Clients must never write an authoritative save: on a networked client most state (GAS, Mass, resources)
    // is only partially replicated, so the resulting slot would be incomplete. Saving is server/standalone-only.
    if (World->GetNetMode() == NM_Client) return;

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
        Data.Location = Unit->GetMassActorLocation();
        Data.Rotation = Unit->GetMassActorRotation();

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

            // Radial attribute-tree investment (per-node point counts). The attribute values these
            // produced are already stored in AttrData above; here we persist only the node bookkeeping.
            // Without it, after load the tree UI shows every node at 0/Max, unlock gating re-locks every
            // non-root branch (IsAttributeTreeNodeUnlocked reads the parent node's Points), and maxed
            // nodes read as investable again -> phantom re-invest that double-raises attributes.
            Data.AttributeTreeNodes.Reset(LevelUnit->AttributeTreeNodes.Num());
            for (const FAttributeTreeNodeState& NodeState : LevelUnit->AttributeTreeNodes)
            {
                FAttributeTreeNodeSaveData NodeSave;
                NodeSave.NodeId = NodeState.NodeId;
                NodeSave.Points = NodeState.Points;
                Data.AttributeTreeNodes.Add(NodeSave);
            }

            // Der Vorrat steht nicht mehr hier, sondern einmal je Team weiter unten.
        }

        // Ability states (owner-level toggles) for this unit
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Unit))
        {
            if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
            {
                if (AGASUnit* GASUnit = Cast<AGASUnit>(Unit))
                {
                    auto CollectFromList = [&](const TArray<TSubclassOf<UGameplayAbilityBase>>& List)
                    {
                        for (const TSubclassOf<UGameplayAbilityBase>& AbilityClass : List)
                        {
                            if (!AbilityClass) continue;
                            const UGameplayAbilityBase* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
                            if (!AbilityCDO) continue;

                            FAbilitySaveData AbilitySave;
                            AbilitySave.AbilityClass = FSoftClassPath(AbilityClass);
                            AbilitySave.AbilityKey = AbilityCDO->AbilityKey;
                            AbilitySave.bOwnerDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForOwner(ASC, AbilitySave.AbilityKey);
                            AbilitySave.bOwnerForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForOwner(ASC, AbilitySave.AbilityKey);
                            Data.Abilities.Add(MoveTemp(AbilitySave));
                        }
                    };

                    CollectFromList(GASUnit->DefaultAbilities);
                    CollectFromList(GASUnit->SecondAbilities);
                    CollectFromList(GASUnit->ThirdAbilities);
                    CollectFromList(GASUnit->FourthAbilities);
                }
            }
        }

        if (OnUnitSave.IsBound())
        {
            OnUnitSave.Broadcast(Unit, Data);
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

    // Team resource economy. Read from the server-authoritative GameMode (the GameState only holds a
    // replicated copy). Skips silently if the active GameMode has no resource economy.
    if (AResourceGameMode* ResourceGM = World->GetAuthGameMode<AResourceGameMode>())
    {
        Save->TeamResources = ResourceGM->TeamResources;
    }

    // Fortschritt der Siegbedingungen (02.09.2026). Fehlte bisher komplett: nach dem Laden
    // stand wieder der erste Abschnitt an und alle Zaehler auf null.
    for (TActorIterator<AWinLoseConfigActor> It(World); It; ++It)
    {
        AWinLoseConfigActor* Config = *It;
        if (!IsValid(Config)) continue;

        FWinLoseSaveData S;
        S.TeamId = Config->TeamId;
        S.ActorName = Config->GetName();
        S.CurrentWinConditionIndex = Config->CurrentWinConditionIndex;
        S.TagProgress = Config->TagProgress;
        Save->WinLoseStates.Add(MoveTemp(S));
    }

    // Attributbaum je Team mitschreiben. Seit dem 20.09.2026 liegt der Vorrat dort und nicht
    // mehr bei den Einheiten; ohne diesen Block waere der gesamte Baumfortschritt nach dem
    // Laden verloren.
    if (const AUpgradeGameState* UpgradeState = World->GetGameState<AUpgradeGameState>())
    {
        for (int32 TeamIndex = 0; TeamIndex < UpgradeState->TeamAttributeTrees.Num(); ++TeamIndex)
        {
            const FTeamAttributeTree& Tree = UpgradeState->TeamAttributeTrees[TeamIndex];

            FTeamAttributeTreeSaveData TreeSave;
            TreeSave.TeamId = TeamIndex + 1;  // Indizierung wie im GameState: TeamIndex = TeamId - 1
            TreeSave.AvailablePoints = Tree.AvailablePoints;
            TreeSave.UsedPoints = Tree.UsedPoints;
            for (const FAttributeTreeNodeState& Node : Tree.Nodes)
            {
                TreeSave.NodeIds.Add(Node.NodeId);
                TreeSave.NodePoints.Add(Node.Points);
            }
            Save->TeamAttributeTrees.Add(MoveTemp(TreeSave));
        }
    }

    // Verstrichene Spielzeit mitschreiben, damit Zeitziele und der Talentpunkt-Takt nach dem
    // Laden nicht von vorn beginnen.
    Save->SavedGameTimeSeconds = World->GetTimeSeconds();

    UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

void UGameSaveSubsystem::LoadGameFromSlot(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // Symmetric with SaveCurrentGame: a networked client must not drive an authoritative load. Doing so
    // would OpenLevel and locally spawn/destroy non-authoritative actors, desyncing from the server.
    // Loading is server/standalone-only; a client requesting a load should route through the host.
    if (World->GetNetMode() == NM_Client) return;

    URTSSaveGame* Save = Cast<URTSSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save) return;

    PendingLoadedSave = Save;
    PendingSlotName = SlotName;
    bPendingQuickSave = false;

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

void UGameSaveSubsystem::SetPendingQuickSave(bool bPending)
{
    bPendingQuickSave = bPending;

    // Always drop any prior binding first so repeated arming (or arming that never leads to a map load) can't
    // stack duplicate callbacks or leak a binding. The single callback also serves the LoadGame path.
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

    if (bPendingQuickSave)
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGameSaveSubsystem::OnPostLoadMapWithWorld);
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
    else
    {
        // Kein angefordertes Laden - also der ganz normale Programmstart ins Menue.
        // Nur die Freischaltungen holen, sonst nichts. Bei einem echten Ladevorgang oben
        // waere das doppelt: ApplyLoadedData bringt sie ohnehin mit.
        RestoreUnlocksFromLatestSave();
    }

    if (bPendingQuickSave)
    {
        FString NewSlotName = GetUniqueSaveSlotName(TEXT("QuickSave"));
        SaveCurrentGame(NewSlotName);
        bPendingQuickSave = false;
    }
}

FString UGameSaveSubsystem::GetUniqueSaveSlotName(const FString& BaseName) const
{
    if (!UGameplayStatics::DoesSaveGameExist(BaseName, 0))
    {
        return BaseName;
    }

    int32 Counter = 1;
    while (UGameplayStatics::DoesSaveGameExist(FString::Printf(TEXT("%s_%d"), *BaseName, Counter), 0))
    {
        Counter++;
    }

    return FString::Printf(TEXT("%s_%d"), *BaseName, Counter);
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

    // Team resource economy. Write back onto the authoritative GameMode, then mirror to the GameState
    // (its replicated copy that drives the resource UI). Skip when the save carried no resources
    // (older save) so we don't wipe the freshly-initialized economy.
    if (SaveData->TeamResources.Num() > 0)
    {
        if (AResourceGameMode* ResourceGM = LoadedWorld->GetAuthGameMode<AResourceGameMode>())
        {
            ResourceGM->TeamResources = SaveData->TeamResources;
            if (AResourceGameState* ResourceGS = LoadedWorld->GetGameState<AResourceGameState>())
            {
                ResourceGS->SetTeamResources(ResourceGM->TeamResources);
            }
        }
    }

    // Attributbaum je Team zurueckspielen. Leer bei Spielstaenden von vor dem 20.09.2026 -
    // dann bleibt der Baum leer, statt mit halben Daten zu starten.
    if (SaveData->TeamAttributeTrees.Num() > 0)
    {
        if (AUpgradeGameState* UpgradeState = LoadedWorld->GetGameState<AUpgradeGameState>())
        {
            for (const FTeamAttributeTreeSaveData& TreeSave : SaveData->TeamAttributeTrees)
            {
                UpgradeState->ResetTeamAttributeTree(TreeSave.TeamId);
                UpgradeState->GrantTeamAttributeTreePoints(TreeSave.TeamId, TreeSave.AvailablePoints + TreeSave.UsedPoints);

                // Ueber denselben Weg wie im Spiel buchen, damit Vorrat und Ausgegebenes
                // zueinander passen; die Wirkung auf den Einheiten holt danach die Nachvergabe.
                const int32 Count = FMath::Min(TreeSave.NodeIds.Num(), TreeSave.NodePoints.Num());
                for (int32 i = 0; i < Count; ++i)
                {
                    for (int32 Step = 0; Step < TreeSave.NodePoints[i]; ++Step)
                    {
                        UpgradeState->InvestTeamAttributeTreeNode(TreeSave.TeamId, TreeSave.NodeIds[i], MAX_int32);
                    }
                }
            }

            // Die Einheiten holen sich den wiederhergestellten Stand selbst - dieselbe
            // Nachvergabe, die auch neu gespawnte Einheiten bedient.
            for (TActorIterator<ALevelUnit> It(LoadedWorld); It; ++It)
            {
                if (ALevelUnit* Unit = *It)
                {
                    Unit->SyncAttributeTreeFromTeam();
                }
            }
        }
    }

    // Fortschritt der Siegbedingungen zurueckspielen (02.09.2026). Zuordnung ueber den
    // Actornamen, ersatzweise ueber die Team-Id - eine Karte kann mehrere Konfigurationen
    // haben, und die Reihenfolge der Actor-Iteration ist nicht garantiert.
    if (SaveData->WinLoseStates.Num() > 0)
    {
        for (TActorIterator<AWinLoseConfigActor> It(LoadedWorld); It; ++It)
        {
            AWinLoseConfigActor* Config = *It;
            if (!IsValid(Config)) continue;

            const FWinLoseSaveData* Passend = SaveData->WinLoseStates.FindByPredicate(
                [Config](const FWinLoseSaveData& S){ return S.ActorName == Config->GetName(); });
            if (!Passend)
            {
                Passend = SaveData->WinLoseStates.FindByPredicate(
                    [Config](const FWinLoseSaveData& S){ return S.TeamId == Config->TeamId; });
            }
            if (!Passend) continue;

            Config->CurrentWinConditionIndex = Passend->CurrentWinConditionIndex;
            Config->TagProgress = Passend->TagProgress;
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

    for (FUnitSaveData& SavedUnit : SaveData->Units)
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

            // Restore the radial attribute-tree bookkeeping as raw state. We deliberately do NOT
            // call InvestInAttributeTreeNode here: the GAS attribute values are re-applied below via
            // UpdateAttributes(AttributeSaveData) and the point pool comes from LevelData, so
            // re-investing would double-spend points and re-raise (already-restored) attributes.
            // AttributeTreeNodes is replicated, so setting it on the authority reaches all clients.
            LevelUnit->AttributeTreeNodes.Reset(SavedUnit.AttributeTreeNodes.Num());
            for (const FAttributeTreeNodeSaveData& NodeSave : SavedUnit.AttributeTreeNodes)
            {
                FAttributeTreeNodeState NodeState;
                NodeState.NodeId = NodeSave.NodeId;
                NodeState.Points = NodeSave.Points;
                LevelUnit->AttributeTreeNodes.Add(NodeState);
            }

            // Der Vorrat wird einmal je Team wiederhergestellt, nicht je Einheit.
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

        // Re-apply saved ability states (owner-level toggles) and optional execute-on-load
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Unit))
        {
            if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
            {
                auto MirrorToClients = [&](const FString& Key, bool bEnable)
                {
                    if (!Unit->HasAuthority()) return;
                    UWorld* World = Unit->GetWorld();
                    if (!World) return;
                    int32 SentCount = 0;
                    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                    {
                        ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(It->Get());
                        if (!CustomPC) continue;
                        if (CustomPC->SelectableTeamId == Unit->TeamId)
                        {
                            CustomPC->Client_ApplyOwnerAbilityKeyToggle(Unit, Key, bEnable);
                            ++SentCount;
                        }
                    }
                };

                for (const FAbilitySaveData& SavedAbility : SavedUnit.Abilities)
                {
                    const FString& Key = SavedAbility.AbilityKey;

                    // Apply force-enabled state first
                    if (SavedAbility.bOwnerForceEnabled)
                    {
                        UGameplayAbilityBase::ApplyOwnerAbilityKeyToggle_Local(ASC, Key, true);
                        MirrorToClients(Key, true);
                        continue; // force enable overrides disabled
                    }

                    if (SavedAbility.bOwnerDisabled)
                    {
                        bool bExecutedInstead = false;
                        if (SavedAbility.AbilityClass.IsValid())
                        {
                            if (UClass* AbilityCls = SavedAbility.AbilityClass.TryLoadClass<UGameplayAbilityBase>())
                            {
                                if (const UGameplayAbilityBase* CDO = AbilityCls->GetDefaultObject<UGameplayAbilityBase>())
                                {
                                    if (CDO->bExecuteOnLoadIfDisabled)
                                    {
                                        // Try to activate this ability instead of disabling it
                                        if (ASC->TryActivateAbilityByClass(AbilityCls, true))
                                        {
                                            bExecutedInstead = true;
                                        }
                                        else
                                        {
                                            // If activation failed, still choose not to disable per requirement
                                            bExecutedInstead = true;
                                        }
                                    }
                                }
                            }
                        }

                        // If not executed-instead, then apply disable
                        if (!bExecutedInstead)
                        {
                            UGameplayAbilityBase::ApplyOwnerAbilityKeyToggle_Local(ASC, Key, false);
                            MirrorToClients(Key, false);
                        }
                    }
                }
            }
        }

        if (OnUnitLoad.IsBound())
        {
            OnUnitLoad.Broadcast(Unit, SavedUnit);
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

    // Restore the GameMode's unit-index allocator high-water mark. ApplyLoadedData re-applies each
    // saved unit's UnitIndex via SetUnitIndex, but the allocator itself was reset by the freshly
    // loaded level's BeginPlay. Without this, a post-load spawn (ARTSGameModeBase increments
    // HighestUnitIndex then assigns it) can hand out an index that collides with a loaded unit,
    // breaking the UnitIndex->registry matching the hybrid-Mass replication link depends on.
    // GetAuthGameMode is null off the authority, so clients simply skip this.
    if (ARTSGameModeBase* RTSGM = LoadedWorld->GetAuthGameMode<ARTSGameModeBase>())
    {
        int32 MaxUnitIndex = RTSGM->HighestUnitIndex;
        for (const FUnitSaveData& SU : SaveData->Units)
        {
            MaxUnitIndex = FMath::Max(MaxUnitIndex, SU.UnitIndex);
        }
        for (TActorIterator<ALevelUnit> ItIdx(LoadedWorld); ItIdx; ++ItIdx)
        {
            if (ALevelUnit* LU = *ItIdx)
            {
                MaxUnitIndex = FMath::Max(MaxUnitIndex, LU->UnitIndex);
            }
        }
        RTSGM->HighestUnitIndex = MaxUnitIndex;
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

bool UGameSaveSubsystem::RestoreUnlocksFromLatestSave()
{
    if (bUnlocksRestored)
    {
        return false;
    }

    UGameInstance* GI = GetGameInstance();
    UMapSwitchSubsystem* MapSub = GI ? GI->GetSubsystem<UMapSwitchSubsystem>() : nullptr;
    if (!MapSub)
    {
        return false;
    }

    // Den juengsten Spielstand suchen - groesster Zeitstempel, nicht weitester Fortschritt.
    //
    // ACHTUNG, daran ist der erste Entwurf gescheitert: im Speicherordner liegen NICHT nur
    // Spielstaende. Gemessen am 16.09.2026 waren es 7860 Dateien, fast alle davon
    // Faehigkeitsdateien je Einheit, dazu Replays von ueber 30 MB. Jede davon anzufassen
    // kostet beim Betreten des Menues spuerbar Zeit - genau daran hing frueher schon die
    // lange Wartezeit des SaveGame-Widgets.
    //
    // Deshalb zuerst nach Aenderungsdatum der DATEI vorsortieren und nur die neuesten
    // MaxKandidaten oeffnen. Das Aenderungsdatum ist nur die Vorauswahl; entschieden wird
    // danach weiter ueber den im Spielstand gespeicherten Zeitstempel. Eine zurueckkopierte
    // Datei mit frischem Dateidatum kann also hoechstens in die Vorauswahl rutschen, nicht
    // faelschlich gewinnen.
    const FString SpeicherOrdner = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    constexpr int32 MaxKandidaten = 50;

    TArray<TPair<FDateTime, FString>> NachDatum;
    for (const FString& Slot : GetAllSaveSlots())
    {
        const FString Pfad = FPaths::Combine(SpeicherOrdner, Slot + TEXT(".sav"));
        NachDatum.Emplace(IFileManager::Get().GetTimeStamp(*Pfad), Slot);
    }
    NachDatum.Sort([](const TPair<FDateTime, FString>& A, const TPair<FDateTime, FString>& B)
    {
        return A.Key > B.Key;
    });

    FString BesterSlot;
    int64 BesteZeit = -1;
    int32 Geprueft = 0;
    for (const TPair<FDateTime, FString>& Eintrag : NachDatum)
    {
        if (Geprueft >= MaxKandidaten)
        {
            break;
        }
        if (!IstSpielstandDatei(Eintrag.Value))
        {
            continue;   // Kostet nur den Dateikopf, nicht die ganze Datei.
        }
        ++Geprueft;

        FString MapAsset, LongName;
        int64 Zeit = 0;
        if (LoadSaveSummary(Eintrag.Value, MapAsset, LongName, Zeit) && Zeit > BesteZeit)
        {
            BesteZeit = Zeit;
            BesterSlot = Eintrag.Value;
        }
    }

    if (BesterSlot.IsEmpty())
    {
        bUnlocksRestored = true;   // Kein Spielstand da - nicht bei jedem Kartenwechsel erneut suchen.
        return false;
    }

    URTSSaveGame* Save = Cast<URTSSaveGame>(UGameplayStatics::LoadGameFromSlot(BesterSlot, 0));
    if (!Save)
    {
        bUnlocksRestored = true;
        return false;
    }

    TMap<FString, TArray<FName>> Freigeschaltet;
    int32 AnzahlTags = 0;
    for (const FMapSwitchTagsForMap& Eintrag : Save->MapEnabledSwitchTags)
    {
        Freigeschaltet.Add(Eintrag.MapKey, Eintrag.Tags);
        AnzahlTags += Eintrag.Tags.Num();
    }
    MapSub->ImportStateFromSave(Freigeschaltet);
    bUnlocksRestored = true;

    UE_LOG(LogTemp, Log,
        TEXT("[Spielstand] Freischaltungen aus '%s' uebernommen (%d Karten, %d Ziele, gespeichert %s). "
             "Karte und Einheiten wurden NICHT geladen."),
        *BesterSlot, Freigeschaltet.Num(), AnzahlTags,
        *FDateTime::FromUnixTimestamp(BesteZeit).ToString());
    UE_LOG(LogTemp, Log, TEXT("[Spielstand] Dafuer %d von %d Dateien geoeffnet."),
        Geprueft, NachDatum.Num());
    return true;
}

FString UGameSaveSubsystem::StartNewGame(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return FString();
    }

    URTSSaveGame* Save = Cast<URTSSaveGame>(UGameplayStatics::CreateSaveGameObject(URTSSaveGame::StaticClass()));
    if (!Save)
    {
        return FString();
    }

    // Absichtlich ein LEERER Spielstand: kein Einheitenbestand, keine Ressourcen, keine
    // Freischaltungen. Nur Karte und Zeitstempel, damit er in der Liste auftaucht und der
    // juengste ist.
    Save->SavedMapLongPackageName = World->GetOutermost()->GetName();
    Save->SavedUnixTimeSeconds = FDateTime::UtcNow().ToUnixTimestamp();
    Save->MapEnabledSwitchTags.Empty();

    const FString Ziel = SlotName.IsEmpty() ? GetUniqueSaveSlotName(TEXT("NewGame")) : SlotName;
    if (!UGameplayStatics::SaveGameToSlot(Save, Ziel, 0))
    {
        return FString();
    }

    // Auch den laufenden Zustand leeren, sonst bleiben die Portale bis zum naechsten
    // Programmstart offen - der Spieler klickt "New Game" und sieht keinen Unterschied.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMapSwitchSubsystem* MapSub = GI->GetSubsystem<UMapSwitchSubsystem>())
        {
            MapSub->ImportStateFromSave(TMap<FString, TArray<FName>>());
        }
    }
    // Nichts mehr nachladen: sonst holte der naechste Kartenwechsel die Freischaltungen aus
    // einem AELTEREN Spielstand zurueck, und das neue Spiel waere sofort wieder durchgespielt.
    bUnlocksRestored = true;

    UE_LOG(LogTemp, Log, TEXT("[Spielstand] Neues Spiel '%s' angelegt. Bisherige Spielstaende bleiben erhalten."),
        *Ziel);
    return Ziel;
}

int32 UGameSaveSubsystem::ResetAllProgress()
{
    const FString Ordner = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    int32 Geloescht = 0;
    for (const FString& Slot : GetAllSaveSlots())
    {
        // Nur was wirklich ein Spielstand ist. Im selben Ordner liegen Replays und
        // Faehigkeitsdateien; die haette ein pauschales Loeschen mitgenommen.
        if (!IstSpielstandDatei(Slot))
        {
            continue;
        }
        if (UGameplayStatics::DeleteGameInSlot(Slot, 0))
        {
            ++Geloescht;
        }
        else if (IFileManager::Get().Delete(*FPaths::Combine(Ordner, Slot + TEXT(".sav"))))
        {
            ++Geloescht;
        }
    }

    // Auch den laufenden Zustand leeren, sonst bleiben die Portale bis zum naechsten
    // Programmstart offen, obwohl die Dateien weg sind.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMapSwitchSubsystem* MapSub = GI->GetSubsystem<UMapSwitchSubsystem>())
        {
            MapSub->ImportStateFromSave(TMap<FString, TArray<FName>>());
        }
    }
    PendingLoadedSave = nullptr;
    PendingSlotName.Reset();
    bPendingQuickSave = false;
    // Nach dem Zuruecksetzen darf nichts mehr nachgeladen werden - sonst holte der naechste
    // Kartenwechsel die eben geloeschten Freischaltungen aus einem Spielstand zurueck, den es
    // nicht mehr gibt.
    bUnlocksRestored = true;

    UE_LOG(LogTemp, Warning, TEXT("[Spielstand] %d Spielstaende geloescht, Freischaltungen zurueckgesetzt."),
        Geloescht);
    return Geloescht;
}

bool UGameSaveSubsystem::IstSpielstandDatei(const FString& SlotName) const
{
    // Nur in den KOPF der Datei sehen, statt sie ganz zu laden.
    //
    // Der Ordner Saved/SaveGames enthaelt weit mehr als Spielstaende: Faehigkeiten je Einheit,
    // Replays, Indexdateien. Gemessen am 10.09.2026 lagen dort 7884 Dateien mit zusammen 267 MB,
    // darunter Replays von ueber 30 MB. LoadSaveSummary lud jede einzelne davon vollstaendig,
    // nur um danach festzustellen, dass es kein URTSSaveGame ist - daher brauchte das
    // SaveGame-Widget so lange zum Oeffnen.
    //
    // Unreal schreibt den Klassennamen des Spielstands weit vorn in die Datei. Ihn dort zu suchen
    // kostet ein paar Kilobyte statt Dutzender Megabyte. Findet sich der Kopf nicht wie erwartet,
    // wird NICHT ausgeschlossen - dann entscheidet wie bisher das vollstaendige Laden, damit ein
    // geaendertes Dateiformat keine Spielstaende verschwinden laesst.
    const FString Pfad = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"),
        SlotName + TEXT(".sav"));

    TUniquePtr<FArchive> Datei(IFileManager::Get().CreateFileReader(*Pfad));
    if (!Datei)
    {
        return true; // Nicht lesbar - die alte Entscheidung uebernehmen.
    }

    const int64 Groesse = Datei->TotalSize();
    const int64 Menge = FMath::Min<int64>(Groesse, 4096);
    if (Menge <= 0)
    {
        return false;
    }

    TArray<uint8> Kopf;
    Kopf.SetNumUninitialized(static_cast<int32>(Menge));
    Datei->Serialize(Kopf.GetData(), Menge);
    Datei->Close();

    // Der Klassenname steht als schlichter Text im Kopf. Beide Schreibweisen pruefen, weil Unreal
    // FString je nach Inhalt einbytig oder zweibytig ablegt.
    auto Enthaelt = [&Kopf](const ANSICHAR* Suche) -> bool
    {
        const int32 Laenge = FCStringAnsi::Strlen(Suche);
        if (Laenge <= 0 || Kopf.Num() < Laenge)
        {
            return false;
        }

        // Einbytig.
        for (int32 i = 0; i + Laenge <= Kopf.Num(); ++i)
        {
            if (FMemory::Memcmp(Kopf.GetData() + i, Suche, Laenge) == 0)
            {
                return true;
            }
        }

        // Zweibytig: jedes Zeichen gefolgt von einer Null.
        for (int32 i = 0; i + Laenge * 2 <= Kopf.Num(); ++i)
        {
            bool bPasst = true;
            for (int32 j = 0; j < Laenge; ++j)
            {
                if (Kopf[i + j * 2] != static_cast<uint8>(Suche[j]) || Kopf[i + j * 2 + 1] != 0)
                {
                    bPasst = false;
                    break;
                }
            }
            if (bPasst)
            {
                return true;
            }
        }

        return false;
    };

    if (Enthaelt("RTSSaveGame"))
    {
        return true;
    }

    // Eindeutig etwas anderes? Dann sicher aussortieren.
    if (Enthaelt("ReplaySaveGame") || Enthaelt("ReplayIndexSaveGame") || Enthaelt("TalentSaveGame"))
    {
        return false;
    }

    // Unbekannt: nicht raten, sondern wie bisher vollstaendig laden.
    return true;
}

bool UGameSaveSubsystem::LoadSaveSummary(const FString& SlotName, FString& OutMapAssetName, FString& OutLongPackageName, int64& OutUnixTime) const
{
    OutMapAssetName.Reset();
    OutLongPackageName.Reset();
    OutUnixTime = 0;

    if (!IstSpielstandDatei(SlotName))
    {
        return false;
    }

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
