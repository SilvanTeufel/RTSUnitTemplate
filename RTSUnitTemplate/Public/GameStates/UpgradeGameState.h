// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameStates/ResourceGameState.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Containers/Ticker.h"
#include "Characters/Unit/LevelUnit.h"  // FAttributeTreeNodeState
#include "UpgradeGameState.generated.h"

USTRUCT(BlueprintType)
struct FUpgradeStatus : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    bool Researched = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
    TSubclassOf<UGameplayEffect> InvestmentEffect;
};

USTRUCT(BlueprintType)
struct FTeamUpgrades : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrades")
    TArray<FUpgradeStatus> Upgrades;

    /** Marks an individual item as dirty for replication */
    void MarkItemDirty(FUpgradeStatus& Item) { FFastArraySerializer::MarkItemDirty(Item); }

    /** Marks the entire array as dirty for replication */
    void MarkArrayDirty() { FFastArraySerializer::MarkArrayDirty(); }
};

/**
 * Punktevorrat und investierte Stufen EINES Teams.
 *
 * Bis zum 20.09.2026 lag beides je Einheit (ALevelUnit::AttributeTreePoints und
 * ::AttributeTreeNodes). Das trug nicht: die Anzeige las den Topf der einen Einheit, das
 * Investieren buchte auf einer anderen, und jede neu gespawnte Einheit fing bei null an.
 * Der Vorrat gehoert deshalb ans Team - die Wirkung an den Tag des Knotens.
 *
 * Liegt bewusst im GameState und nicht in einem Subsystem: Subsysteme replizieren nicht
 * (das war die Ursache des Verbuendeten-Healthbar-Flackerns), das Widget zeichnet aber auf
 * dem Client.
 */
USTRUCT(BlueprintType)
struct FTeamAttributeTree
{
    GENERATED_BODY()

    /** Freie Punkte des Teams. */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute Tree")
    int32 AvailablePoints = 0;

    /** Bereits ausgegebene Punkte - Grundlage der Rueckerstattung beim Zuruecksetzen. */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute Tree")
    int32 UsedPoints = 0;

    /** Investierte Stufen je Knoten. Nur Knoten mit mehr als 0 Punkten stehen drin. */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute Tree")
    TArray<FAttributeTreeNodeState> Nodes;
};

class UAutoAbility;

/** An automatic ability a team has switched on for one unit type - by card, in the attribute tree, or both. */
USTRUCT(BlueprintType)
struct FTeamAutoAbilityGrant
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    int32 TeamId = 0;

    /** Units.Tier.N - every unit carrying it gets the ability. */
    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    FGameplayTag UnitTag;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    TSubclassOf<UAutoAbility> Ability;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    bool bFromCard = false;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    bool bFromTree = false;
};

/** The ability drawn for an auto ability node of a team's attribute tree (FAttributeTreeNodeRow::bAutoAbilitySlot). */
USTRUCT(BlueprintType)
struct FTreeAutoAbilityPick
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    int32 TeamId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    FName NodeId;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    FGameplayTag UnitTag;

    UPROPERTY(BlueprintReadOnly, Category = "Auto Abilities")
    TSubclassOf<UAutoAbility> Ability;
};

UCLASS()
class RTSUNITTEMPLATE_API AUpgradeGameState : public AResourceGameState
{
    GENERATED_BODY()

public:
    // ----------------------------------------------------------------------
    //  Automatische Faehigkeiten je Team (14.11.2026). Karten (UStartLoadoutComponent::
    //  AutoAbilityCardSets) und Auto-Knoten des Attributbaums schalten eine UAutoAbility fuer
    //  einen Einheitentyp frei; UAutoAbilitySubsystem verteilt sie an die Einheiten. Repliziert,
    //  weil Baum-Tooltips und Karten auf dem Client zeigen, was gezogen wurde.
    // ----------------------------------------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_AutoAbilities, VisibleAnywhere, BlueprintReadOnly, Category = "Auto Abilities")
    TArray<FTeamAutoAbilityGrant> TeamAutoAbilityGrants;

    UPROPERTY(ReplicatedUsing = OnRep_AutoAbilities, VisibleAnywhere, BlueprintReadOnly, Category = "Auto Abilities")
    TArray<FTreeAutoAbilityPick> TreeAutoAbilityPicks;

    UFUNCTION()
    void OnRep_AutoAbilities();

    /** Grants or picks changed (server and clients) - the tree widget redraws its auto ability nodes. */
    FSimpleMulticastDelegate OnAutoAbilitiesChanged;

    /** Server: counts every change - the subsystem re-syncs the units when it moves. */
    int32 AutoAbilityVersion = 0;

    /** Server: switches the ability on for every unit of the team with UnitTag (bFromCard = card, else tree). */
    UFUNCTION(BlueprintCallable, Category = "Auto Abilities")
    void GrantTeamAutoAbility(int32 TeamId, FGameplayTag UnitTag, TSubclassOf<UAutoAbility> Ability, bool bFromCard);

    /** Server: the tree's share goes (tree reset); card grants stay. */
    void RemoveTreeAutoAbilities(int32 TeamId);

    /** 0.5 when the team has the ability by card AND from the tree (any unit type), else 1. */
    UFUNCTION(BlueprintPure, Category = "Auto Abilities")
    float GetAutoAbilityCooldownMultiplier(int32 TeamId, TSubclassOf<UAutoAbility> Ability) const;

    /** Does the team have the ability by card (bCard) / from the tree? */
    UFUNCTION(BlueprintPure, Category = "Auto Abilities")
    bool HasAutoAbilityFrom(int32 TeamId, TSubclassOf<UAutoAbility> Ability, bool bCard) const;

    const FTreeAutoAbilityPick* FindTreeAutoAbilityPick(int32 TeamId, FName NodeId) const;

    /** Server: draws the abilities of the team's auto ability tree nodes (once per team). */
    void EnsureTreeAutoAbilityPicks(int32 TeamId);

    /** Server: the team invested in an auto ability node - grants its drawn ability. */
    bool ApplyTreeAutoAbilityNode(int32 TeamId, FName NodeId);

    AUpgradeGameState();

    // Array of team upgrades
    UPROPERTY(ReplicatedUsing = OnRep_TeamUpgrades, VisibleAnywhere, BlueprintReadOnly, Category = "Upgrades")
    TArray<FTeamUpgrades> TeamUpgradesArray;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_TeamUpgrades();

    // Main setter
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void SetTeamUpgrades(int32 TeamId, const TArray<FUpgradeStatus>& Upgrades);

    // Individual upgrade management
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void SetUpgradeResearched(int32 TeamId, int32 UpgradeIndex, bool bResearched);

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    TSubclassOf<UGameplayEffect> GetUpgradeInvestmentEffect(int32 TeamId, const FString& UpgradeName) const;
    
    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void AddUpgradeToTeam(int32 TeamId, const FUpgradeStatus& Upgrade);

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    bool GetUpgradeResearchedStatus(int32 TeamId, FString UpgradeName) const;

    UFUNCTION(BlueprintCallable, Category = "Upgrades")
    void ResearchUpgradeByName(int32 TeamId, FString UpgradeName);

    // ----------------------------------------------------------------------
    //  Attributbaum je Team
    //
    //  Indiziert wie TeamUpgradesArray: TeamIndex = TeamId - 1, TeamId beginnt bei 1.
    //  Alle Schreibzugriffe nur auf dem Server; die Replikation traegt das Ergebnis zum
    //  Client, wo das Widget zeichnet.
    // ----------------------------------------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_TeamAttributeTrees, VisibleAnywhere, BlueprintReadOnly, Category = "Attribute Tree")
    TArray<FTeamAttributeTree> TeamAttributeTrees;

    UFUNCTION()
    void OnRep_TeamAttributeTrees();

    /** Legt Punkte in den Topf des Teams. Nur Server. */
    UFUNCTION(BlueprintCallable, Category = "Attribute Tree")
    void GrantTeamAttributeTreePoints(int32 TeamId, int32 Count);

    /** Freie Punkte des Teams. 0, wenn das Team noch keinen Eintrag hat. */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    int32 GetTeamAttributeTreePoints(int32 TeamId) const;

    /** Bereits ausgegebene Punkte des Teams. */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    int32 GetTeamUsedAttributeTreePoints(int32 TeamId) const;

    /** In diesen Knoten investierte Stufen des Teams (0, wenn nie investiert). */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    int32 GetTeamAttributeTreeNodePoints(int32 TeamId, FName NodeId) const;

    /** Alle investierten Knoten des Teams - Grundlage der Nachvergabe an neu gespawnte Einheiten. */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    TArray<FAttributeTreeNodeState> GetTeamAttributeTreeNodes(int32 TeamId) const;

    /**
     * Bucht EINEN Punkt auf den Knoten und zieht ihn vom Vorrat ab. Nur Server.
     *
     * Prueft NUR die Buchhaltung (Vorrat vorhanden, MaxPoints nicht ueberschritten). Ob der
     * Knoten freigeschaltet ist, entscheidet der Aufrufer - nur er kennt die DataTable.
     *
     * @return true, wenn tatsaechlich gebucht wurde.
     */
    UFUNCTION(BlueprintCallable, Category = "Attribute Tree")
    bool InvestTeamAttributeTreeNode(int32 TeamId, FName NodeId, int32 MaxPoints);

    /** Erstattet alle ausgegebenen Punkte und leert die Knoten des Teams. Nur Server. */
    UFUNCTION(BlueprintCallable, Category = "Attribute Tree")
    void ResetTeamAttributeTree(int32 TeamId);

    /**
     * Die Baumtabelle des Teams - die erste Einheit des Teams, die eine traegt.
     *
     * Der Baum ist fuer alle Einheiten eines Teams dieselbe Tabelle; welche Einheit sie liefert,
     * ist deshalb gleichgueltig.
     */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    const UDataTable* FindTeamAttributeTreeTable(int32 TeamId) const;

    /**
     * Ist der Knoten im Baum DES TEAMS freigeschaltet?
     *
     * Bewusst am Teamstand geprueft und nicht an einer Einheit: seit der Umstellung auf den
     * gemeinsamen Topf gehoert die Vorbedingung dem Team. Eine Einheit, die spaeter dazukommt,
     * hat einen leeren eigenen Baum und wuerde jede Pruefung an sich selbst nicht bestehen.
     */
    UFUNCTION(BlueprintPure, Category = "Attribute Tree")
    bool IsTeamAttributeTreeNodeUnlocked(int32 TeamId, FName NodeId) const;

    // ----------------------------------------------------------------------
    //  Talentpause (01.11.2026): bekommt ein Team einen neuen Punkt, haelt das Spiel an, bis
    //  jedes wartende Team gewaehlt hat oder die Zeit abgelaufen ist. Nur Server. Die Clients
    //  erfahren es ueber ACameraControllerBase::Client_BeginTalentPause / Client_EndTalentPause -
    //  RPCs statt Replikation, weil in der Pause die Weltzeit steht und Properties darum nicht
    //  zuverlaessig nachkommen.
    // ----------------------------------------------------------------------

    /**
     * Haelt das Spiel an (oder verlaengert die Liste der wartenden Teams einer laufenden Pause)
     * und laesst beim Team des Anfragenden den Attributbaum aufgehen. Eine Pause, die schon aus
     * dem Menue kommt, bleibt unberuehrt - dann passiert nichts.
     */
    void RequestTalentPause(class ACameraControllerBase* Requester, float Seconds);

    /**
     * Das Team hat einen Punkt investiert. Fertig ist es erst, wenn es keine Punkte mehr hat (oder
     * nichts mehr investierbar ist) - ist es dann das letzte wartende, geht es weiter.
     */
    void NotifyTalentPauseChoice(int32 TeamId);

    /** Beendet die Talentpause sofort (Zeit abgelaufen oder alle haben gewaehlt). */
    void EndTalentPause(const TCHAR* Reason);

    bool IsTalentPauseActive() const { return bTalentPauseActive; }

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * So lange (Echtzeit) laeuft das Spiel nach der Ankuendigung noch weiter, bevor es steht.
     * Die Clients stellen die Musik in der Zeit auf UI-Sound um: haelt die Engine zuerst an,
     * pausiert sie die Musik fest bis zum Ende der Pause - und deren Spielzeit laeuft als UI-Sound
     * trotzdem weiter, so dass am Liedende das naechste Lied kam (gemeldet 02.11.2026).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree")
    float TalentPauseAudioLeadSeconds = 0.3f;

private:
    bool bTalentPauseActive = false;

    /** Steht das Spiel schon (SetPause erfolgt)? Davor laeuft nur die Vorlaufzeit. */
    bool bTalentPauseFrozen = false;

    /** Echtzeit, zu der das Spiel angehalten wird. */
    double TalentPauseFreezeTime = 0.0;

    /** Wer die Pause angestossen hat (fuer SetPause). */
    TWeakObjectPtr<class ACameraControllerBase> TalentPauseRequester;

    /** Teams whose tree auto abilities were drawn. */
    TSet<int32> TreePicksRolled;

    void AutoAbilitiesChanged();

    /** Hat das Team noch Punkte und etwas, worin es sie stecken kann? */
    bool TeamCanStillInvest(int32 TeamId) const;

    /** Ende der Pause in Echtzeit (FPlatformTime) - die Weltzeit steht ja. */
    double TalentPauseEndTime = 0.0;

    /** Teams, die noch waehlen sollen. */
    TSet<int32> TalentPauseWaitingTeams;

    /** Laeuft auch in der Pause (anders als der TimerManager der Welt). */
    FTSTicker::FDelegateHandle TalentPauseTicker;

    bool TickTalentPause(float DeltaTime);

    /** CanUnpause der Pause: so hebt das Pausemenue die Talentpause nicht vorzeitig auf. */
    bool CanUnpauseTalentPause();

    /** Sorgt dafuer, dass es den Eintrag gibt, und gibt ihn zurueck. Nur Server. */
    FTeamAttributeTree* FindOrAddTeamAttributeTree(int32 TeamId);

    /** Nur-Lese-Zugriff ohne Anlegen; nullptr, wenn das Team keinen Eintrag hat. */
    const FTeamAttributeTree* FindTeamAttributeTree(int32 TeamId) const;
};
