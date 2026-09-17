// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityTypes.h"
#include "Core/UnitData.h"
#include "Engine/DataTable.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UnitAnimationProcessor.generated.h"

USTRUCT(BlueprintType)
struct FISMAnimationData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    TEnumAsByte<UnitData::EState> AnimState = UnitData::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float StateCustomDataValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float TransitionRate = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float StartFrame = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float EndFrame = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float PlayRate = 1.0f;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FUnitAnimationFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    float TargetBlendPoint_1 = 0.f;
    
    UPROPERTY(Transient)
    float TargetBlendPoint_2 = 0.f;
    
    UPROPERTY(Transient)
    float CurrentBlendPoint_1 = 0.f;
    
    UPROPERTY(Transient)
    float CurrentBlendPoint_2 = 0.f;
    
    UPROPERTY(Transient)
    float TransitionRate_1 = 0.5f;
    
    UPROPERTY(Transient)
    float TransitionRate_2 = 0.5f;
    
    UPROPERTY(Transient)
    float Resolution_1 = 0.f;
    
    UPROPERTY(Transient)
    float Resolution_2 = 0.f;

    UPROPERTY(Transient)
    class USoundBase* Sound = nullptr;

    UPROPERTY(Transient)
    TEnumAsByte<UnitData::EState> LastProcessedState = UnitData::None;

    /**
     * Wohin die Instanzdaten zuletzt geschrieben wurden.
     *
     * Eine Einheit bekommt ihre Instanz zuerst auf der EIGENEN ISMComponent und wird danach vom
     * UUnitVisualManager auf eine gepoolte ISM umgezogen. Der Erstschreibvorgang landet dann auf der
     * alten Komponente, auf der neuen stehen weiter Nullen - also Frames 0..0 und damit ein Standbild,
     * bis zufaellig ein Zustandswechsel neu schreibt. Deshalb merken wir uns das Ziel und schreiben
     * neu, sobald es sich geaendert hat.
     */
    UPROPERTY(Transient)
    TWeakObjectPtr<UInstancedStaticMeshComponent> LastWrittenISM = nullptr;

    UPROPERTY(Transient)
    int32 LastWrittenInstanceIndex = INDEX_NONE;

	UPROPERTY(Transient)
	float PlayRate = 1.0f;

	UPROPERTY(Transient)
	float AnimationPosition = 0.0f;

    UPROPERTY(Transient)
    float TargetStateCustomDataValue = 0.0f;

    UPROPERTY(Transient)
    float PrevTargetStateCustomDataValue = 0.0f;

    UPROPERTY(Transient)
    float PrevStartTime = 0.0f;

    UPROPERTY(Transient)
    float PrevStartFrame = 0.0f;

    UPROPERTY(Transient)
    float PrevEndFrame = 0.0f;

    UPROPERTY(Transient)
    float CurrentStartTime = 0.0f;

    UPROPERTY(Transient)
    float CurrentStartFrame = 0.0f;

    UPROPERTY(Transient)
    float CurrentEndFrame = 0.0f;

    UPROPERTY(Transient)
    float BlendAlpha = 1.0f;

    UPROPERTY(Transient)
    float CurrentPlayRate = 1.0f;

    UPROPERTY(Transient)
    float PrevPlayRate = 1.0f;

    UPROPERTY(Transient)
    class UDataTable* ISMAnimationDataTable = nullptr;
};

/**
 * Ist der Aktor in der Auswahl des oertlichen Spielers?
 *
 * Nur fuer die Diagnose. Am Wegpunkt draengen sich zwanzig Einheiten; welche davon der Nutzer
 * laufen SIEHT, geht aus dem Namen im Log nicht hervor. Er waehlt sie im Spiel aus, und nur
 * sie schreibt dann.
 */
RTSUNITTEMPLATE_API bool RTSDiagIstAusgewaehlt(const AActor* Aktor);

UCLASS()
class RTSUNITTEMPLATE_API UUnitAnimationProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UUnitAnimationProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StateCustomDataIndex = 1;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 TransitionRateCustomDataIndex = 2;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StartTimeCustomDataIndex = 3;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StartFrameCustomDataIndex = 4;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 EndFrameCustomDataIndex = 5;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStateCustomDataIndex = 6;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStartTimeCustomDataIndex = 7;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStartFrameCustomDataIndex = 8;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevEndFrameCustomDataIndex = 9;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 BlendAlphaCustomDataIndex = 10;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PlayRateCustomDataIndex = 11;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevPlayRateCustomDataIndex = 12;

    FMassEntityQuery EntityQuery;

    /**
     * Diagnostic only: is a unit playing a movement animation while it does not move?
     *
     * Measures the ACTUAL displacement per entity, not the velocity fragment - the fragment holds
     * what the movement wants, which is exactly the value in doubt here.
     */
    struct FAnimStandWatch
    {
        FVector  LastLocation = FVector::ZeroVector;
        float    SecondsStanding = 0.f;
        bool     bHasLocation = false;

        // NETTO-FENSTER: wo die Einheit zu Beginn des laufenden Fensters stand.
        //
        // Das Mass pro Einzelbild taugt am Wegpunkt nicht. Gemessen am 16.09.2026 an einem
        // ausgewaehlten Vector auf dem Client, ueber achtzig aufeinanderfolgende Bilder:
        //   Verschiebung=241  Fragment=257  massgeblich=241   (x8)
        //   Verschiebung=1710 Fragment=245  massgeblich=245   <- Ruecksprung
        // Acht Bilder lang laeuft die Einheit mit ~250 uu/s vorwaerts (rund 33 uu), dann wirft
        // der naechste Serverstand sie in EINEM Bild wieder zurueck. Netto bleibt sie stehen -
        // aber jedes Einzelbild meldet volle Laufgeschwindigkeit, und beide Masse (Verschiebung
        // UND Geschwindigkeitsfragment) melden sie gleichzeitig. Das Minimum aus beiden half
        // deshalb nicht.
        //
        // Nur der Abstand ueber ein ZEITFENSTER trennt "laeuft" von "zappelt auf der Stelle".
        FVector  FensterStart = FVector::ZeroVector;
        float    FensterZeit = 0.f;
        float    FensterNettoTempo = 0.f;
        // Summe der GEWOLLTEN Geschwindigkeit ueber das Fenster, um daraus den Mittelwert zu
        // bilden. Der Vergleich Netto gegen Gewollt ist das eigentliche Mass - siehe
        // AnimStandFortschrittsAnteil.
        float    FensterWunschSumme = 0.f;
        float    FensterWunschTempo = 0.f;
        bool     bFensterStill = false;
    };
    TMap<FMassEntityHandle, FAnimStandWatch> AnimStandWatches;




    /**
     * Show a unit that does not move as standing, even while its state says it is walking.
     *
     * Applies to movement states only - standing is the correct picture for Attack, Pause, Build,
     * ResourceExtraction and Casting, and those are never touched. Only the animation row is
     * swapped; the unit's own state stays exactly as it was.
     */
    UPROPERTY(EditAnywhere, Category = "Animations")
    bool bAnimStandFix = true;


    /** Below this measured ground speed a unit counts as standing (uu/s). */
    UPROPERTY(EditAnywhere, Category = "Mass|Diagnostics")
    float AnimStandSpeedThreshold = 5.f;

    /** How long it has to stand before it counts - a single blocked frame is not the problem. */
    UPROPERTY(EditAnywhere, Category = "Mass|Diagnostics")
    float AnimStandMinSeconds = 0.5f;

    /**
     * Ab dieser Strecke vom Fensteranfang gilt die Einheit sofort wieder als laufend, ohne das
     * Fenster abzuwarten.
     *
     * Sonst haengt eine Einheit, die gerade wirklich losgelaufen ist, bis zu AnimStandMinSeconds
     * in der Stillstandspose fest. Der Wert muss ueber der gemessenen Zappelweite liegen: der
     * Vector kam am 16.09.2026 nie weiter als etwa 33 uu vom Fleck, bevor der Ruecksprung kam.
     * 60 uu liegt darueber und ist fuer einen echten Laeufer (rund 250 uu/s) nach 0,24 s erreicht.
     */
    float AnimStandAusbruchStrecke = 60.f;


    /**
     * Anteil der GEWOLLTEN Geschwindigkeit, den eine Einheit ueber das Fenster tatsaechlich
     * zuruecklegen muss, um als laufend zu gelten.
     *
     * Ein fester Schwellwert in uu/s kann den Fehler nicht fangen. Gemessen am 16.09.2026 an
     * einem ausgewaehlten Vector auf dem Client, drei Fenster hintereinander:
     *   netto=13.87   netto=36.58   netto=21.40      bei gewollt rund 300 uu/s
     * Die Einheit steht also NICHT still - sie kriecht mit 5 bis 12 Prozent ihrer Laufgeschwin-
     * digkeit vorwaerts, waehrend die Animation den vollen Lauf zeigt. Absolut betrachtet liegen
     * 37 uu/s ueber jeder sinnvollen Stillstandsschwelle; im Verhaeltnis zu den gewollten 300 sind
     * sie es nicht.
     *
     * Ein echter Laeufer erreicht ueber ein halbes Sekundenfenster nahezu 100 Prozent, selbst in
     * einer Kurve deutlich ueber die Haelfte. 25 Prozent liegt weit von beiden Seiten entfernt.
     */
    float AnimStandFortschrittsAnteil = 0.25f;
};
