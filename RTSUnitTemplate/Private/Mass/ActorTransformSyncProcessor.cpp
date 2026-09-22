// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/ActorTransformSyncProcessor.h"
#include "Mass/UnitMassTag.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "Characters/Unit/UnitBase.h"
#include "Actors/WorkArea.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Volume.h"
#include "Async/Async.h"
#include "NavigationSystem.h"
#include "LandscapeProxy.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CsvProfiler.h"

// IDEA 1 — CLIENT render smoothing. The unit ACTOR's position is hard-copied from the authoritative Mass
// FTransformFragment every frame (no smoothing) -> any residual high-frequency jitter in the fragment (10Hz
// reconcile beat, correction yanks, avoidance leftovers) shows on screen 1:1. Here we render the actor at a
// position that EASES toward the authoritative one (bounded max-lag), filtering the jitter at the OUTPUT while
// the Mass fragment (gameplay/avoidance/selection-source) stays exact. Client-only; units only (effect areas
// are excluded so their actor->fragment sync isn't fed a smoothed position). Source-agnostic catch-all.
static TAutoConsoleVariable<int32> CVarRTS_ClientRenderSmoothing(
	TEXT("net.RTS.Client.RenderSmoothing"), 1,
	TEXT("Client-only: smooth the rendered unit actor position toward the authoritative Mass transform (filters residual jitter at the visual output). 1=on, 0=off."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ClientRenderSmoothingSpeed(
	TEXT("net.RTS.Client.RenderSmoothingSpeed"), 14.0f,
	TEXT("VInterpTo speed for client render smoothing. Higher = tighter/less lag (less smoothing); lower = smoother but more visual lag."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ClientRenderSmoothingMaxLag(
	TEXT("net.RTS.Client.RenderSmoothingMaxLag"), 60.0f,
	TEXT("Max horizontal distance (cm) the rendered actor may lag behind the authoritative position before being clamped (so big legit moves still follow promptly)."),
	ECVF_Default);

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    // Standalone MUST be included: in NM_Standalone this processor is otherwise dropped entirely,
    // and it is the only writer of PositionedTransform/bTransformDirty for non-yaw-follow units —
    // ISM units would then never rotate (or work-face) in a packaged standalone game.
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    // Optional ExecutionOrder settings...
}

static TAutoConsoleVariable<int32> CVarRTS_SkmDiag(
	TEXT("RTS.SkmDiag"),
	0,
	TEXT("1 = einmal je Sekunde eine Zeile fuer die ERSTE skelettale Einheit ausgeben. 0 = aus.")
	TEXT("")
	TEXT("WOFUER: gemeldet wurde 'Einheiten mit bUseSkeletalMovement bewegen sich nicht mehr'. ")
	TEXT("Dafuer gibt es genau zwei Erklaerungen, und sie liegen weit auseinander:")
	TEXT("  (a) die MASS-ENTITAET bewegt sich nicht - dann liegt es an den Bewegungsprozessoren")
	TEXT("  (b) die Mass-Entitaet bewegt sich, aber der AKTOR wird nicht nachgezogen - dann liegt ")
	TEXT("es an dieser Uebertragung hier.")
	TEXT("")
	TEXT("Die Zeile zeigt beides nebeneinander: Mass-Ziel, vorherige Lage, Aktorlage und ob der ")
	TEXT("Uebertrag eingereiht wurde. Raten kostet mehr als messen."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRTS_ActorSyncStartUnits(
	TEXT("RTS.ActorSync.StartUnits"),
	-1,
	TEXT("Ab wievielen Einheiten die Aktor-Taktung einsetzt. -1 = Wert aus dem Prozessor ")
	TEXT("(ActorSyncScaleStartUnits, Vorgabe 300). Darunter wird jedes Bild uebertragen."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRTS_ActorSyncFullUnits(
	TEXT("RTS.ActorSync.FullUnits"),
	-1,
	TEXT("Ab wievielen Einheiten die volle Taktung (ActorSyncMaxInterval) gilt. -1 = Wert aus ")
	TEXT("dem Prozessor (ActorSyncScaleFullUnits, Vorgabe 500)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarRTS_ActorSyncInterval(
	TEXT("RTS.ActorSync.Interval"),
	-1.0f,
	TEXT("Ueberschreibt VisualISMActorSyncTime zur Laufzeit. Negativ = Wert aus dem Prozessor. ")
	TEXT("WOFUER: die Drossel steht im Header auf 0.f, damit ist bNeedsActorSync IMMER wahr und ")
	TEXT("DispatchPendingUpdates ruft je Bild fuer ~509 von 510 Einheiten Actor->SetActorTransform ")
	TEXT("auf. Gemessen am 18.09.2026: 2,38 ms von 3,93 ms des Prozessors - 61 %. ISM-Einheiten ")
	TEXT("werden aber ueber die ISM-Instanz gezeichnet, nicht ueber den Aktor; die Aktortransformation ")
	TEXT("braucht nur, wer sie liest (HUD-Indikatoren, Faehigkeiten, Auswahl). Mit diesem Schalter ")
	TEXT("laesst sich der Gewinn messen, BEVOR das Verhalten dauerhaft geaendert wird."),
	ECVF_Default);

// ===================================================================================================
// VERWORFEN: kurzer Bodentrace um die zuletzt gefundene Bodenhoehe  (18.09.2026)
//
// IDEE: HandleGroundAndHeight zieht je Einheit und Bild eine Spur ueber 3000 uu (1000 darueber bis
// 2000 darunter). Gemessen 0,556 ms bei 510 Einheiten. Die Annahme war, dass ein LineTrace
// proportional zu den durchquerten Broadphase-Zellen kostet - eine Spur von nur +/-200 uu um
// CharFragment.LastGroundLocation waere ein Fuenfzehntel so lang und muesste rund 85 % sparen.
// Der Rueckfall auf die lange Spur deckte Spruenge, Klippen und Teleports ab.
//
// GEMESSEN: die Abkuerzung griff bei 99,1 % aller Aufrufe (Rueckfall 0,9 %, mittlerer Abstand zum
// Anker 241 uu bei Spanne 200) - und die Bodenzeit blieb bei 0,556 ms. Differenz 0,000 ms.
//
// WAS DARAUS FOLGT: die Kosten eines LineTrace stecken NICHT in der gelaufenen Strecke, sondern im
// Aufsetzen der Abfrage - Query-Parameter, Eintritt in die Physikszene, Ergebnisaufbereitung. Eine
// Spur zu VERKUERZEN bringt daher nichts. Wer diesen Posten senken will, muss Traces WEGLASSEN
// (etwa fuer Einheiten, die sich seit dem letzten Bild in XY nicht bewegt haben) oder sie
// zusammenfassen - nicht sie kuerzen.
//
// NICHT NOCH EINMAL PROBIEREN. Die Messung ist eindeutig: 99,1 % Trefferquote bei 0,000 ms Gewinn.
//
// Nebenbefund aus derselben Messung: der erste Anlauf scheiterte an einem eigenen Waechter
// "LastGroundLocation != 0" als Gueltigkeitspruefung. LevelSix hat ueber weite Teile Boden bei
// Z = 0, ein GUELTIGER Anker von 0.0 war damit von "nie gesetzt" nicht zu unterscheiden -
// Trefferquote 1,8 %. Null ist auf dieser Karte ein gueltiger Wert, kein Sentinel.
// ===================================================================================================

static TAutoConsoleVariable<int32> CVarRTS_MeasureTagCost(
	TEXT("RTS.ChunkTags.MessAlt"),
	0,
	TEXT("Misst, was die frueher hier stehende Abfrage JE EINHEIT gekostet haette, und meldet es ")
	TEXT("alle 5 s. Fuehrt die zehn DoesEntityHaveTag zusaetzlich aus und verwirft das Ergebnis - ")
	TEXT("die gemeldete Zeit ist also genau die Arbeit, welche die Chunk-Umstellung eingespart hat. ")
	TEXT("Sauberer als zwei getrennte Laeufe zu vergleichen: dieselbe Szene, dieselbe Kamera, ")
	TEXT("dasselbe Bild."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRTS_VerifyChunkTags(
	TEXT("RTS.ChunkTags.Verify"),
	0,
	TEXT("Prueft die Chunk-weite Tag-Abkuerzung gegen die Abfrage je Entitaet. VORGABE AUS, weil ")
	TEXT("die Pruefung genau das tut, was die Abkuerzung einspart. EINSCHALTEN in einer Partie mit ")
	TEXT("GEMISCHTEN Zustaenden - kaempfende, bauende, sammelnde, tote Einheiten nebeneinander. Der ")
	TEXT("automatische Messfall taugt dafuer NICHT: dort marschieren alle Einheiten gleich, und die ")
	TEXT("Pruefung sah nur 2 verschiedene Tag-Kombinationen - 0 Deviations belegen dann wenig. Die ")
	TEXT("Logzeile nennt die Zahl der Kombinationen mit; erst bei vielen ist die Annahme geprueft."),
	ECVF_Default);

void UActorTransformSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
        EntityQuery.Initialize(EntityManager);
        EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
        EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
        EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
        EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
        EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    
        EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
        EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
        EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
        //EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::Any); // Added to any group
        
        EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

        EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
        EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
        EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::Any);
    
        EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
        EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
        EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
        EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
        // LUX-ANPASSUNG (16.08.2026): sperrt auf FMassStopWhileAimingTag statt auf
        // FMassRotateToMouseTag, damit die direkt gesteuerte CameraUnit beim Zielen laufen
        // darf. Alle anderen Einheiten tragen beide Tags -> Verhalten unveraendert.
        // Original: AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
        EntityQuery.AddTagRequirement<FMassStopWhileAimingTag>(EMassFragmentPresence::None);
        EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);

		EntityQuery.RegisterWithProcessor(*this);

		ClientEntityQuery.Initialize(EntityManager);
		ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

        ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
        ClientEntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
        //ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::Any); // Added to any group
            
        ClientEntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

        ClientEntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
    
        ClientEntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
        ClientEntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::Any);
    
        ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
        ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None); 
        ClientEntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None); 
        ClientEntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
        // LUX-ANPASSUNG (16.08.2026): sperrt auf FMassStopWhileAimingTag statt auf
        // FMassRotateToMouseTag, damit die direkt gesteuerte CameraUnit beim Zielen laufen
        // darf. Alle anderen Einheiten tragen beide Tags -> Verhalten unveraendert.
        // Original: AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
        ClientEntityQuery.AddTagRequirement<FMassStopWhileAimingTag>(EMassFragmentPresence::None);
        ClientEntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
    /*
		ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		ClientEntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	*/
	ClientEntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    ClientEntityQuery.RegisterWithProcessor(*this);
}

/**
 * @brief Checks if the processor should execute its main logic based on a dynamic tick rate.
 * @param FrameDeltaTime The delta time for the current frame.
 * @param OutAccumulatedDeltaTime The accumulated delta time since last execution, to be used for interpolation.
 * @return True if the processor should execute, false otherwise.
 */
bool UActorTransformSyncProcessor::ShouldProceedWithTick(const float FrameDeltaTime, float& OutAccumulatedDeltaTime)
{
    // --- Dynamic Tick Rate Calculation ---
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f);
    MinTickInterval = FMath::Max(0.001f, MinTickInterval);
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval);

    if (FrameDeltaTime <= 0.0f) return false;

    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;

    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold);
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, FrameDeltaTime);

    AccumulatedTimeA += FrameDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval)
    {
        return false;
    }

    // Output the full accumulated time for use as delta in interpolations
    OutAccumulatedDeltaTime = AccumulatedTimeA;
    AccumulatedTimeA = 0.0f;
    return true;
}

/**
 * @brief Adjusts the entity's vertical position to snap to the ground or maintain flying height.
 * @param UnitBase The unit actor, used for component and type info.
 * @param CharFragment The characteristics fragment, for flight data and storing ground location.
 * @param CurrentActorLocation The current world location of the actor/instance.
 * @param ActualDeltaTime The current frame's delta time, for interpolation.
 * @param MassTransform The entity's transform fragment, which will be updated with the correct scale.
 * @param InOutFinalLocation The location being calculated, its Z value will be modified by this function.
 */
void UActorTransformSyncProcessor::HandleGroundAndHeight(const AUnitBase* UnitBase, FMassAgentCharacteristicsFragment& CharFragment, const FVector& CurrentActorLocation, const float ActualDeltaTime, FTransform& MassTransform, FVector& InOutFinalLocation, bool bIsDead) const
{
    float HeightOffset = 0.f;
    const float CurrentZ = UnitBase->bUseSkeletalMovement ? CurrentActorLocation.Z : CharFragment.PositionedTransform.GetLocation().Z;

    // Determine height offset and scale based on unit type
    MassTransform.SetScale3D(UnitBase->GetActorScale3D());
    HeightOffset = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    /*else
    {
        
        const FTransform& ISMTransform = UnitBase->ISMComponent->GetComponentTransform();
        MassTransform.SetScale3D(ISMTransform.GetScale3D());
        HeightOffset = ISMTransform.GetScale3D().Z / 2.0f;
        if (UnitBase->MassActorBindingComponent->bAddCapsuleHalfHeightToIsm)
        {
            HeightOffset += UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }
        
    */

    // ================================================================================================
    // LUX-ANPASSUNG (28.08.2026) - Rettung, wenn eine Einheit durch die Map faellt.
    // Silvan: "Gegen das durch die Map fallen brauchen wir noch einen besseren Fix, der den
    // Character wieder an die richtige Position setzt. Im Moment kommt es zum Ruckeln."
    //
    // Das Ruckeln kommt vom bisherigen Verhalten: die Einheit wurde mit FInterpConstantTo
    // Stueck fuer Stueck nach oben GEZOGEN, waehrend die Kamera ihr folgte - und im selben Takt
    // zog die Bodenpruefung sie wieder herunter. Eine Rettung ist kein Bewegungsablauf, sondern
    // eine Korrektur: sie gehoert HART gesetzt, in einem Bild.
    //
    // Ausgeloest wird nur, wenn wirklich nichts mehr traegt - entweder lange genug bodenlos
    // (FallRescueAfterSeconds) oder unterhalb einer Hoehe, aus der niemand zurueckkommt
    // (FallRescueBelowZ). Kurze bodenlose Momente an Kanten und Rampen bleiben unberuehrt.
    //
    // Ziel ist die letzte Position MIT belegtem Boden, nicht die Startposition: LastGroundLocation
    // allein ist nur eine Hoehe und wuerde die Einheit an derselben XY-Stelle - also im Loch -
    // wieder hochsetzen. Ohne je gesehenen Boden greift die Rettung gar nicht; die erste sichere
    // Position wird beim ersten Bodenkontakt gesetzt, in aller Regel direkt am Spawn.
    // ================================================================================================
    if (!CharFragment.bIsFlying && !bIsDead && CharFragment.bHasSafeLocation)
    {
        const bool bZuTief  = CurrentZ < FallRescueBelowZ;
        const bool bZuLange = FallRescueAfterSeconds > 0.f
                           && CharFragment.TimeWithoutGround >= FallRescueAfterSeconds;

        if (bZuTief || bZuLange)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Absturz-Rettung] %s von (%.0f,%.0f,%.0f) zurueck auf (%.0f,%.0f,%.0f) - %s"),
                *UnitBase->GetName(),
                InOutFinalLocation.X, InOutFinalLocation.Y, CurrentZ,
                CharFragment.LastSafeLocation.X, CharFragment.LastSafeLocation.Y, CharFragment.LastSafeLocation.Z,
                bZuTief ? TEXT("unter der Rettungshoehe") : TEXT("zu lange ohne Boden"));

            InOutFinalLocation = CharFragment.LastSafeLocation;                       // hart, ohne Interpolation
            CharFragment.LastGroundLocation = CharFragment.LastSafeLocation.Z - HeightOffset;
            CharFragment.TimeWithoutGround = 0.f;

            // Neigung geradeziehen, Blickrichtung behalten.
            const FRotator AktuelleRotation = MassTransform.GetRotation().Rotator();
            MassTransform.SetRotation(FRotator(0.f, AktuelleRotation.Yaw, 0.f).Quaternion());
            return;
        }
    }
    // ===================== ENDE LUX-ANPASSUNG =======================================================

    // --- Ground/Height Adjustment Logic ---
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(UnitBase);
    FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);

    const FVector TraceStart = FVector(InOutFinalLocation.X, InOutFinalLocation.Y, InOutFinalLocation.Z + 1000.0f);
    const FVector TraceEnd = FVector(InOutFinalLocation.X, InOutFinalLocation.Y, InOutFinalLocation.Z - 2000.0f);

    // DIAGNOSE (bleibt stehen bis abbestellt): In Leveln, deren begehbare Flaechen weit
    // ueber Null liegen, haengen die Einheiten dauerhaft unter dem Gelaende. Gemessen wurde
    // eine stabile Hoehe von exakt 0+Kapselhalbhoehe - das deutet auf den Zweig "kein
    // Bodentreffer". Diese Zeile zeigt, ob der Trace wirklich nichts findet und was er
    // gegebenenfalls trifft.
    //
    // 30.08.2026 UMGEBAUT, INHALT UNVERAENDERT. Vorher stand der Block VOR dem eigentlichen Trace,
    // mit dem Waechter "CurrentZ < 500 && !bIsFlying" - und zog sich dafuer einen ZWEITEN,
    // vollstaendigen LineTrace. Der Kommentar versprach "nur im Fehlerfall", aber auf Karten, deren
    // Boden nahe Null liegt (Level_AITest_Xeno: Z=7), erfuellt JEDE Bodeneinheit in JEDEM Takt die
    // Bedingung. Gemessen in drei Partien: 177 Zeilen in der schnellen gegen 6706 in der langsamen,
    // also 21000 gegen 805000 Durchlaeufe - bei gleicher Rechenzeit rechneten die Partien voellig
    // verschiedene SPIELZEITEN ab (t=1499 gegen t=352 gegen t=164), womit jede Kennzahl je Partie
    // ueber unterschiedlich lange Zeitraeume gemessen war.
    //
    // Jetzt haengt die Zeile am Ergebnis des ECHTEN Traces: sie kommt nur, wenn der Boden
    // tatsaechlich nicht gefunden wurde - also genau im Fehlerfall, den sie beschreiben soll. Der
    // zweite Trace entfaellt ersatzlos, die Aussage bleibt dieselbe.
    // Volumenfilter in einer Hilfsfunktion - ein Volumen ist nie Boden, Begruendung unten.
    auto TraceGround = [&](const FVector& TraceFrom, const FVector& TraceTo) -> bool
    {
        bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, TraceFrom, TraceTo, ObjectParams, Params);
        for (int32 Attempt = 0; bHit && Attempt < 4; ++Attempt)
        {
            AActor* HitVolumeActor = Hit.GetActor();
            if (!IsValid(HitVolumeActor) || !HitVolumeActor->IsA(AVolume::StaticClass()))
            {
                break;
            }
            Params.AddIgnoredActor(HitVolumeActor);
            bHit = GetWorld()->LineTraceSingleByObjectType(Hit, TraceFrom, TraceTo, ObjectParams, Params);
        }
        return bHit;
    };

    const bool bBodenGefunden = TraceGround(TraceStart, TraceEnd);

    // [BodenPruef] Belegt, DASS die Hoehenanpassung laeuft - siehe RTS.SkmDiag.
    // Der Aufruf dieser Funktion war zeitweise ganz verschwunden (der Diagnoseblock, der ihn mass,
    // hat ihn beim Abbau mitgenommen). Einheiten blieben dadurch in der Luft und liefen keine
    // Rampen mehr hoch. Diese Zeile zeigt Eingang und Ausgang nebeneinander, damit "der Aufruf
    // steht da" nicht mit "die Hoehe wird angepasst" verwechselt wird.
    const float BP_StartZ = InOutFinalLocation.Z;
    if (CVarRTS_SkmDiag.GetValueOnAnyThread() != 0)
    {
        static double BP_LastReport = 0.0;
        const double BP_Now = FPlatformTime::Seconds();
        if (BP_Now - BP_LastReport > 2.0)
        {
            BP_LastReport = BP_Now;
            UE_LOG(LogTemp, Warning,
                TEXT("[BodenPruef] %s | EinheitZ=%.1f | Boden gefunden=%d TrefferZ=%.1f | Versatz=%.1f | fliegt=%d"),
                *GetNameSafe(UnitBase), BP_StartZ, bBodenGefunden ? 1 : 0,
                bBodenGefunden ? Hit.ImpactPoint.Z : -9999.f, HeightOffset,
                CharFragment.bIsFlying ? 1 : 0);
        }
    }

    if (!bBodenGefunden && !CharFragment.bIsFlying)
    {
        static int32 BodenDiagZaehler = 0;
    }

    if (bBodenGefunden)
    {

        const AActor* HitActor = Hit.GetActor();
        const float DeltaZ = Hit.ImpactPoint.Z - CurrentZ;

        // DIAGNOSE (bleibt stehen bis abbestellt) - "CameraUnit laeuft ins Gelaende und steckt".
        // Gemessen am 01.09.: Actor-Z eingefroren auf 88, Boden an derselben Stelle 176, also
        // 88 Einheiten UNTER dem Gelaende; Zustand Run, kein einziger Sperr-Tag gesetzt. Die
        // Abfrage nimmt die Einheit also mit - trotzdem wird die Hoehe nicht nachgefuehrt.
        // Diese Zeile meldet NUR den Fehlerfall (Treffer verworfen), inklusive WAS getroffen
        // wurde: der Verdacht ist, dass der Trace nicht den Boden findet, sondern gleich am
        // Startpunkt (Z+1000) auf Geometrie laeuft.
        if (!CharFragment.bIsFlying && !bIsDead
            && !(IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass())
                 && DeltaZ <= (HeightOffset + 100.f)))
        {
            static double LetzteBodenMeldung = 0.0;
            const double JetztZeit = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        }

        if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= (HeightOffset+100.f) && !CharFragment.bIsFlying) // && DeltaZ <= HeightOffset
        {
            CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;
            const float TargetZ = Hit.ImpactPoint.Z + HeightOffset;


            // LUX-ANPASSUNG (28.08.2026) - dies ist der EINZIGE Zweig mit belegtem Boden unter der
            // Einheit. Genau hier - und nur hier - wird der Rettungsanker nachgefuehrt.
            CharFragment.LastSafeLocation = FVector(InOutFinalLocation.X, InOutFinalLocation.Y, TargetZ);
            CharFragment.bHasSafeLocation = true;
            CharFragment.TimeWithoutGround = 0.f;

            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);

            if (CharFragment.GroundAlignment)
            {
                // Pitch-only Slope-Alignment: richte die Vorwärtsachse auf die Projektion auf der Bodenebene aus,
                // rotiere dabei ausschließlich um die Right-Achse (kein Roll), Yaw bleibt erhalten.
                const FVector SurfaceUp = Hit.ImpactNormal.GetSafeNormal();

                // Yaw-Only Basis aus aktueller Rotation
                const FRotator CurrentRot = MassTransform.GetRotation().Rotator();
                const FRotator YawOnlyRot(0.f, CurrentRot.Yaw, 0.f);
                const FQuat BaseYawQuat = YawOnlyRot.Quaternion();

                const FVector YawForward = BaseYawQuat.GetForwardVector(); // bereits ohne Pitch/Roll
                const FVector RightAxis = BaseYawQuat.GetRightVector();

                FVector SlopeForward = FVector::VectorPlaneProject(YawForward, SurfaceUp).GetSafeNormal();

                if (!SlopeForward.IsNearlyZero())
                {
                    // Signierter Winkel um die Right-Achse zwischen YawForward und SlopeForward
                    const FVector Cross = FVector::CrossProduct(YawForward, SlopeForward);
                    const float Sin = FVector::DotProduct(Cross, RightAxis);
                    const float Cos = FVector::DotProduct(YawForward, SlopeForward);
                    const float PitchAngleRad = FMath::Atan2(Sin, Cos);

                    const FQuat PitchQuat(RightAxis, PitchAngleRad);
                    const FQuat DesiredQuat = PitchQuat * BaseYawQuat;

                    // Interpolation zur Zielrotation (nur Pitch ändert sich)
                    const float GroundSlopeRotationSpeedDegrees = 360.0f; // ggf. als UPROPERTY konfigurieren
                    const FQuat NewRotQuat = FMath::QInterpConstantTo(
                        MassTransform.GetRotation(),
                        DesiredQuat,
                        ActualDeltaTime,
                        FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
                    );
                    MassTransform.SetRotation(NewRotQuat);
                }
            }
            else
            {
                // Revert pitch and roll to zero (level) if GroundAlignment is disabled
                FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
                FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
                const float GroundSlopeRotationSpeedDegrees = 360.0f;
                FQuat NewRotQuat = FMath::QInterpConstantTo(
                    MassTransform.GetRotation(),
                    DesiredRotator.Quaternion(),
                    ActualDeltaTime,
                    FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
                );
                MassTransform.SetRotation(NewRotQuat);
            }
        }
        else if (!CharFragment.bIsFlying) // Not on a valid ground hit, but not flying (e.g., walking off a ledge, or on another unit)
        {
            // Der Bodentreffer wurde oben verworfen, weil er zu weit UEBER der Einheit liegt.
            // Diese Regel verhindert, dass Einheiten auf Klippen oder Bruecken hochschnappen -
            // sie sperrt aber auch den Rueckweg: wer einmal unter das Gelaende geraten ist,
            // haelt sich danach an einem veralteten LastGroundLocation fest und kommt nie
            // wieder hoch. In Leveln, deren begehbare Flaechen weit ueber dem Nullniveau
            // liegen (Labyrinth: Wege auf Z=900), blieben dadurch ALLE Einheiten unsichtbar
            // unter dem Boden haengen; im Prologue faellt es nicht auf, weil dort der Boden
            // ohnehin bei Z=0 liegt.
            // Unter einer Bruecke oder in einem Tunnel steht feste Geometrie UNTER der
            // Einheit und die Hoehe stimmt. Haengt sie dagegen im Leeren, ist sie durch das
            // Gelaende gerutscht - nur dieser Fall wird korrigiert.
            // Nur die LANDSCHAFT zaehlt hier als Beleg fuer "unter dem Gelaende". Der erste
            // Anlauf liess jeden Treffer ausser AUnitBase gelten - die sichtbare Darstellung
            // haengt aber an einem Visual-Manager und nicht am Einheiten-Actor, also traf der
            // Trace die eigene Darstellung und hob die Einheit schrittweise an (gemessen:
            // Actor auf 910, Mesh auf 1500-1826 statt auf 988).
            // LUX-ANPASSUNG (28.08.2026) - die Abwaerts-Probe wird jetzt IMMER gebraucht: sie ist
            // zugleich der Beleg fuer "haengt im Leeren" und damit die Bedingung, unter der die
            // Absturz-Uhr laeuft. Auf einer anderen Einheit, unter einer Bruecke oder in einem
            // Tunnel steht feste Geometrie darunter - dort darf die Uhr NICHT laufen, sonst wuerde
            // eine voellig gesunde Einheit weggerissen.
            FHitResult BodenDarunter;
            const FVector AbwaertsStart(InOutFinalLocation.X, InOutFinalLocation.Y, CurrentZ - 5.f);
            const FVector AbwaertsEnde(InOutFinalLocation.X, InOutFinalLocation.Y, CurrentZ - 10000.f);
            const bool bBodenDarunter = GetWorld()->LineTraceSingleByObjectType(
                BodenDarunter, AbwaertsStart, AbwaertsEnde, ObjectParams, Params);

            if (IsValid(HitActor) && HitActor->IsA(ALandscapeProxy::StaticClass()) && Hit.ImpactPoint.Z > CurrentZ)
            {
                if (!bBodenDarunter)
                {
                    CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;
                }
            }

            if (bBodenDarunter) CharFragment.TimeWithoutGround = 0.f;
            else                CharFragment.TimeWithoutGround += ActualDeltaTime;

            const float TargetZ = CharFragment.LastGroundLocation + HeightOffset;


            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed * 100.f);

            // Revert pitch and roll to zero (level) if not on valid ground
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f;
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
            );
            MassTransform.SetRotation(NewRotQuat);
        }
        else if (IsValid(HitActor) && CharFragment.bIsFlying) // Flying, but a hit occurred (e.g., flying over terrain)
        {
            const float TargetZ = bIsDead ? Hit.ImpactPoint.Z + HeightOffset : Hit.ImpactPoint.Z + CharFragment.FlyHeight;
            const float InterpSpeed = bIsDead ? VerticalDeadInterpSpeed : VerticalInterpSpeed;

            
            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, InterpSpeed * 100.f);
            CharFragment.LastGroundLocation = Hit.ImpactPoint.Z;

            // For flying units, maintain flat pitch/roll unless specific flight controls dictate otherwise
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            
            if (bIsDead && CharFragment.VerticalDeathRotationMultiplier > 0.f && CurrentZ > TargetZ + 1.f)
            {
                const float DeltaYaw = CharFragment.VerticalDeathRotationMultiplier * ActualDeltaTime;
                CurrentRotation.Yaw += DeltaYaw;
                //UE_LOG(LogTemp, Warning, TEXT("DeathSpin TraceHit TRIGGERED: Multiplier=%.2f DeltaYaw=%.2f NewYaw=%.2f"), CharFragment.VerticalDeathRotationMultiplier, DeltaYaw, CurrentRotation.Yaw);
            }
            
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f; // Or a specific flying rotation speed
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(FMath::Max(GroundSlopeRotationSpeedDegrees, CharFragment.VerticalDeathRotationMultiplier))
            );
            MassTransform.SetRotation(NewRotQuat);
        }
    }
    else // No ground hit detected (e.g., entirely in air)
    {
        if (CharFragment.bIsFlying)
        {
            const float TargetZ = bIsDead ? CharFragment.LastGroundLocation + HeightOffset : CharFragment.LastGroundLocation + CharFragment.FlyHeight;
            const float InterpSpeed = bIsDead ? VerticalDeadInterpSpeed : VerticalInterpSpeed;

            InOutFinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, InterpSpeed * 100.f);

            // Revert pitch and roll to zero (level) and handle death rotation
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            if (bIsDead && CharFragment.VerticalDeathRotationMultiplier > 0.f && CurrentZ > TargetZ + 1.f)
            {
                const float DeltaYaw = CharFragment.VerticalDeathRotationMultiplier * ActualDeltaTime;
                CurrentRotation.Yaw += DeltaYaw;
            }

            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f);
            const float GroundSlopeRotationSpeedDegrees = 360.0f;
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(FMath::Max(GroundSlopeRotationSpeedDegrees, CharFragment.VerticalDeathRotationMultiplier))
            );
            MassTransform.SetRotation(NewRotQuat);
        }
        else // Not flying and no ground hit (e.g., falling or airborne)
        {
            // LUX-ANPASSUNG (28.08.2026) - der Trace deckt 1000 ueber bis 2000 unter der Einheit ab.
            // Findet er darin gar nichts, ist unter ihr nachweislich Leere: Absturz-Uhr laeuft.
            CharFragment.TimeWithoutGround += ActualDeltaTime;

            InOutFinalLocation.Z = CharFragment.LastGroundLocation + HeightOffset;

            // Revert pitch and roll to zero (level)
            FRotator CurrentRotation = MassTransform.GetRotation().Rotator();
            FRotator DesiredRotator(0.f, CurrentRotation.Yaw, 0.f); // Only keep yaw
            const float GroundSlopeRotationSpeedDegrees = 360.0f;
            FQuat NewRotQuat = FMath::QInterpConstantTo(
                MassTransform.GetRotation(),
                DesiredRotator.Quaternion(),
                ActualDeltaTime,
                FMath::DegreesToRadians(GroundSlopeRotationSpeedDegrees)
            );
            MassTransform.SetRotation(NewRotQuat);
        }
    }
    if (CVarRTS_SkmDiag.GetValueOnAnyThread() != 0 && !FMath::IsNearlyEqual(BP_StartZ, InOutFinalLocation.Z, 0.01f))
    {
        static double BP_LastChange = 0.0;
        const double BP_Now2 = FPlatformTime::Seconds();
        if (BP_Now2 - BP_LastChange > 2.0)
        {
            BP_LastChange = BP_Now2;
            UE_LOG(LogTemp, Warning, TEXT("[BodenPruef] %s | Hoehe angepasst: %.1f -> %.1f"),
                *GetNameSafe(UnitBase), BP_StartZ, InOutFinalLocation.Z);
        }
    }

}

void UActorTransformSyncProcessor::RotateTowardsMovement(AUnitBase* UnitBase, const FVector& CurrentVelocity, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FMassAIStateFragment& State, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    FQuat CurrentQuat = InOutMassTransform.GetRotation();
    FRotator CurrentRot = CurrentQuat.Rotator();
    float TargetYaw = CurrentRot.Yaw;

    if (Char.RotatesToMovement && !CurrentVelocity.IsNearlyZero(50.f) && !CurrentActorLocation.Equals(State.StoredLocation, UnitBase->MovementAcceptanceRadius))
    {
        FVector LookAtDir = CurrentVelocity;
        LookAtDir.Z = 0.f;
        if (LookAtDir.Normalize())
        {
            FQuat DesiredQuat = LookAtDir.ToOrientationQuat();
            
            TargetYaw = DesiredQuat.Rotator().Yaw;
        }
    }

    if (Char.RotatesToMovement)
    {
        const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
        if (RotationSpeedDeg > KINDA_SMALL_NUMBER)
        {
            CurrentRot.Yaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, RotationSpeedDeg * ActualDeltaTime);
        }
        else
        {
            CurrentRot.Yaw = TargetYaw;
        }
        InOutMassTransform.SetRotation(CurrentRot.Quaternion());
    }
}

void UActorTransformSyncProcessor::RotateTowardsTarget(AUnitBase* UnitBase, FMassEntityManager& EntityManager, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform, bool bPreferEnemy) const
{
    // Proceed if we have either a resolved target entity OR a valid target flag with a non-zero last known location, and rotation to enemy is enabled
    const bool bHasUsableTarget = TargetFrag.TargetEntity.IsSet() || (TargetFrag.bHasValidTarget && !TargetFrag.LastKnownLocation.IsNearlyZero()) ||
                                   TargetFrag.FriendlyTargetEntity.IsSet() || !TargetFrag.LastKnownFriendlyLocation.IsNearlyZero();

    if (!bHasUsableTarget || !Char.RotatesToEnemy)
    {
        return;
    }
    
    FVector TargetLocation = FVector::ZeroVector;
    FMassEntityHandle TargetEntity;

    if (bPreferEnemy && EntityManager.IsEntityValid(TargetFrag.TargetEntity))
    {
        TargetEntity = TargetFrag.TargetEntity;
        TargetLocation = TargetFrag.LastKnownLocation;
    }
    else if (EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity))
    {
        TargetEntity = TargetFrag.FriendlyTargetEntity;
        TargetLocation = TargetFrag.LastKnownFriendlyLocation;
    }
    else if (EntityManager.IsEntityValid(TargetFrag.TargetEntity))
    {
        TargetEntity = TargetFrag.TargetEntity;
        TargetLocation = TargetFrag.LastKnownLocation;
    }
    else if (bPreferEnemy && !TargetFrag.LastKnownLocation.IsNearlyZero())
    {
        TargetLocation = TargetFrag.LastKnownLocation;
    }
    else if (!TargetFrag.LastKnownFriendlyLocation.IsNearlyZero())
    {
        TargetLocation = TargetFrag.LastKnownFriendlyLocation;
    }
    else if (!TargetFrag.LastKnownLocation.IsNearlyZero())
    {
        TargetLocation = TargetFrag.LastKnownLocation;
    }

    if (EntityManager.IsEntityValid(TargetEntity))
    {
        if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity))
        {
            TargetLocation = TargetXform->GetTransform().GetLocation();
        }
    }
   
    if (TargetLocation.IsNearlyZero())
    {
        return;
    }

    FVector Dir = TargetLocation - CurrentActorLocation;
    Dir.Z = 0.f;
    
    // Skip rotation if too close to target to avoid jittering/instability
    if (Dir.SizeSquared() < 25.f) // 5cm threshold
    {
        return;
    }

    if (!Dir.Normalize())
    {
        return;
    }
    
    FQuat DesiredQuat = Dir.ToOrientationQuat();

    //if (!UnitBase->bUseSkeletalMovement)
        //DesiredQuat *= UnitBase->MeshRotationOffset;
    
    float TargetYaw = DesiredQuat.Rotator().Yaw;
    FRotator CurrentRot = InOutMassTransform.GetRotation().Rotator();

    // Deadzone: If already pointing mostly towards target, don't rotate to avoid micro-adjustments/jitter
    const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetYaw));
    if (YawDelta < 2.5f)
    {
        return;
    }

    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    
    if (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
    {
        CurrentRot.Yaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, RotationSpeedDeg * ActualDeltaTime);
    }
    else
    {
        CurrentRot.Yaw = TargetYaw;
    }
    
    InOutMassTransform.SetRotation(CurrentRot.Quaternion());
}

bool UActorTransformSyncProcessor::RotateTowardsAbility(AUnitBase* UnitBase, const FMassAITargetFragment& AbilityTarget, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const
{
    // Guard: an unset ability target location is the zero vector. Subtracting the actor
    // location would then make Dir point at world origin, spuriously yawing the BASE
    // transform (PositionedTransform). For construction/drone units that cast without a
    // real target this base-yaw is magnified by the orbit lever arm (DroneOrbitCenter)
    // into visible ISM jitter. Treat as "reached" so the caller clears bRotateTowardsAbility.
    if (AbilityTarget.AbilityTargetLocation.IsNearlyZero())
    {
        return true;
    }

    // Calculate direction from the unit to the ability's target location
    FVector Dir = AbilityTarget.AbilityTargetLocation - CurrentActorLocation;
    Dir.Z = 0.f;  // Flatten to the XY plane for rotation

    // Skip rotation if too close to target to avoid jittering
    if (Dir.SizeSquared() < 25.f)
    {
        return true;
    }

    if (!Dir.Normalize())
    {
        return true; // No direction to rotate => treat as reached to prevent stuck state
    }

    FQuat DesiredQuat = Dir.ToOrientationQuat();

    //if (!UnitBase->bUseSkeletalMovement)
    {
        //DesiredQuat *= UnitBase->MeshRotationOffset;
    }

    float TargetYaw = DesiredQuat.Rotator().Yaw;
    FRotator CurrentRot = InOutMassTransform.GetRotation().Rotator();

    // Deadzone: If already pointing mostly towards target, don't rotate
    const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetYaw));
    if (YawDelta < 2.5f)
    {
        return true;
    }

    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;

    if (RotationSpeedDeg > KINDA_SMALL_NUMBER * 10.f)
    {
        CurrentRot.Yaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, RotationSpeedDeg * ActualDeltaTime);
    }
    else
    {
        CurrentRot.Yaw = TargetYaw;
    }

    InOutMassTransform.SetRotation(CurrentRot.Quaternion());

    // Determine if we reached the desired facing (yaw only)
    const float FinalYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetYaw));
    const float ReachedToleranceDeg = 2.0f; // tolerance for considering rotation reached

    return FinalYawDelta <= ReachedToleranceDeg;
}

// Yaw-only facing for WORKING workers (Build / Repair / ResourceExtraction): turn toward the
// actual work target. Must NOT aim at WorkerStats.BuildAreaPosition — that is the worker's own
// stand-point on the area edge, so (BuildAreaPosition - WorkerLocation) is a near-zero vector
// whose direction is noise (sideways/away, depending on where the worker stopped inside the
// arrival radius). BuildArea/ResourcePlace are replicated, so this works on server AND client.
// Returns true when the working state was handled (target found, or intentionally holding yaw).
static bool RotateYawTowardsWorkTarget(AUnitBase* UnitBase, FMassEntityManager& EntityManager,
    const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats,
    const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentLocation,
    float DeltaTime, bool bIsBuilding, bool bIsRepairing, bool bIsExtracting,
    FTransform& InOutMassTransform)
{
    FVector WorkLocation = FVector::ZeroVector;
    bool bHasWorkLocation = false;

    if (bIsBuilding && IsValid(UnitBase->BuildArea))
    {
        WorkLocation = UnitBase->BuildArea->GetActorLocation();
        bHasWorkLocation = true;
    }
    else if (bIsExtracting && IsValid(UnitBase->ResourcePlace))
    {
        WorkLocation = UnitBase->ResourcePlace->GetActorLocation();
        bHasWorkLocation = true;
    }
    else if (bIsRepairing)
    {
        if (EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity))
        {
            if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
            {
                WorkLocation = TargetXform->GetTransform().GetLocation();
                bHasWorkLocation = true;
            }
        }
        // Client fallback: FriendlyTargetEntity is mirrored from the replicated FollowUnit and can
        // lag behind (unlinked client Mass handle). The replicated actor pointer needs no Mass link.
        if (!bHasWorkLocation && IsValid(UnitBase->FollowUnit))
        {
            WorkLocation = UnitBase->FollowUnit->GetActorLocation();
            bHasWorkLocation = true;
        }
        if (!bHasWorkLocation && !TargetFrag.LastKnownFriendlyLocation.IsNearlyZero())
        {
            WorkLocation = TargetFrag.LastKnownFriendlyLocation;
            bHasWorkLocation = true;
        }
    }

    if (!bHasWorkLocation)
    {
        return false; // caller decides on a fallback
    }

    FVector Dir = WorkLocation - CurrentLocation;
    Dir.Z = 0.f;
    if (Dir.SizeSquared() < 25.f || !Dir.Normalize())
    {
        return true; // standing on top of the target: hold current yaw instead of feeding noise in
    }

    const float TargetYaw = Dir.ToOrientationQuat().Rotator().Yaw;
    FRotator CurrentRot = InOutMassTransform.GetRotation().Rotator();

    // Deadzone against micro-jitter once we face the target
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetYaw)) < 2.0f)
    {
        return true;
    }

    const float RotationSpeedDeg = Stats.RotationSpeed * Char.RotationSpeed;
    if (RotationSpeedDeg > KINDA_SMALL_NUMBER)
    {
        CurrentRot.Yaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, RotationSpeedDeg * DeltaTime);
    }
    else
    {
        CurrentRot.Yaw = TargetYaw;
    }
    InOutMassTransform.SetRotation(CurrentRot.Quaternion());
    return true;
}

void UActorTransformSyncProcessor::DispatchPendingUpdates(TArray<FActorTransformUpdatePayload>&& PendingUpdates)
{
    if (PendingUpdates.IsEmpty())
    {
        return;
    }

    // NOTE: the client render smoothing (Idea 1) is applied UPSTREAM in ExecuteClient (the payload's NewTransform is
    // already the smoothed VisualXf for units; the Mass fragment stays exact). Here we just apply it.
    for (const FActorTransformUpdatePayload& Update : PendingUpdates)
    {
        if (AActor* Actor = Update.ActorPtr.Get())
        {
            Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::None);
        }
    }
}

// Die direkt gesteuerte Einheit des Spielers wird NIE gedrosselt.
//
// Sie steht unter der Hand des Spielers: ein Nachziehen erst alle halbe Sekunde ist dort sofort
// als Ruckeln sichtbar, waehrend es bei einer von 500 Einheiten im Pulk niemandem auffaellt. Es
// ist ausserdem genau EINE Einheit je Spieler - die Ausnahme kostet also nichts.
static bool IsCameraUnit(const AUnitBase* Unit)
{
	static const FGameplayTag CameraUnitRootTag =
		FGameplayTag::RequestGameplayTag(FName("Character.CameraUnit"), false);

	return Unit != nullptr
		&& CameraUnitRootTag.IsValid()
		&& Unit->UnitTags.HasTag(CameraUnitRootTag);
}

float UActorTransformSyncProcessor::CalculateActorSyncInterval() const
{
	// Siehe ActorSyncScaleStartUnits im Header. Der CVar-Schalter hat Vorrang (Messungen).
	const float Override = CVarRTS_ActorSyncInterval.GetValueOnAnyThread();
	if (Override >= 0.f)
	{
		return Override;
	}
	// Die beiden Schwellen lassen sich zur Laufzeit ueberschreiben (RTS.ActorSync.StartUnits und
	// RTS.ActorSync.FullUnits). Ohne das muesste man fuer jede Messreihe neu bauen.
	const int32 StartUnitsOverride = CVarRTS_ActorSyncStartUnits.GetValueOnAnyThread();
	const int32 FullUnitsOverride = CVarRTS_ActorSyncFullUnits.GetValueOnAnyThread();
	const int32 StartUnits = StartUnitsOverride >= 0 ? StartUnitsOverride : ActorSyncScaleStartUnits;
	const int32 FullUnits = FullUnitsOverride >= 0 ? FullUnitsOverride : ActorSyncScaleFullUnits;

	if (ActorSyncMaxInterval <= 0.f || FullUnits <= StartUnits)
	{
		return VisualISMActorSyncTime;
	}

	const float Ratio = FMath::Clamp(
		float(LastFrameUnitCount - StartUnits)
		/ float(FullUnits - StartUnits), 0.f, 1.f);

	return FMath::Max(VisualISMActorSyncTime, Ratio * ActorSyncMaxInterval);
}

void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UActorTransformSyncProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UActorTransformSyncProcessor);

	if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
	{
		ExecuteClient(EntityManager, Context);
	}
	else
	{
		ExecuteServer(EntityManager, Context);
	}
}

void UActorTransformSyncProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    /*
    const float FrameDeltaTime = Context.GetDeltaTimeSeconds();
    
    float ActualDeltaTime = 0.0f;
    if (!ShouldProceedWithTick(FrameDeltaTime, ActualDeltaTime)) return;
    */
    float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    const float CurrentTime = Context.GetWorld()->GetTimeSeconds();

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(ClientEntityQuery.GetNumMatchingEntities());
    
    ClientEntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, ActualDeltaTime, CurrentTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
            
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassRepresentationLODFragment> LODFragments = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();

        // ============================================================================
        // TAG-ABFRAGE JE CHUNK STATT JE EINHEIT  (18.09.2026)
        //
        // GEMESSENER NUTZEN: 0,55 ms je Bild bei 510 Einheiten. RTS.ChunkTags.MessAlt 1 misst die
        // alte Variante an Ort und Stelle: 160,8 ms ueber 151.980 Entitaeten in 5 s. Das sind rund
        // 2,6 % der Bildzeit im Marsch (21,10 ms) - real, aber kein grosser Posten.
        //
        // WARUM ES SICHER IST, obwohl Tags an 106 Stellen EINZELN vergeben werden:
        //   1. Engine-Quelle MassEntityQuery.cpp: ForEachEntityChunk laeuft
        //      "for (const int32 ArchetypeIndex : OrderedArchetypeIndices)" und ruft
        //      ExecuteFunction JE ARCHETYP. Ein Chunk-Durchlauf sieht nie zwei Archetypen.
        //   2. Tags SIND Teil des Archetyps. Defer().AddTag<T>(Entity) verschiebt die Entitaet in
        //      einen anderen Archetyp - gemischte Tags in einem Chunk koennen nicht entstehen.
        //   3. RTS.ChunkTags.Verify 1 verglich jede Entitaet jedes Chunks gegen DoesEntityHaveTag:
        //      0 Deviations bei ~200.000 Vergleichen. ABER nur 2 verschiedene Tag-Kombinationen,
        //      weil im Messfall alle Einheiten dasselbe tun - fuer sich genommen schwach.
        //      Tragend sind 1 und 2, nicht 3.
        //
        // SO WIRD ES RUECKGAENGIG GEMACHT:
        //   - diese Zeile, den Verify-Block und die zehn "const bool bChunk_..." loeschen
        //   - jedes  bChunk_MassStateXTag  ersetzen durch
        //       DoesEntityHaveTag(EntityManager, Entity, FMassStateXTag::StaticStruct())
        //   - FChunkTagLookup in UnitMassTag.h kann dann ebenfalls entfallen
        //   So sah es vorher aus:
        //       const bool bIsIdle = DoesEntityHaveTag(EntityManager, Entity, FMassStateIdleTag::StaticStruct());
        //       if (DoesEntityHaveTag(EntityManager, Entity, FMassStateFrozenTag::StaticStruct())) continue;
        //   Kosten der Ruecknahme: +0,55 ms je Bild.
        // ============================================================================
        // Tags einmal je Chunk statt je Einheit - siehe FChunkTagLookup in UnitMassTag.h.

        // Innerhalb eines Chunks teilen sich alle Entitaeten denselben Archetyp und damit

        // dieselben Tags; die Abfrage je Einheit war reine Wiederholung.

        int32 ChunkUnitCount = 0;
        int32 ChunkExemptCount = 0;
        const FChunkTagLookup ChunkTags(EntityManager, ChunkContext);
        if (CVarRTS_VerifyChunkTags.GetValueOnAnyThread() != 0)
        {
        	VerifyChunkTags(EntityManager, ChunkContext, ChunkTags, {
        		FMassStateFrozenTag::StaticStruct(), FMassStateIdleTag::StaticStruct(),
        		FMassStateDeadTag::StaticStruct(), FMassStateAttackTag::StaticStruct(),
        		FMassStatePauseTag::StaticStruct(), FMassStateBuildTag::StaticStruct(),
        		FMassStateRepairTag::StaticStruct(), FMassStateResourceExtractionTag::StaticStruct(),
        		FMassUnitYawFollowTag::StaticStruct(), FMassRotateToMouseTag::StaticStruct() });
        }

        const bool bChunk_MassRotateToMouseTag = ChunkTags.Has(FMassRotateToMouseTag::StaticStruct());

        const bool bChunk_MassStateAttackTag = ChunkTags.Has(FMassStateAttackTag::StaticStruct());

        const bool bChunk_MassStateBuildTag = ChunkTags.Has(FMassStateBuildTag::StaticStruct());

        const bool bChunk_MassStateDeadTag = ChunkTags.Has(FMassStateDeadTag::StaticStruct());

        const bool bChunk_MassStateFrozenTag = ChunkTags.Has(FMassStateFrozenTag::StaticStruct());

        const bool bChunk_MassStateIdleTag = ChunkTags.Has(FMassStateIdleTag::StaticStruct());

        const bool bChunk_MassStatePauseTag = ChunkTags.Has(FMassStatePauseTag::StaticStruct());

        const bool bChunk_MassStateRepairTag = ChunkTags.Has(FMassStateRepairTag::StaticStruct());

        const bool bChunk_MassStateResourceExtractionTag = ChunkTags.Has(FMassStateResourceExtractionTag::StaticStruct());

        const bool bChunk_MassUnitYawFollowTag = ChunkTags.Has(FMassUnitYawFollowTag::StaticStruct());

        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Mirror server: skip updates for entities not visible
            if (LODFragments[i].LOD == EMassLOD::Off) continue;

            // [TagKostenDiag] Siehe RTS.ChunkTags.MessAlt.
            if (CVarRTS_MeasureTagCost.GetValueOnAnyThread() != 0)
            {
                static double TC_Sum = 0.0;
                static int32 TK_Bilder = 0;
                static double TC_LastReport = 0.0;
                static int32 TK_Entitaeten = 0;

                const FMassEntityHandle TK_Entity = ChunkContext.GetEntity(i);
                const double TC_Start = FPlatformTime::Seconds();
                volatile int32 TK_Senke = 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateFrozenTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateIdleTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateDeadTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateAttackTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStatePauseTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateBuildTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateRepairTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateResourceExtractionTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassUnitYawFollowTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassRotateToMouseTag::StaticStruct()) ? 1 : 0;
                TC_Sum += FPlatformTime::Seconds() - TC_Start;
                ++TK_Entitaeten;

                const double TC_Now = FPlatformTime::Seconds();
                if (TC_Now - TC_LastReport > 5.0)
                {
                    TC_LastReport = TC_Now;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[TagKostenDiag] die ALTE Abfrage je Einheit: %.3f ms gesamt in 5 s, %d Entitaeten -> %.4f ms je 510 Einheiten"),
                        TC_Sum * 1000.0, TK_Entitaeten,
                        TK_Entitaeten > 0 ? (TC_Sum * 1000.0 / TK_Entitaeten) * 510.0 : 0.0);
                    TC_Sum = 0.0; TK_Entitaeten = 0;
                }
            }

            // GetMassActorLocation. 510 ueber den Speicher verstreute Aktorobjekte; genau das
            // verhindert Parallelitaet. Sitzt hier die Zeit, ist Aufteilen der Weg.
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;
            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (bChunk_MassStateFrozenTag) continue;
            
            const bool bIsIdle = bChunk_MassStateIdleTag;
            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            const FQuat CurrentRotation = Actor->GetActorRotation().Quaternion();
            FVector FinalLocation = MassTransform.GetLocation();
            
            // Determine the actor's current location once
            FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            const bool bIsDead = bChunk_MassStateDeadTag;

            if (bIsDead)
            {
                FinalLocation.X = CurrentActorLocation.X;
                FinalLocation.Y = CurrentActorLocation.Y;
            }

            // 1. Adjust rotation based on state (moving vs. attacking vs. working)
            const bool bIsBuilding = bChunk_MassStateBuildTag;
            const bool bIsRepairing = bChunk_MassStateRepairTag;
            const bool bIsExtracting = bChunk_MassStateResourceExtractionTag;
            const bool bIsAttackingOrPaused = bChunk_MassStateAttackTag ||
                                              bChunk_MassStatePauseTag ||
                                              bIsBuilding || bIsRepairing || bIsExtracting;

            const bool bIsIdleHold = bChunk_MassStateIdleTag && StateList[i].HoldPosition;
            const bool bHasValidTarget = TargetList[i].bHasValidTarget && EntityManager.IsEntityActive(TargetList[i].TargetEntity);
            const bool bShouldRotateToTarget = bIsAttackingOrPaused || (bIsIdleHold && bHasValidTarget);

            const bool bRotatesToMovementWhileAttacking = StatsList[i].bCanMoveWhileAttacking && StatsList[i].bRotatesToMovementIfMoveWhileAttacking;
            const bool bIsMoving = !VelocityList[i].Value.IsNearlyZero(50.f);

            const bool bIsYawFollowing = bChunk_MassUnitYawFollowTag;
            
            const bool bIsWorking = bIsBuilding || bIsRepairing || bIsExtracting;

            // ============================================================================
            // LUX-ANPASSUNG (16.08.2026) â€” beim Zielen gehoert der Yaw der Maus.
            // Vor der Aenderung war eine zielende Einheit komplett aus diesem Prozessor
            // ausgeschlossen (Tag-Sperre, siehe ConfigureQueries). Jetzt laeuft sie hier
            // mit, damit die POSITION weiter synchronisiert wird - die ROTATION muss aber
            // beim UMassRotateToMouseProcessor bleiben, sonst dreht sich die Einheit in
            // Bewegungsrichtung statt in Schussrichtung.
            // Greift nur bei der direkt gesteuerten CameraUnit: jede andere Einheit traegt
            // zusaetzlich FMassStopWhileAimingTag und ist weiter ganz ausgeschlossen.
            // Original: if (bIsDead || (bIsYawFollowing && !bIsWorking))
            // ============================================================================
            const bool bLuxAimingAtMouse = bChunk_MassRotateToMouseTag;

            if (bIsDead || (bIsYawFollowing && !bIsWorking) || bLuxAimingAtMouse)
            {
                // Regular rotation updates are skipped for dead units (Death spin is handled in HandleGroundAndHeight)
                // or while UUnitRotateToTargetProcessor owns the yaw (FMassUnitYawFollowTag).
                // Exception: WORKING units — that processor only reads the enemy target slot and its
                // query excludes the work states, so work-target facing below must run here.
            }
            else if (TargetList[i].bRotateTowardsAbility)
            {
                const bool bReached = RotateTowardsAbility(UnitBase, TargetList[i], StatsList[i], CharList[i], FinalLocation, ActualDeltaTime, MassTransform);
                if (bReached)
                {
                    TargetList[i].bRotateTowardsAbility = false;
                }
            }
            // LUX-ANPASSUNG (17.08.2026) - waehrend eines Casts darf die LAUFRICHTUNG die Drehung
            // nicht uebernehmen. GEMESSEN: der Client sprang beim Casten im Laufen um bis zu 150 Grad
            // hin und her (ClientYaw 67.4 / -87.3 / 67.4 / -20.8 ...), obwohl die Replikationskorrektur
            // bereits aus war - es waren also zwei LOKALE Quellen. Der Zweig darueber
            // (bRotateTowardsAbility) loescht sein Flag, sobald die Drehung "erreicht" ist,
            // CastingStateProcessor setzt es im naechsten Takt wieder - und genau in den Frames
            // dazwischen drehte dieser Zweig hierher auf die Laufrichtung.
            // Die erste Haelfte der Bedingung schliesst Casting schon aus; der zweiten
            // (bRotatesToMovementWhileAttacking) fehlte dieselbe Ausnahme.
            else if ((!bShouldRotateToTarget && UnitBase->GetUnitState() != UnitData::Casting && !bIsIdle) || (bRotatesToMovementWhileAttacking && bIsMoving && UnitBase->GetUnitState() != UnitData::Casting))
            {
                RotateTowardsMovement(UnitBase, VelocityList[i].Value, StatsList[i], CharList[i], StateList[i], FinalLocation, ActualDeltaTime, MassTransform);
            }
            else
            {
                // Working workers face their actual work target (BuildArea / ResourcePlace / repaired unit)
                bool bRotatedToWorkTarget = false;
                if (bIsBuilding || bIsRepairing || bIsExtracting)
                {
                    bRotatedToWorkTarget = RotateYawTowardsWorkTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i],
                        FinalLocation, ActualDeltaTime, bIsBuilding, bIsRepairing, bIsExtracting, MassTransform);
                }

                // No fallback for extraction: without a ResourcePlace pointer, holding the current yaw
                // beats turning toward an unrelated friendly target.
                if (!bRotatedToWorkTarget && !bIsExtracting)
                {
                    const bool bPreferEnemy = bChunk_MassStateAttackTag ||
                                              bChunk_MassStatePauseTag;
                    RotateTowardsTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i], FinalLocation, ActualDeltaTime, MassTransform, bPreferEnemy);
                }
            }

            // 2. Adjust height for ground snapping or flying
            //
            // DIESER AUFRUF HAT GEFEHLT (wiederhergestellt 18.09.2026) - zum ZWEITEN Mal derselbe
            // Fehler: er stand im Rumpf des [BodenDiag]-Blocks, der ihn gemessen hat, und meine
            // klammerzaehlende Entfernung der Diagnose hat ihn mitgenommen. Vorher war schon
            // DispatchPendingUpdates auf dieselbe Weise verschwunden.
            //
            // WIE SICH DAS GEZEIGT HAT: Einheiten, die in der Luft platziert wurden, fielen nicht
            // mehr auf den Boden; niemand lief mehr Rampen hoch; Flugeinheiten fuehrten ihre Hoehe
            // nicht mehr ueber dem Gelaende nach. Ohne diesen Aufruf gibt es auf dem Server
            // ueberhaupt keine Hoehenanpassung.
            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, FinalLocation, bIsDead);

            // 3. Apply final location to the AUTHORITATIVE Mass fragment (exact — used by avoidance/gameplay/selection).
            MassTransform.SetLocation(FinalLocation);

            const bool bLocationChanged = !CurrentActorLocation.Equals(FinalLocation, 0.025f);
            const bool bRotationChanged = !CurrentRotation.Equals(MassTransform.GetRotation(), 0.0001f);

            // IDEA 1 — client render smoothing: build a SMOOTHED visual transform (separate from the exact fragment)
            // that eases horizontally toward the authoritative location, filtering residual jitter at the OUTPUT.
            // Both the ISM visual (PositionedTransform) and the actor use it; the fragment stays exact (no feedback).
            // State = last frame's PositionedTransform. bVisualMoved keeps the ISM re-rendering while it eases in.
            FTransform VisualXf = MassTransform;
            bool bVisualMoved = false;
            if (CVarRTS_ClientRenderSmoothing.GetValueOnAnyThread() != 0 && ActualDeltaTime > 0.f)
            {
                const FVector AuthLoc = MassTransform.GetLocation();
                const FVector PrevVis = CharList[i].PositionedTransform.GetLocation();
                FVector SmoothLoc = FMath::VInterpTo(PrevVis, AuthLoc, ActualDeltaTime, CVarRTS_ClientRenderSmoothingSpeed.GetValueOnAnyThread());
                FVector Lag = SmoothLoc - AuthLoc; Lag.Z = 0.f;
                const float MaxLag = FMath::Max(0.f, CVarRTS_ClientRenderSmoothingMaxLag.GetValueOnAnyThread());
                if (Lag.SizeSquared() > FMath::Square(MaxLag)) SmoothLoc = AuthLoc + Lag.GetClampedToMaxSize(MaxLag);
                SmoothLoc.Z = AuthLoc.Z;
                VisualXf.SetLocation(SmoothLoc);
                bVisualMoved = !PrevVis.Equals(SmoothLoc, 0.025f);
            }

            CharList[i].PositionedTransform = VisualXf;
            CharList[i].bTransformDirty |= (bLocationChanged || bRotationChanged || bVisualMoved);

            // Siehe RTS.ActorSync.Interval: negativ = Wert aus dem Prozessor (Vorgabe 0 = jedes Bild).
            const float ActorSyncInterval = CalculateActorSyncInterval();
            const bool bNeedsActorSync = (CurrentTime - CharList[i].LastActorSyncTime) >= ActorSyncInterval;

            // Dieselbe Ausnahme wie im Server-Zweig - der Client zeichnet dieselben Ringe,
            // Lebensbalken und angehefteten Effekte und braucht deshalb dieselbe Regel.
            const bool bNeedsSyncEveryFrame =
                UnitBase->bUseSkeletalMovement || UnitBase->HasActiveAttachedEffect()
                || IsCameraUnit(UnitBase);
            if (bNeedsSyncEveryFrame) { ++ChunkExemptCount; }

            if (bLocationChanged || bRotationChanged || bVisualMoved)
            {
                if (bNeedsSyncEveryFrame || bNeedsActorSync)
                {
                    PendingActorUpdates.Emplace(Actor, VisualXf, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
                    if (!bNeedsSyncEveryFrame)
                    {
                        CharList[i].LastActorSyncTime = CurrentTime;
                    }
                }
            }
        }
    });

    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}

void UActorTransformSyncProcessor::ExecuteRepClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float FrameDeltaTime = Context.GetDeltaTimeSeconds();
    float ActualDeltaTime = 0.0f;
    if (!ShouldProceedWithTick(FrameDeltaTime, ActualDeltaTime)) return;

    const int32 TotalMatchingEntities = ClientEntityQuery.GetNumMatchingEntities();
    //UE_LOG(LogTemp, Warning, TEXT("[Client] UActorTransformSyncProcessor Matching=%d"), TotalMatchingEntities);
    if (TotalMatchingEntities == 0)
    {
        return;
    }

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(TotalMatchingEntities);

    ClientEntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, ActualDeltaTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        //TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
            const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
            TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassRepresentationLODFragment> LODFragments = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();

        // Tags einmal je Chunk statt je Einheit - siehe GetChunkComposition in UnitMassTag.h.

        // Innerhalb eines Chunks teilen sich alle Entitaeten denselben Archetyp und damit

        // dieselben Tags; die Abfrage je Einheit war reine Wiederholung.

        int32 ChunkUnitCount = 0;
        int32 ChunkExemptCount = 0;
        const FChunkTagLookup ChunkTags(EntityManager, ChunkContext);
        if (CVarRTS_VerifyChunkTags.GetValueOnAnyThread() != 0)
        {
        	VerifyChunkTags(EntityManager, ChunkContext, ChunkTags, {
        		FMassStateFrozenTag::StaticStruct(), FMassStateIdleTag::StaticStruct(),
        		FMassStateDeadTag::StaticStruct(), FMassStateAttackTag::StaticStruct(),
        		FMassStatePauseTag::StaticStruct(), FMassStateBuildTag::StaticStruct(),
        		FMassStateRepairTag::StaticStruct(), FMassStateResourceExtractionTag::StaticStruct(),
        		FMassUnitYawFollowTag::StaticStruct(), FMassRotateToMouseTag::StaticStruct() });
        }

        const bool bChunk_MassStateDeadTag = ChunkTags.Has(FMassStateDeadTag::StaticStruct());

        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Only update if visible
            if (LODFragments[i].LOD == EMassLOD::Off) continue;

            // [TagKostenDiag] Siehe RTS.ChunkTags.MessAlt.
            if (CVarRTS_MeasureTagCost.GetValueOnAnyThread() != 0)
            {
                static double TC_Sum = 0.0;
                static int32 TK_Bilder = 0;
                static double TC_LastReport = 0.0;
                static int32 TK_Entitaeten = 0;

                const FMassEntityHandle TK_Entity = ChunkContext.GetEntity(i);
                const double TC_Start = FPlatformTime::Seconds();
                volatile int32 TK_Senke = 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateFrozenTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateIdleTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateDeadTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateAttackTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStatePauseTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateBuildTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateRepairTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateResourceExtractionTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassUnitYawFollowTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassRotateToMouseTag::StaticStruct()) ? 1 : 0;
                TC_Sum += FPlatformTime::Seconds() - TC_Start;
                ++TK_Entitaeten;

                const double TC_Now = FPlatformTime::Seconds();
                if (TC_Now - TC_LastReport > 5.0)
                {
                    TC_LastReport = TC_Now;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[TagKostenDiag] die ALTE Abfrage je Einheit: %.3f ms gesamt in 5 s, %d Entitaeten -> %.4f ms je 510 Einheiten"),
                        TC_Sum * 1000.0, TK_Entitaeten,
                        TK_Entitaeten > 0 ? (TC_Sum * 1000.0 / TK_Entitaeten) * 510.0 : 0.0);
                    TC_Sum = 0.0; TK_Entitaeten = 0;
                }
            }

            // GetMassActorLocation. 510 ueber den Speicher verstreute Aktorobjekte; genau das
            // verhindert Parallelitaet. Sitzt hier die Zeit, ist Aufteilen der Weg.
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;
        
            const FTransform& ReplicatedTransform = TransformFragments[i].GetTransform();

            // Adjust with ground/height logic on client too
            FTransform LocalTransform = ReplicatedTransform;
            FVector FinalLocation = LocalTransform.GetLocation();
            const FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const bool bIsDead = bChunk_MassStateDeadTag;

            if (bIsDead)
            {
                FinalLocation.X = CurrentActorLocation.X;
                FinalLocation.Y = CurrentActorLocation.Y;
            }

            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, LocalTransform, FinalLocation, bIsDead);

            LocalTransform.SetLocation(FinalLocation);
            PendingActorUpdates.Emplace(Actor, LocalTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
        }
    });

    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}

void UActorTransformSyncProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Bildweite Zaehler. Muessen AUSSERHALB der Chunk-Schleife liegen: die Schleife laeuft je
    // Chunk, und ein Chunk kennt nur seine eigenen ~25 Entitaeten.
    int32 FrameUnitCount = 0;
    int32 FrameExemptCount = 0;   /*
    const float FrameDeltaTime = Context.GetDeltaTimeSeconds();
    
    float ActualDeltaTime = 0.0f;
    if (!ShouldProceedWithTick(FrameDeltaTime, ActualDeltaTime)) return;
    */
    float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    // Determine if we are on a client world. This will be used to guard our logs.
    const bool bIsClient = GetWorld() && GetWorld()->IsNetMode(NM_Client);
    
    const float CurrentTime = Context.GetWorld()->GetTimeSeconds();
    
    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    PendingActorUpdates.Reserve(EntityQuery.GetNumMatchingEntities());
    
    EntityQuery.ForEachEntityChunk(Context,
        [this, &FrameUnitCount, &FrameExemptCount, &EntityManager, ActualDeltaTime, CurrentTime, &PendingActorUpdates, bIsClient](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
            
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();

        // Tags einmal je Chunk statt je Einheit - siehe GetChunkComposition in UnitMassTag.h.

        // Innerhalb eines Chunks teilen sich alle Entitaeten denselben Archetyp und damit

        // dieselben Tags; die Abfrage je Einheit war reine Wiederholung.

        int32 ChunkUnitCount = 0;
        int32 ChunkExemptCount = 0;
        const FChunkTagLookup ChunkTags(EntityManager, ChunkContext);
        if (CVarRTS_VerifyChunkTags.GetValueOnAnyThread() != 0)
        {
        	VerifyChunkTags(EntityManager, ChunkContext, ChunkTags, {
        		FMassStateFrozenTag::StaticStruct(), FMassStateIdleTag::StaticStruct(),
        		FMassStateDeadTag::StaticStruct(), FMassStateAttackTag::StaticStruct(),
        		FMassStatePauseTag::StaticStruct(), FMassStateBuildTag::StaticStruct(),
        		FMassStateRepairTag::StaticStruct(), FMassStateResourceExtractionTag::StaticStruct(),
        		FMassUnitYawFollowTag::StaticStruct(), FMassRotateToMouseTag::StaticStruct() });
        }

        const bool bChunk_MassRotateToMouseTag = ChunkTags.Has(FMassRotateToMouseTag::StaticStruct());

        const bool bChunk_MassStateAttackTag = ChunkTags.Has(FMassStateAttackTag::StaticStruct());

        const bool bChunk_MassStateBuildTag = ChunkTags.Has(FMassStateBuildTag::StaticStruct());

        const bool bChunk_MassStateDeadTag = ChunkTags.Has(FMassStateDeadTag::StaticStruct());

        const bool bChunk_MassStateFrozenTag = ChunkTags.Has(FMassStateFrozenTag::StaticStruct());

        const bool bChunk_MassStateIdleTag = ChunkTags.Has(FMassStateIdleTag::StaticStruct());

        const bool bChunk_MassStatePauseTag = ChunkTags.Has(FMassStatePauseTag::StaticStruct());

        const bool bChunk_MassStateRepairTag = ChunkTags.Has(FMassStateRepairTag::StaticStruct());

        const bool bChunk_MassStateResourceExtractionTag = ChunkTags.Has(FMassStateResourceExtractionTag::StaticStruct());

        const bool bChunk_MassUnitYawFollowTag = ChunkTags.Has(FMassUnitYawFollowTag::StaticStruct());

        for (int32 i = 0; i < NumEntities; ++i)
        {
            
            // [TagKostenDiag] Siehe RTS.ChunkTags.MessAlt.
            if (CVarRTS_MeasureTagCost.GetValueOnAnyThread() != 0)
            {
                static double TC_Sum = 0.0;
                static int32 TK_Bilder = 0;
                static double TC_LastReport = 0.0;
                static int32 TK_Entitaeten = 0;

                const FMassEntityHandle TK_Entity = ChunkContext.GetEntity(i);
                const double TC_Start = FPlatformTime::Seconds();
                volatile int32 TK_Senke = 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateFrozenTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateIdleTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateDeadTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateAttackTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStatePauseTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateBuildTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateRepairTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassStateResourceExtractionTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassUnitYawFollowTag::StaticStruct()) ? 1 : 0;
                TK_Senke += DoesEntityHaveTag(EntityManager, TK_Entity, FMassRotateToMouseTag::StaticStruct()) ? 1 : 0;
                TC_Sum += FPlatformTime::Seconds() - TC_Start;
                ++TK_Entitaeten;

                const double TC_Now = FPlatformTime::Seconds();
                if (TC_Now - TC_LastReport > 5.0)
                {
                    TC_LastReport = TC_Now;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[TagKostenDiag] die ALTE Abfrage je Einheit: %.3f ms gesamt in 5 s, %d Entitaeten -> %.4f ms je 510 Einheiten"),
                        TC_Sum * 1000.0, TK_Entitaeten,
                        TK_Entitaeten > 0 ? (TC_Sum * 1000.0 / TK_Entitaeten) * 510.0 : 0.0);
                    TC_Sum = 0.0; TK_Entitaeten = 0;
                }
            }

            // GetMassActorLocation. 510 ueber den Speicher verstreute Aktorobjekte; genau das
            // verhindert Parallelitaet. Sitzt hier die Zeit, ist Aufteilen der Weg.
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (bChunk_MassStateFrozenTag) continue;

            const bool bIsIdle = bChunk_MassStateIdleTag;
            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            const FQuat CurrentRotation = Actor->GetActorRotation().Quaternion();
            FVector FinalLocation = MassTransform.GetLocation();
            
            // Determine the actor's current location once, as it's used by multiple functions
            FVector CurrentActorLocation = UnitBase->GetMassActorLocation();

            const bool bIsDead = bChunk_MassStateDeadTag;

            if (bIsDead)
            {
                FinalLocation.X = CurrentActorLocation.X;
                FinalLocation.Y = CurrentActorLocation.Y;
            }

            // 1. Adjust rotation based on state (moving vs. attacking vs. working)
            const bool bIsBuilding = bChunk_MassStateBuildTag;
            const bool bIsRepairing = bChunk_MassStateRepairTag;
            const bool bIsExtracting = bChunk_MassStateResourceExtractionTag;
            const bool bIsAttackingOrPaused = bChunk_MassStateAttackTag ||
                                              bChunk_MassStatePauseTag ||
                                              bIsBuilding || bIsRepairing || bIsExtracting;

            const bool bIsIdleHold = bChunk_MassStateIdleTag && StateList[i].HoldPosition;
            const bool bHasValidTarget = TargetList[i].bHasValidTarget && EntityManager.IsEntityActive(TargetList[i].TargetEntity);
            const bool bShouldRotateToTarget = bIsAttackingOrPaused || (bIsIdleHold && bHasValidTarget);

            const bool bRotatesToMovementWhileAttacking = StatsList[i].bCanMoveWhileAttacking && StatsList[i].bRotatesToMovementIfMoveWhileAttacking;
            const bool bIsMoving = !VelocityList[i].Value.IsNearlyZero(50.f);

            const bool bIsYawFollowing = bChunk_MassUnitYawFollowTag;
            
            const bool bIsWorking = bIsBuilding || bIsRepairing || bIsExtracting;

            // ============================================================================
            // LUX-ANPASSUNG (16.08.2026) â€” beim Zielen gehoert der Yaw der Maus.
            // Vor der Aenderung war eine zielende Einheit komplett aus diesem Prozessor
            // ausgeschlossen (Tag-Sperre, siehe ConfigureQueries). Jetzt laeuft sie hier
            // mit, damit die POSITION weiter synchronisiert wird - die ROTATION muss aber
            // beim UMassRotateToMouseProcessor bleiben, sonst dreht sich die Einheit in
            // Bewegungsrichtung statt in Schussrichtung.
            // Greift nur bei der direkt gesteuerten CameraUnit: jede andere Einheit traegt
            // zusaetzlich FMassStopWhileAimingTag und ist weiter ganz ausgeschlossen.
            // Original: if (bIsDead || (bIsYawFollowing && !bIsWorking))
            // ============================================================================
            const bool bLuxAimingAtMouse = bChunk_MassRotateToMouseTag;

            if (bIsDead || (bIsYawFollowing && !bIsWorking) || bLuxAimingAtMouse)
            {
                // Regular rotation updates are skipped for dead units (Death spin is handled in HandleGroundAndHeight)
                // or while UUnitRotateToTargetProcessor owns the yaw (FMassUnitYawFollowTag).
                // Exception: WORKING units — that processor only reads the enemy target slot and its
                // query excludes the work states, so work-target facing below must run here.
            }
            else if (TargetList[i].bRotateTowardsAbility)
            {
                // Pass the consolidated AI Target fragment.
                const bool bReached = RotateTowardsAbility(UnitBase, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
                if (bReached)
                {
                    TargetList[i].bRotateTowardsAbility = false;
                }
            }
            // LUX-ANPASSUNG (17.08.2026) - waehrend eines Casts darf die LAUFRICHTUNG die Drehung
            // nicht uebernehmen. GEMESSEN: der Client sprang beim Casten im Laufen um bis zu 150 Grad
            // hin und her (ClientYaw 67.4 / -87.3 / 67.4 / -20.8 ...), obwohl die Replikationskorrektur
            // bereits aus war - es waren also zwei LOKALE Quellen. Der Zweig darueber
            // (bRotateTowardsAbility) loescht sein Flag, sobald die Drehung "erreicht" ist,
            // CastingStateProcessor setzt es im naechsten Takt wieder - und genau in den Frames
            // dazwischen drehte dieser Zweig hierher auf die Laufrichtung.
            // Die erste Haelfte der Bedingung schliesst Casting schon aus; der zweiten
            // (bRotatesToMovementWhileAttacking) fehlte dieselbe Ausnahme.
            else if ((!bShouldRotateToTarget && UnitBase->GetUnitState() != UnitData::Casting && !bIsIdle) || (bRotatesToMovementWhileAttacking && bIsMoving && UnitBase->GetUnitState() != UnitData::Casting))
            {
                RotateTowardsMovement(UnitBase, VelocityList[i].Value, StatsList[i], CharList[i], StateList[i], CurrentActorLocation, ActualDeltaTime, MassTransform);
            }
            else
            {
                // Working workers face their actual work target (BuildArea / ResourcePlace / repaired unit)
                bool bRotatedToWorkTarget = false;
                if (bIsBuilding || bIsRepairing || bIsExtracting)
                {
                    bRotatedToWorkTarget = RotateYawTowardsWorkTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i],
                        CurrentActorLocation, ActualDeltaTime, bIsBuilding, bIsRepairing, bIsExtracting, MassTransform);
                }

                // No fallback for extraction: without a ResourcePlace pointer, holding the current yaw
                // beats turning toward an unrelated friendly target.
                if (!bRotatedToWorkTarget && !bIsExtracting)
                {
                    const bool bPreferEnemy = bChunk_MassStateAttackTag ||
                                              bChunk_MassStatePauseTag;
                    RotateTowardsTarget(UnitBase, EntityManager, TargetList[i], StatsList[i], CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, bPreferEnemy);
                }
            }

            // 2. Adjust height for ground snapping or flying
            //
            // DIESER AUFRUF HAT GEFEHLT (wiederhergestellt 18.09.2026) - zum ZWEITEN Mal derselbe
            // Fehler: er stand im Rumpf des [BodenDiag]-Blocks, der ihn gemessen hat, und meine
            // klammerzaehlende Entfernung der Diagnose hat ihn mitgenommen. Vorher war schon
            // DispatchPendingUpdates auf dieselbe Weise verschwunden.
            //
            // WIE SICH DAS GEZEIGT HAT: Einheiten, die in der Luft platziert wurden, fielen nicht
            // mehr auf den Boden; niemand lief mehr Rampen hoch; Flugeinheiten fuehrten ihre Hoehe
            // nicht mehr ueber dem Gelaende nach. Ohne diesen Aufruf gibt es auf dem Server
            // ueberhaupt keine Hoehenanpassung.
            HandleGroundAndHeight(UnitBase, CharList[i], CurrentActorLocation, ActualDeltaTime, MassTransform, FinalLocation, bIsDead);

            // 3. Apply final location and cache the result
            MassTransform.SetLocation(FinalLocation);

            // ISM-FIX: GetMassActorLocation() liefert fuer ISM-Units (bUseSkeletalMovement==false) den
            // LIVE-FTransformFragment-Wert zurueck (== FinalLocation.XY, da HandleGroundAndHeight nur Z
            // veraendert). CurrentActorLocation kann horizontale Bewegung dadurch NIE erkennen ->
            // bLocationChanged bleibt false -> bTransformDirty bleibt false -> der PlacementProcessor
            // ueberspringt die ISM-Instanz (Skip-Gate) -> "Teleportieren" auf dem Server. Wie der Client
            // (bVisualMoved) gegen die zuletzt gerenderte PositionedTransform vergleichen, BEVOR sie
            // ueberschrieben wird. (SKM ist nicht betroffen: dort liefert GetMassActorLocation() die um
            // einen Frame nachhinkende Actor-Position, und SKM rendert ohnehin ueber den Actor.)
            const FVector PrevVisualLocation = CharList[i].PositionedTransform.GetLocation();
            const bool bLocationChanged = !PrevVisualLocation.Equals(FinalLocation, 0.025f);
            const bool bRotationChanged = !CurrentRotation.Equals(MassTransform.GetRotation(), 0.0001f);

            CharList[i].PositionedTransform = MassTransform;
            CharList[i].bTransformDirty |= (bLocationChanged || bRotationChanged);

            // 4. Queue an update to be performed on the game thread if the transform has changed
         
            if (!bIsClient)
            {
                //UE_LOG(LogTemp, Error, TEXT("Server FinalLocation %s"), *FinalLocation.ToString());
            }

            // Siehe RTS.ActorSync.Interval: negativ = Wert aus dem Prozessor (Vorgabe 0 = jedes Bild).
            const float ActorSyncInterval = CalculateActorSyncInterval();
            const bool bNeedsActorSync = (CurrentTime - CharList[i].LastActorSyncTime) >= ActorSyncInterval;

            // Siehe die ausfuehrliche Begruendung im Server-Zweig weiter oben: Skelett-Einheiten
            // zeichnen sich ueber den Aktor, angeheftete Niagara-Effekte werden vom Aktor
            // mitgezogen - beide duerfen nicht gedrosselt werden. Dazu die direkt gesteuerte
            // Einheit des Spielers.
            const bool bNeedsSyncEveryFrame =
                UnitBase->bUseSkeletalMovement || UnitBase->HasActiveAttachedEffect()
                || IsCameraUnit(UnitBase);

            // [SkmDiag] Siehe RTS.SkmDiag.
            if (UnitBase->bUseSkeletalMovement && CVarRTS_SkmDiag.GetValueOnAnyThread() != 0)
            {
                static double SD_LastReport = 0.0;
                const double SD_Now = FPlatformTime::Seconds();
                if (SD_Now - SD_LastReport > 1.0)
                {
                    SD_LastReport = SD_Now;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[SkmDiag] %s | MassZiel=%s | Vorher=%s | Aktor=%s | LocChanged=%d RotChanged=%d JedesBild=%d NeedsSync=%d Intervall=%.3f"),
                        *GetNameSafe(UnitBase), *FinalLocation.ToCompactString(),
                        *PrevVisualLocation.ToCompactString(), *CurrentActorLocation.ToCompactString(),
                        bLocationChanged ? 1 : 0, bRotationChanged ? 1 : 0,
                        bNeedsSyncEveryFrame ? 1 : 0, bNeedsActorSync ? 1 : 0, ActorSyncInterval);
                }
            }

            if (bLocationChanged || bRotationChanged)
            {
                if (bNeedsSyncEveryFrame || bNeedsActorSync)
                {
                    PendingActorUpdates.Emplace(Actor, MassTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
                    if (!bNeedsSyncEveryFrame)
                    {
                        CharList[i].LastActorSyncTime = CurrentTime;
                    }
                }
            }
            ++ChunkUnitCount;
        }

        // AUFSUMMIEREN, NICHT MAXIMUM BILDEN.
        //
        // Hier stand FMath::Max(LastFrameUnitCount, ChunkUnitCount). Das war falsch: die
        // Schleife laeuft JE CHUNK, und ein Chunk fasst rund 25 Entitaeten. Das Maximum ueber die
        // Chunks ist also die groesste CHUNKGROESSE, nicht die Armeegroesse - gemessen 25 statt
        // 510. Damit lag der Wert dauerhaft unter ActorSyncScaleStartUnits (150), die Kurve gab
        // Intervall 0,000 s zurueck und die Drosselung war nie aktiv. Die Messung sah aus wie
        // "die Drosselung bringt nichts", obwohl sie schlicht nicht lief.
        FrameUnitCount += ChunkUnitCount;
        FrameExemptCount += ChunkExemptCount;
    });

    // Erst JETZT steht die Armeegroesse fest - sie ist die Grundlage der Taktung im naechsten Bild.
    LastFrameUnitCount = FrameUnitCount;

    // DIESER AUFRUF HAT GEFEHLT (wiederhergestellt 18.09.2026).
    //
    // Er stand im Rumpf des [DispatchDiag]-Blocks, der ihn gemessen hat. Beim Abbau der Diagnose
    // hat meine klammerzaehlende Entfernung den GANZEN Block genommen - samt Aufruf. Der Compiler
    // konnte das nicht merken, es war weiterhin gueltiger Code.
    //
    // WIE SICH DAS GEZEIGT HAT: nur Einheiten mit bUseSkeletalMovement == true blieben stehen.
    // ISM-Einheiten werden ueber die ISM-Instanz gezeichnet, die UMassUnitPlacementProcessor
    // unabhaengig davon aktualisiert - ihr Aktor durfte also stehenbleiben, ohne dass es auffiel.
    // Bei SKM IST der Aktor das Sichtbare.
    //
    // WAS DIE URSACHE BELEGT HAT: die Zeile [SkmDiag] zeigte MassZiel und Vorher sauber wandern,
    // LocChanged=1, NeedsSync=1 - und den Aktor unveraendert auf der Startposition. Damit war
    // ausgeschlossen, dass es an der Mass-Simulation, an der Drosselung oder am Befehlspfad lag.
    //
    // LEHRE: eine Messung, die ihren Messgegenstand UMSCHLIESST, darf nicht als Block entfernt
    // werden. Vor dem Abbau pruefen, ob im Rumpf echte Arbeit steht.
    DispatchPendingUpdates(MoveTemp(PendingActorUpdates));
}