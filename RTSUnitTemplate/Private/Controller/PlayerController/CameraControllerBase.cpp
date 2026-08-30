// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Characters/Unit/UnitBase.h" // Include UnitBase for the RPC
#include "Widgets/WinLoseWidget.h"
#include "Widgets/LoadingWidget.h"
#include "GameModes/RTSGameModeBase.h"
#include "GameStates/ResourceGameState.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h" // LUX-ANPASSUNG (16.08.2026): FMassVelocityFragment fuers Auslaufen
#include "MassEntityTypes.h"
#include "Mass/UnitMassTag.h"
#include "GAS/GameplayAbilityBase.h"
#include "Mass/Signals/MySignals.h" // LUX-ANPASSUNG (16.08.2026): UnitSignals::Run fuer die lokale Vorhersage
#include "Steering/MassSteeringFragments.h" // FMassSteeringFragment fuer die Startdiagnose
#include "Mass/UnitNavigationFragments.h" // DIAGNOSE (17.08.2026): Pfadstatus beim Losdruecken


void ACameraControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}


bool ACameraControllerBase::Server_UpdateCameraUnitMovement_Validate(const FVector& TargetLocation)
{
	return CameraUnitWithTag != nullptr;
}

// ================================================================================================
// LUX-ANPASSUNG 1/3 â€” harter Stopp fuer die Direktsteuerung (16.08.2026)
// Wird beim Loslassen der WASD-Taste gerufen. Server_UpdateCameraUnitMovement taugt dafuer
// nicht: es ruft UpdateMoveTarget mit voller BaseRunSpeed auf die aktuelle Position auf und
// setzt den Stopp-Tag nur verzoegert - die Einheit rollte dadurch sichtbar aus (Silvan:
// "beim Antippen laeuft er mind. 150"). StopMovement() setzt dagegen sofort
// EMassMovementAction::Stand und DesiredSpeed 0.
// ================================================================================================
void ACameraControllerBase::Server_StopCameraUnitDirect_Implementation()
{
	if (!CameraUnitWithTag) return;

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (!CameraUnitWithTag->GetMassEntityData(EntityManager, EntityHandle) || !EntityManager)
	{
		return;
	}

	const FVector StopLoc = CameraUnitWithTag->GetMassActorLocation();

	// Direktsteuerung beendet -> Tag weg, ab jetzt gilt wieder die normale Pfadsuche.
	//
	// MUSS hier oben stehen, nicht am Ende: der Auslauf-Zweig weiter unten kehrt vorzeitig
	// zurueck (return im Glide-Fall), damit blieb der Tag nach JEDER normalen Bewegung gesetzt.
	// Folge: der Bewegungsprozessor lenkt die Einheit weiter auf den Auslaufpunkt, ueberschiesst
	// ihn und pendelt darum - genau das gemeldete Wackeln im Stillstand auf dem Server.
	// Das Auslaufen selbst braucht den Tag nicht: sein Ziel steht FEST, eine einmalige Pfadsuche
	// stoert also nicht. Der Tag existiert nur gegen das staendig mitwandernde WASD-Ziel.
	if (!EntityManager->IsProcessing()
		&& DoesEntityHaveTag(*EntityManager, EntityHandle, FMassDirectControlTag::StaticStruct()))
	{
		EntityManager->Defer().RemoveTag<FMassDirectControlTag>(EntityHandle);
		EntityManager->FlushCommands();
	}

	// ============================================================================================
	// LUX-ANPASSUNG (16.08.2026) â€” kleines Auslaufen statt Vollbremsung.
	// Silvan: "Jetzt sollten wir der Einheit etwas Moment geben bevor sie stoppt."
	// Statt sofort auf Tempo 0 zu gehen, bekommt sie ein Ziel ein kurzes Stueck in der
	// aktuellen Laufrichtung. Die Ankunftslogik von Mass bremst sie dorthin aus - das ergibt
	// die Traegheit, ohne eine eigene Physik-Kraft einzufuehren.
	// Richtung kommt aus dem Velocity-Fragment (AActor::GetVelocity() ist bei Mass-Einheiten
	// immer null). Steht die Einheit schon, faellt es auf den harten Stopp zurueck.
	// Ueber UnitDirectStopGlide auf 0 gesetzt = altes Verhalten.
	// ============================================================================================
	if (HasAuthority() && UnitDirectStopGlide > 1.f)
	{
		FVector GlideDir = FVector::ZeroVector;
		if (const FMassVelocityFragment* VelFrag =
			EntityManager->GetFragmentDataPtr<FMassVelocityFragment>(EntityHandle))
		{
			GlideDir = VelFrag->Value;
			GlideDir.Z = 0.f;
			GlideDir = GlideDir.GetSafeNormal();
		}

		if (!GlideDir.IsNearlyZero() && CameraUnitWithTag->Attributes)
		{
			if (FMassMoveTargetFragment* MoveTargetFrag =
				EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
			{
				const FVector GlideTarget = StopLoc + GlideDir * UnitDirectStopGlide;

				::UpdateMoveTarget(*MoveTargetFrag, GlideTarget,
					CameraUnitWithTag->Attributes->GetBaseRunSpeed(), GetWorld());

				// StoredLocation MUSS das Auslaufziel sein, sonst zieht die Idle-Regel
				// "laufe zurueck zu StoredLocation" die Einheit hinterher wieder zurueck.
				if (FMassAIStateFragment* AiStateFrag =
					EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
				{
					AiStateFrag->StoredLocation = GlideTarget;
				}

				// Bewusst KEIN AddStopMovementTagToEntity(): der Tag wuerde die Einheit
				// sofort einfrieren und das Auslaufen wieder zunichtemachen.
				return;
			}
		}
	}
	// ===================== ENDE LUX-ANPASSUNG ===================================================

	if (HasAuthority())
	{
		if (FMassMoveTargetFragment* MoveTargetFrag =
			EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
		{
			// ':: ' ist noetig: AController hat eine eigene StopMovement()-Methode, die die
			// freie Funktion aus UnitMassTag.h sonst verdeckt (C2660).
			::StopMovement(*MoveTargetFrag, GetWorld());

			// StopMovement() setzt nur Stand + Speed 0 und laesst Center bewusst stehen.
			// Center ist hier aber noch der Vorhalte-Punkt (UnitDirectMoveLookAhead voraus),
			// den Server_UpdateCameraUnitMovement gesetzt hat - also mitziehen.
			MoveTargetFrag->Center = StopLoc;
		}

		// Silvan: "Wenn der Character stoppt dann stoppt er nur kurz und bewegt sich danach
		// wieder ein Stueck." Ursache: der IdleStateProcessor hat eine generische Regel
		// "laufe zurueck zu StoredLocation" (IdleStateProcessor.cpp ~Z.405). StoredLocation
		// wird von Server_UpdateCameraUnitMovement auf den Vorhalte-Punkt gesetzt
		// (CustomControllerBase.cpp ~Z.965). Nach dem harten Stopp geht die Einheit auf Idle,
		// findet dort den alten Punkt weit genug entfernt - und laeuft den Rest ab.
		// Deshalb den gespeicherten Ort ebenfalls auf die aktuelle Position ziehen.
		if (FMassAIStateFragment* AiStateFrag =
			EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
		{
			AiStateFrag->StoredLocation = StopLoc;
		}
	}
	else if (FMassClientPredictionFragment* PredFrag =
		EntityManager->GetFragmentDataPtr<FMassClientPredictionFragment>(EntityHandle))
	{
		// Client-Prediction ebenfalls stoppen, sonst zuckt die Einheit zurueck.
		PredFrag->Location = StopLoc;
		PredFrag->PredDesiredSpeed = 0.f;
		PredFrag->bHasData = true;
	}

	CameraUnitWithTag->AddStopMovementTagToEntity();
}
// ===================== ENDE LUX-ANPASSUNG 1/3 ===================================================

// ================================================================================================
// LUX-ANPASSUNG (28.08.2026) - Klick beim Zielen gehoert der zielenden Faehigkeit.
// Silvan: "Wenn AbilityIndicator aktiviert ist, soll der naechste Klick nicht wieder eine Ability
// aktivieren sondern in der gleichen Ability den ClickCounter erhoehen."
//
// Warum das nur die CameraUnit betrifft: AControllerBase::LeftClickSelect fragt vor dem Selektieren
// IsAnyAbilityActive() ab und schickt den Klick dann als FireAbilityMouseHit an die laufende
// Faehigkeit. Die Direktsteuerung geht an dieser Routine vorbei - ihr Linksklick landet direkt in
// ExecuteOnAbilityInputDetected(AbilityOne) - und startete deshalb den Schuss, statt das Wurfziel
// zu bestaetigen. Diese Funktion holt den vorhandenen Zweig fuer den Direktsteuerungs-Pfad nach.
//
// Bewusst NICHT fuer jede laufende Faehigkeit: das Flag bIndicatorClicksAdvanceAbility an der
// Faehigkeit mit dem Indikator entscheidet, sonst verloere jede beliebige laufende Faehigkeit den
// Schuss-Klick.
// ================================================================================================
bool ACameraControllerBase::LuxTryAdvanceIndicatorAbilityWithClick()
{
	if (!CurrentDraggedAbilityIndicator || !CameraUnitWithTag)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[IndikatorKlick] nicht uebernommen: Indikator=%d CameraUnit=%d"),
			CurrentDraggedAbilityIndicator ? 1 : 0, CameraUnitWithTag ? 1 : 0);
		return false;
	}

	// Auf dem Client existiert die Instanz nicht - dort traegt der replizierte Snapshot die Klasse.
	const UGameplayAbilityBase* Running = CameraUnitWithTag->ActivatedAbilityInstance
		? CameraUnitWithTag->ActivatedAbilityInstance
		: (CameraUnitWithTag->CurrentSnapshot.AbilityClass
			? CameraUnitWithTag->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>()
			: nullptr);

	if (!Running || !Running->bIndicatorClicksAdvanceAbility || !Running->AbilityIndicatorClass)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[IndikatorKlick] nicht uebernommen: Faehigkeit=%s Flag=%d IndikatorKlasse=%d"),
			Running ? *Running->GetClass()->GetName() : TEXT("keine"),
			Running && Running->bIndicatorClicksAdvanceAbility ? 1 : 0,
			Running && Running->AbilityIndicatorClass ? 1 : 0);
		return false;
	}

	// Dieselbe Drossel wie in LeftClickSelect. Der Klick gilt trotzdem als verbraucht: waehrend der
	// Sperrzeit darf er erst recht nicht stattdessen den Schuss ausloesen.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - CameraUnitWithTag->LastMouseHitRequestTime < CameraUnitWithTag->AbilityReactivationThrottle)
	{
		return true;
	}
	CameraUnitWithTag->LastMouseHitRequestTime = Now;

	// Zielpunkt: erst der Bodenstrahl, den auch der Indikator nutzt (MoveAbilityIndicator_Local),
	// sonst der Indikator selbst. GetHitResultUnderCursor taugt hier NICHT - in der
	// Direktsteuerung liefert es keinen Treffer (gemessen: Maustreffer=0, Ziel (0,0,0)), die
	// Faehigkeit haette also ins Nichts geworfen. Der Indikator steht ohnehin genau dort, wohin
	// der Spieler zielt; damit landet der Wurf sichtbar dort, wo der Ring liegt.
	FHitResult Hit;
	FVector Bodenpunkt;
	if (!TraceMouseToGround(Bodenpunkt, Hit) || !Hit.bBlockingHit)
	{
		Hit = FHitResult();
		Hit.bBlockingHit = true;
		Hit.Location = CurrentDraggedAbilityIndicator->GetActorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.TraceStart = Hit.Location;
		Hit.TraceEnd = Hit.Location;
	}

	UE_LOG(LogTemp, Log, TEXT("[IndikatorKlick] weitergeleitet an %s, ClickCount vorher=%d, Ziel=%s"),
		*Running->GetClass()->GetName(),
		CameraUnitWithTag->ActivatedAbilityInstance ? CameraUnitWithTag->ActivatedAbilityInstance->ClickCount : -1,
		*Hit.ImpactPoint.ToCompactString());

	FireAbilityMouseHit(CameraUnitWithTag, Hit);
	return true;
}
// ===================== ENDE LUX-ANPASSUNG =======================================================

// ================================================================================================
// LUX-ANPASSUNG 5/5 - lokale Vorhersage fuer die WASD-Direktsteuerung (16.08.2026)
// Setzt auf dem steuernden Client dasselbe FMassClientPredictionFragment, das auch der
// Rechtsklick-Befehl setzt (ApplyMovePredictionToUnit). Der UnitMovementProcessor bewegt die
// Einheit auf dem Client dann sofort auf Pred.Location zu, statt auf die replizierte
// Server-Position zu warten. Der Server bleibt autoritativ - die Reconciliation zieht die
// Einheit weiterhin auf die Serverposition, was hier nur eine kleine Korrektur ist, weil
// beide dasselbe Ziel mit derselben Geschwindigkeit anlaufen.
// bStopping: beim Loslassen wird das Auslaufziel vorhergesagt (dieselbe Strecke, die der
// Server in Server_StopCameraUnitDirect nimmt), damit Client und Server gleich ausrollen.
// ================================================================================================
// Schalter zum Gegenmessen/Abschalten: rts.lux.directpredict 0 = alte Fassung (nur Server-RPC).
// Zur Laufzeit in der Konsole umschaltbar, damit man den Unterschied direkt vergleichen kann.
static TAutoConsoleVariable<int32> CVarLuxDirectPredict(
	TEXT("rts.lux.directpredict"),
	1,
	TEXT("1 = WASD-Direktsteuerung sagt auf dem Client lokal vorher (responsiv), 0 = nur Server-RPC."),
	ECVF_Default);

void ACameraControllerBase::ApplyDirectMovePredictionLocally(const FVector& Target, bool bStopping, bool bStartingMove)
{
	if (CVarLuxDirectPredict.GetValueOnGameThread() == 0) return;

	if (!CameraUnitWithTag || !CameraUnitWithTag->Attributes) return;

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (!CameraUnitWithTag->GetMassEntityData(EntityManager, EntityHandle) || !EntityManager) return;
	if (!EntityManager->IsEntityValid(EntityHandle)) return;

	FMassClientPredictionFragment* Pred =
		EntityManager->GetFragmentDataPtr<FMassClientPredictionFragment>(EntityHandle);
	if (!Pred) return;

	UWorld* World = GetWorld();

	if (FMassAIStateFragment* AiState = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
	{
		// Ohne das ueberspringen mehrere Client-Prozessoren die Einheit ("if (SwitchingState) continue;")
		// und die Vorhersage wuerde erst einen Tick spaeter greifen.
		AiState->SwitchingState = false;
		AiState->SwitchingStateClient = false;
		// StoredLocation mitziehen: die Idle-Regel "laufe zurueck zu StoredLocation" wuerde die
		// Einheit sonst nach dem Anhalten wieder an den alten Punkt ziehen.
		AiState->StoredLocation = Target;
	}

	// Die Ankunftslogik im UnitMovementProcessor braucht eine Geschwindigkeit > 0, auch beim
	// Ausrollen - gestoppt wird ueber das Erreichen des (nahen) Auslaufziels, nicht ueber Tempo 0.
	Pred->Location = Target;
	Pred->PredDesiredSpeed = CameraUnitWithTag->Attributes->GetBaseRunSpeed();
	Pred->PredAcceptanceRadius = CameraUnitWithTag->MovementAcceptanceRadius > 0.f
		? CameraUnitWithTag->MovementAcceptanceRadius
		: 50.f;
	Pred->bHasData = true;
	Pred->CommandPredictTime = World ? World->GetTimeSeconds() : 0.f;

	// ============================================================================================
	// LUX-ANPASSUNG 6/6 (17.08.2026) - Animationstempo: NICHT hier schreiben.
	//
	// Erster Versuch war, FMassVelocityFragment in der Vorhersage selbst zu setzen, weil der AnimBP
	// sein Tempo daraus liest (UnitBaseAnimInstance -> MassSpeed) und es auf dem Client 0 blieb.
	// Das war falsch: das Fragment ist zugleich der INTEGRATIONSZUSTAND der Bewegung.
	// UUnitApplyMassMovementProcessor beschleunigt von Velocity in Richtung DesiredVelocity - wer
	// Velocity jeden Frame ueberschreibt, setzt diesen Aufbau staendig zurueck. Gemessen: Lenkung
	// konstant 800, tatsaechliche Geschwindigkeit pendelnd 290-518, in 0,22 s nur 74 statt ~176
	// Einheiten Weg. Genau die gemeldete Traegheit auf dem Client.
	//
	// Die eigentliche Ursache der 0 war ein FEHLENDER Zustands-Tag: ohne FMassStateRunTag
	// ueberspringt der Applier die Entity und aktualisiert ihre Velocity gar nicht. Der Tag wird
	// unten gesetzt - damit pflegt der Applier das Fragment selbst, und die Animation stimmt ohne
	// jeden Eingriff von hier.
	// ============================================================================================

	// ============================================================================================
	// DIAGNOSE (17.08.2026) - wo genau entsteht die Traegheit beim Losdruecken?
	// Silvan: "Beim loslaufen ist es immernoch sehr traege auf dem Client".
	// Kandidaten sind Pfadsuche (der Prozessor setzt DesiredVelocity waehrenddessen auf NULL),
	// eine Bewegungssperre, oder ein Zustand, in dem der Client-Mover die Einheit ueberspringt.
	// Nur die erste Sekunde je Bewegung, nur Client - danach still.
	// ============================================================================================
	if (bStartingMove)
	{
		DiagnoseStartZeit = World ? World->GetTimeSeconds() : 0.f;
		DiagnoseStartOrt = CameraUnitWithTag->GetMassActorLocation();
	}
	if (!bStopping && DiagnoseStartZeit > 0.f && World && World->GetTimeSeconds() - DiagnoseStartZeit < 1.0f)
	{
		const FMassSteeringFragment* St = EntityManager->GetFragmentDataPtr<FMassSteeringFragment>(EntityHandle);
		const FMassVelocityFragment* Ve = EntityManager->GetFragmentDataPtr<FMassVelocityFragment>(EntityHandle);
		const FUnitNavigationPathFragment* Pf = EntityManager->GetFragmentDataPtr<FUnitNavigationPathFragment>(EntityHandle);
		const FMassAIStateFragment* Ai = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle);
		UE_LOG(LogTemp, Warning,
			TEXT("[StartDiag] t=%.3f Strecke=%.0f Steer=%.0f Vel=%.0f Pfadsuche=%d Pfad=%d RunTag=%d Sperre=%d CanMove=%d Zustand=%d"),
			World->GetTimeSeconds() - DiagnoseStartZeit,
			FVector::Dist2D(CameraUnitWithTag->GetMassActorLocation(), DiagnoseStartOrt),
			St ? St->DesiredVelocity.Size2D() : -1.f,
			Ve ? Ve->Value.Size2D() : -1.f,
			Pf ? (int32)Pf->bIsPathfindingInProgress : -1,
			Pf ? (int32)Pf->HasValidPath() : -1,
			(int32)DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStateRunTag::StaticStruct()),
			(int32)DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStopWhileAimingTag::StaticStruct()),
			Ai ? (int32)Ai->CanMove : -1,
			(int32)CameraUnitWithTag->GetUnitState());
	}

	// ============================================================================================
	// Direktsteuerungs-Tag auch auf dem CLIENT fuehren - Gegenstueck zum Server.
	//
	// Diese Zeilen waren beim Entfernen des Velocity-Blocks versehentlich mit herausgefallen; im
	// Log stand danach wieder "Pfadsuche=1 Steer=0" und [ApplierDiag] blieb stumm. Ohne den Tag
	// sucht der Client bei jedem Takt einen neuen Pfad (das WASD-Ziel wandert mit der Einheit
	// mit) und der Prozessor setzt waehrend der Suche die Lenkung auf null - das ist die
	// Traegheit beim Losllaufen und zugleich der Grund fuer Tempo 0 in der Animation.
	// Beim Anhalten wieder entfernen: sonst wuerde ein spaeterer Rechtsklick-Befehl die Einheit
	// geradeaus schicken statt um Hindernisse herum.
	// ============================================================================================
	if (!EntityManager->IsProcessing())
	{
		const bool bHatTag =
			DoesEntityHaveTag(*EntityManager, EntityHandle, FMassDirectControlTag::StaticStruct());
		if (bStopping && bHatTag)
		{
			EntityManager->Defer().RemoveTag<FMassDirectControlTag>(EntityHandle);
			EntityManager->FlushCommands();
		}
		else if (bStartingMove && !bHatTag)
		{
			EntityManager->Defer().AddTag<FMassDirectControlTag>(EntityHandle);
			EntityManager->FlushCommands();
		}
	}

	if (!bStartingMove)
	{
		return;
	}

	// Auf den ENTITY-Tag pruefen, NICHT auf CameraUnitWithTag->GetUnitState().
	//
	// Der Actor-Zustand ist auf dem Client der replizierte Wert und sagt laengst "Run" - die
	// Bedingung war damit auf dem Client praktisch immer falsch, der Tag wurde nie gesetzt, und
	// ohne Tag ueberspringt UUnitApplyMassMovementProcessor die Entity (siehe oben). Der Tag ist
	// das, worauf es ankommt, also wird er auch abgefragt.
	// Weiterhin nur beim Wechsel setzen: SetUnitState schreibt Tags um und wuerde bei 60 Hz sonst
	// dauernd Zustaende neu schalten.
	if (!DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStateRunTag::StaticStruct()))
	{
		const FMassCombatStatsFragment* Stats =
			EntityManager->GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle);
		const bool bAttackingOrPausing =
			DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStateAttackTag::StaticStruct()) ||
			DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStatePauseTag::StaticStruct());

		// Schiessen im Laufen: dann NICHT auf Run schalten, sonst bricht der Angriffszyklus ab.
		// Die Vorhersage oben bewegt die Einheit trotzdem.
		if (!(Stats && Stats->bCanMoveWhileAttacking && bAttackingOrPausing))
		{
			if (FMassAIStateFragment* AiState = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
			{
				AiState->PlaceholderSignal = UnitSignals::Run;
			}
			CameraUnitWithTag->SetUnitState(UnitData::Run);
			if (!EntityManager->IsProcessing())
			{
				EntityManager->Defer().AddTag<FMassStateRunTag>(EntityHandle);
				EntityManager->FlushCommands();
			}
		}
	}
}


void ACameraControllerBase::Server_UpdateCameraUnitMovement_Implementation(const FVector& TargetLocation)
{
	if (!CameraUnitWithTag || !CameraUnitWithTag->Attributes || bIsCameraMovementHaltedByUI) return;

	if (CameraUnitWithTag)
	{
		if (TargetLocation.Equals(CameraUnitWithTag->GetMassActorLocation(), 10.0f))
		{
			FMassEntityManager* EntityManager = nullptr;
			FMassEntityHandle EntityHandle;
			if (CameraUnitWithTag->GetMassEntityData(EntityManager, EntityHandle) && EntityManager)
			{
				if (HasAuthority())
				{
					if (FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
					{
						UpdateMoveTarget(*MoveTargetFragmentPtr, CameraUnitWithTag->GetMassActorLocation(), CameraUnitWithTag->Attributes->GetBaseRunSpeed(), GetWorld());
					}
				}
				else if (FMassClientPredictionFragment* PredFrag = EntityManager->GetFragmentDataPtr<FMassClientPredictionFragment>(EntityHandle))
				{
					PredFrag->Location = CameraUnitWithTag->GetMassActorLocation();
					PredFrag->PredDesiredSpeed = CameraUnitWithTag->Attributes->GetBaseRunSpeed();
					PredFrag->PredAcceptanceRadius = 50.f;
					PredFrag->bHasData = true;
				}
			}
			CameraUnitWithTag->AddStopMovementTagToEntity();
			return;
		}

		// ============================================================================
		// LUX-ANPASSUNG 2/3 (Serverseite) â€” Ausnahme NUR fuer die Direktsteuerung.
		// Sonst gilt die Original-Regel unveraendert. In der Direktsteuerung haelt nur
		// ein echter Cast an; eine bloss laufende Faehigkeit (Schiessen) nicht, sonst
		// wird jede Bewegungsanforderung waehrend des Feuerns verworfen.
		// Original: if (GetUnitState() == UnitData::Casting || ActivatedAbilityInstance != nullptr)
		// ============================================================================
		const bool bDirectCtrl = bUnitDirectControl && !CameraUnitMouseFollow;
		const bool bBlocked = bDirectCtrl
			? (CameraUnitWithTag->GetUnitState() == UnitData::Casting)
			: (CameraUnitWithTag->GetUnitState() == UnitData::Casting
			   || CameraUnitWithTag->ActivatedAbilityInstance != nullptr);
		if (bBlocked)
		{
			return;
		}
		// ===================== ENDE LUX-ANPASSUNG 2/3 ===============================

		// Serverseitig denselben Tag fuehren wie der Client, sonst laeuft der Server mit Pfadsuche
		// und der Client ohne - die Reconciliation korrigiert dann dauernd gegeneinander.
		if (bDirectCtrl)
		{
			FMassEntityManager* EM = nullptr;
			FMassEntityHandle EH;
			if (CameraUnitWithTag->GetMassEntityData(EM, EH) && EM && !EM->IsProcessing()
				&& !DoesEntityHaveTag(*EM, EH, FMassDirectControlTag::StaticStruct()))
			{
				EM->Defer().AddTag<FMassDirectControlTag>(EH);
				EM->FlushCommands();
			}
		}

		bool bNavMod = false;
		FVector ValidatedLocation = TraceRunLocation(TargetLocation, bNavMod); // Projiziert die Position auf das Navmesh/den Boden.

		if (bNavMod)
		{
			// Position ist ungültig, keine Bewegung ausführen.
			return;
		}

		//DrawDebugCircle(GetWorld(), ValidatedLocation, 40.f, 16, FColor::Green, false, 0.5f);
		const float Speed = CameraUnitWithTag->Attributes->GetBaseRunSpeed();

		CorrectSetUnitMoveTarget(GetWorld(), CameraUnitWithTag, ValidatedLocation, Speed, CameraUnitWithTag->MovementAcceptanceRadius);
	}
}


#include "Engine/GameInstance.h"
#include "System/MapSwitchSubsystem.h"
#include "Misc/PackageName.h"

bool ACameraControllerBase::Server_TravelToMap_Validate(const FString& MapName, FName TagToEnable)
{
	// Reject only structurally malicious input; a false return disconnects the client, so keep this lenient —
	// legitimate races always carry a valid, server-authored map name. Content validation happens (softly) in
	// the implementation so a bad-but-plausible name is ignored rather than kicking the player.
	return MapName.Len() <= 512;
}

void ACameraControllerBase::Server_TravelToMap_Implementation(const FString& MapName, FName TagToEnable)
{
	// Runs on the SERVER only.
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Guard against a duplicate travel request (e.g. host and client both press "OK" at nearly the same time).
	// We derive the "already travelling" state directly from the engine instead of a process-global static flag:
	//  - World->NextURL is set by ServerTravel and consumed by TickWorldTravel — it self-clears on success AND failure,
	//  - IsInSeamlessTravel() covers the seamless-transition window after NextURL has already been consumed.
	// This is per-world (correct scope) and can never get permanently stuck, unlike the old static bServerTravelInProgress.
	if (!World->NextURL.IsEmpty() || World->IsInSeamlessTravel())
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_TravelToMap: a travel is already pending/in progress, ignoring duplicate request."));
		return;
	}

	// Validate the destination up front so an invalid/garbage name can't kick off a doomed travel (which historically
	// could also latch the old guard). IsValidLongPackageName is a cheap format check (no filesystem hit).
	if (MapName.IsEmpty() || !FPackageName::IsValidLongPackageName(MapName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_TravelToMap: invalid map name '%s', ignoring request."), *MapName);
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
		{
			if (TagToEnable != NAME_None)
			{
				MapSwitchSub->MarkSwitchEnabledForMap(MapName, TagToEnable);
			}
		}
	}

	// Put every player behind a loading screen BEFORE the travel starts. ServerTravel only sets
	// World->NextURL; the map is loaded on a later TickWorldTravel, and until then the old level keeps
	// rendering. Without this the player sits in the old level for the whole load.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get()))
		{
			PC->Client_ShowTravelLoadingScreen();
		}
	}

	World->ServerTravel(MapName);
}

void ACameraControllerBase::Client_ShowTravelLoadingScreen_Implementation()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	TSubclassOf<ULoadingWidget> ClassToUse = TravelLoadingWidgetClass;
	if (!ClassToUse)
	{
		// Reuse whatever the level already configured for the post-arrival loading widget, so this
		// works out of the box on every existing map.
		if (AResourceGameState* GS = GetWorld() ? GetWorld()->GetGameState<AResourceGameState>() : nullptr)
		{
			ClassToUse = GS->LoadingWidgetConfig.WidgetClass;
		}
	}

	if (!ClassToUse)
	{
		UE_LOG(LogTemp, Warning, TEXT("Client_ShowTravelLoadingScreen: no widget class set (TravelLoadingWidgetClass and GameState LoadingWidgetConfig.WidgetClass are both null)."));
		return;
	}

	if (ULoadingWidget* TravelWidget = CreateWidget<ULoadingWidget>(this, ClassToUse))
	{
		// High ZOrder so it covers the HUD that is still up from the old level.
		TravelWidget->AddToViewport(1000);
		StopAllCameraMovement();
	}
}

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Actors/AutoCamWaypoint.h"
#include "Actors/AbilityIndicator.h" // LUX-ANPASSUNG (28.08.2026): Indikatorposition als Wurfziel
#include "Engine/GameViewportClient.h" // Include the header for UGameViewportClient
#include "Engine/Engine.h"      
#include "Kismet/GameplayStatics.h"


void ACameraControllerBase::Client_TriggerWinLoseUI_Implementation(bool bWon, TSubclassOf<class UWinLoseWidget> InWidgetClass, const FString& InMapName, FName DestinationSwitchTagToEnable)
{
	StopAllCameraMovement();
	if (InWidgetClass)
	{
		UWinLoseWidget* WinLoseWidget = CreateWidget<UWinLoseWidget>(this, InWidgetClass);
		if (WinLoseWidget)
		{
			WinLoseWidget->SetupWidget(bWon, InMapName, DestinationSwitchTagToEnable);
			WinLoseWidget->AddToViewport();
		}
	}
}

void ACameraControllerBase::Client_InitializeWinLoseSystem_Implementation()
{
	if (AExtendedCameraBase* ExtendedCamera = Cast<AExtendedCameraBase>(GetPawn()))
	{
		ExtendedCamera->InitializeWinConditionDisplay();
	}
}

void ACameraControllerBase::Client_ShowLoadingWidget_Implementation(TSubclassOf<class ULoadingWidget> InClass, float InTotalDuration, float InServerWorldTimeStart, int32 InTriggerId)
{
	bool bIsLocal = IsLocalPlayerController();
	UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::Client_ShowLoadingWidget: InTriggerId=%d, LastProcessed=%d, IsLocal=%d, TotalDuration=%f, StartTime=%f"), 
		InTriggerId, LastProcessedLoadingTriggerId, bIsLocal, InTotalDuration, InServerWorldTimeStart);

	// If we've already processed this specific trigger, don't show it again
	if (InTriggerId != 0 && InTriggerId == LastProcessedLoadingTriggerId)
	{
		UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::Client_ShowLoadingWidget: TriggerId %d already processed. Skipping."), InTriggerId);
		return;
	}
	
	LastProcessedLoadingTriggerId = InTriggerId;

	if (InClass && bIsLocal)
	{
		Retry_ShowLoadingWidget(InClass, InTotalDuration, InServerWorldTimeStart, InTriggerId, 0);
	}
	else if (!InClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] ACameraControllerBase::Client_ShowLoadingWidget: LoadingWidgetClass is null!"));
	}
	else if (!bIsLocal)
	{
		UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::Client_ShowLoadingWidget: Not local player controller."));
	}
}

void ACameraControllerBase::CheckForLoadingWidget()
{
	if (!IsLocalPlayerController()) return;

	AResourceGameState* GS = GetWorld()->GetGameState<AResourceGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::CheckForLoadingWidget: GameState not ready. Retrying in 0.2s..."));
		FTimerHandle RetryGameStateTimer;
		GetWorldTimerManager().SetTimer(RetryGameStateTimer, this, &ACameraControllerBase::CheckForLoadingWidget, 0.2f, false);
		return;
	}

	const FLoadingWidgetConfig& Config = GS->LoadingWidgetConfig;
	
	UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::CheckForLoadingWidget: Config TriggerId=%d, Class=%s, StartTime=%f"), 
		Config.TriggerId, Config.WidgetClass ? *Config.WidgetClass->GetName() : TEXT("None"), Config.ServerWorldTimeStart);

	if (Config.WidgetClass && Config.Duration > 0.f && Config.ServerWorldTimeStart >= 0.f)
	{
		if (Config.TriggerId != 0 && Config.TriggerId == LastProcessedLoadingTriggerId)
		{
			return;
		}

		float CurrentServerTime = GS->GetServerWorldTimeSeconds();
		float Elapsed = CurrentServerTime - Config.ServerWorldTimeStart;

		if (Elapsed < Config.Duration)
		{
			Client_ShowLoadingWidget(Config.WidgetClass, Config.Duration, Config.ServerWorldTimeStart, Config.TriggerId);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::CheckForLoadingWidget: Loading period already expired (Elapsed=%f, Duration=%f)."), Elapsed, Config.Duration);
		}
	}
}

void ACameraControllerBase::Retry_ShowLoadingWidget(TSubclassOf<class ULoadingWidget> InClass, float InTotalDuration, float InServerWorldTimeStart, int32 InTriggerId, int32 RetryCount)
{
	if (!IsLocalPlayerController() || !InClass) return;

	ULocalPlayer* LP = GetLocalPlayer();
	// Check if LocalPlayer and its ViewportClient are ready
	if (!LP || !LP->ViewportClient)
	{
		if (RetryCount < 20)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] ACameraControllerBase::Retry_ShowLoadingWidget: LocalPlayer or Viewport not ready. Retry %d..."), RetryCount + 1);
			FTimerHandle RetryTimerHandle;
			FTimerDelegate RetryDelegate;
			RetryDelegate.BindUObject(this, &ACameraControllerBase::Retry_ShowLoadingWidget, InClass, InTotalDuration, InServerWorldTimeStart, InTriggerId, RetryCount + 1);
			GetWorldTimerManager().SetTimer(RetryTimerHandle, RetryDelegate, 0.2f, false);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[DEBUG_LOG] ACameraControllerBase::Retry_ShowLoadingWidget: Failed to get LocalPlayer/Viewport after 20 retries. TriggerId=%d"), InTriggerId);
		}
		return;
	}

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->RemoveFromParent();
		ActiveLoadingWidget = nullptr;
	}

	ActiveLoadingWidget = CreateWidget<ULoadingWidget>(this, InClass);
	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->SetupLoadingWidget(InTotalDuration, InServerWorldTimeStart);
		ActiveLoadingWidget->AddToViewport(9999);
		UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ACameraControllerBase::Retry_ShowLoadingWidget: Widget created and added to viewport. TriggerId=%d, StartTime=%f"), InTriggerId, InServerWorldTimeStart);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] ACameraControllerBase::Retry_ShowLoadingWidget: Failed to create widget! TriggerId=%d"), InTriggerId);
	}
}

ACameraControllerBase::ACameraControllerBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
}

void ACameraControllerBase::BeginPlay()
{
	Super::BeginPlay();

	// (The old static ServerTravel guard reset used to live here; the guard is now derived per-world from
	//  World->NextURL / IsInSeamlessTravel in Server_TravelToMap_Implementation, so no manual reset is needed.)

	// Check GameState for any active loading widget (useful if we join late)
	if (IsLocalPlayerController())
	{
		CheckForLoadingWidget();
	}

	HUDBase = Cast<APathProviderHUD>(GetHUD());
	CameraBase = Cast<ACameraBase>(GetPawn());
	SetActorTickEnabled(true);
	
	if(CameraBase) GetViewPortScreenSizes(CameraBase->GetViewPortScreenSizesState);
	
	GetAutoCamWaypoints();
}


void ACameraControllerBase::SetCameraUnitWithTag_Implementation(FGameplayTag Tag, int TeamId)
{
	UE_LOG(LogTemp, Log, TEXT("ACameraControllerBase::SetCameraUnitWithTag_Implementation: Tag=%s, TeamId=%d"), *Tag.ToString(), TeamId);
	
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
		UE_LOG(LogTemp, Log, TEXT("ACameraControllerBase::SetCameraUnitWithTag_Implementation: GameMode found, AllUnits.Num()=%d"), GameMode->AllUnits.Num());
		bool bFound = false;
		for (int32 i = 0; i < GameMode->AllUnits.Num(); i++)
		{
			AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
			
			if (Unit && Unit->UnitTags.HasTagExact(Tag) && Unit->TeamId == TeamId)
			{
				UE_LOG(LogTemp, Log, TEXT("ACameraControllerBase::SetCameraUnitWithTag_Implementation: Found matching unit: %s"), *Unit->GetName());
				ServerSetCameraUnit(Unit, TeamId);
				ClientSetCameraUnit(Unit, TeamId);
				bFound = true;
			}
		}

		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("ACameraControllerBase::SetCameraUnitWithTag_Implementation: No unit found with Tag %s and TeamId %d"), *Tag.ToString(), TeamId);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ACameraControllerBase::SetCameraUnitWithTag_Implementation: GameMode NOT found!"));
	}
}

void ACameraControllerBase::ServerSetCameraUnit_Implementation(AUnitBase* CameraUnit, int TeamId)
{
	if (TeamId != SelectableTeamId) return;
	
	CameraUnitWithTag = CameraUnit;
				
	CameraBase = Cast<ACameraBase>(GetPawn());

	if(CameraBase)
		CameraBase->SetCameraState(CameraData::LockOnCharacterWithTag);
}

void ACameraControllerBase::Multi_SetCameraOnly_Implementation()
{
	CameraBase = Cast<ACameraBase>(GetPawn());
}

void ACameraControllerBase::ClientSetCameraUnit_Implementation(AUnitBase* CameraUnit, int TeamId)
{
	if (GetNetMode() == NM_Client) 	UE_LOG(LogTemp, Log, TEXT("!!!!EXECUTED ON CLIENT!!!!!!!!!!"));
	
	if (TeamId != SelectableTeamId) return;


	if (GetNetMode() == NM_Client) 	UE_LOG(LogTemp, Log, TEXT("!!!!EXECUTED ON CLIENT!2222!!!!!!!!!"));
	CameraUnitWithTag = CameraUnit;
				
	CameraBase = Cast<ACameraBase>(GetPawn());

	if(CameraBase)
		CameraBase->SetCameraState(CameraData::LockOnCharacterWithTag);
}

void ACameraControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent(); 
}

void ACameraControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CheckSpeakingUnits();
	RotateCam(DeltaSeconds);
	CameraBaseMachine(DeltaSeconds);
}

void ACameraControllerBase::MoveCamToLocation(ACameraBase* Camera, const FVector& DestinationLocation)
{
	if (!Camera || !Camera->GetCharacterMovement() || !Camera->GetController())
	{
		return;
	}

	// Check if we have a valid AI controller for the unit
	AAIController* AIController = Cast<AAIController>(Camera->GetController());
	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("No AI-Controller found"));
		return;
	}
	
}

bool ACameraControllerBase::CheckSpeakingUnits()
{
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	
	if(GameMode)
	for (int32 i = 0; i < GameMode->SpeakingUnits.Num(); i++)
	{
		if(GameMode->SpeakingUnits[i]->LockCamOnUnit)
		{
			SpeakingUnit = GameMode->SpeakingUnits[i];
			SetCameraState(CameraData::LockOnSpeaking);
			return true;
		}
	}
	SpeakingUnit = nullptr;
	return false;
}

void ACameraControllerBase::SetCameraState(TEnumAsByte<CameraData::CameraState> NewCameraState)
{
	
	CameraBase->SetCameraState(NewCameraState);
}

void ACameraControllerBase::GetViewPortScreenSizes(int x)
{
	switch (x)
	{
	case 1:
		GetViewportSize(CameraBase->ScreenSizeX, CameraBase->ScreenSizeY);
		break;
	case 2:
		if (GEngine && GEngine->GameViewport)
		{
			FViewport* Viewport = GEngine->GameViewport->Viewport;
			FIntPoint Size = Viewport->GetSizeXY();
			CameraBase->ScreenSizeX = Size.X;
			CameraBase->ScreenSizeY = Size.Y;
		}
		break;
	}
}

FVector ACameraControllerBase::GetCameraPanDirection() {
	float MousePosX = 0;
	float MousePosY = 0;
	float CamDirectionX = 0;
	float CamDirectionY = 0;

	GetMousePosition(MousePosX, MousePosY);

	const float CosYaw = FMath::Cos(CameraBase->SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(CameraBase->SpringArmRotator.Yaw*PI/180);
	
	if (MousePosX <= CameraBase->Margin)
	{
		CamDirectionY = -CosYaw;
		CamDirectionX = SinYaw;
	}
	if (MousePosY <= CameraBase->Margin)
	{
		CamDirectionX = CosYaw;
		CamDirectionY = SinYaw;
	}
	if (MousePosX >= CameraBase->ScreenSizeX - CameraBase->Margin)
	{
		CamDirectionY = CosYaw;
		CamDirectionX = -SinYaw;
	}
	if (MousePosY >= CameraBase->ScreenSizeY - CameraBase->Margin)
	{
		CamDirectionX = -CosYaw;
		CamDirectionY = -SinYaw;
	}
	
	return FVector(CamDirectionX, CamDirectionY, 0);
}

void ACameraControllerBase::SetCameraZDistance(int Index)
{
	if(SelectedUnits.Num() && SelectedUnits[Index])
	{
		float Distance = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);

		if(Distance >= 50.f)
		{
			CameraBase->CameraDistanceToCharacter = Distance;
			
		}else
		{
			CameraBase->CameraDistanceToCharacter = 50.f;
		}
	}
}

void ACameraControllerBase::RotateCam(float DeltaTime)
{
	if (!CameraBase || CameraBase->BlockControls) return;
	if(!MiddleMouseIsPressed) return;
	
	//FHitResult Hit;
	//GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

	if(CameraBase)
	{
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			CameraBase->RotateFree(FVector(MouseX, MouseY, 0.f));
		}
	}
}

FVector ACameraControllerBase::CalculateUnitsAverage(float DeltaTime) {
	
	FVector SumPosition(0, 0, 0);
	int32 UnitCount = 0;
	TArray <AActor*> Units;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnitBase::StaticClass(), Units);
	
	
	int32 UnitsInRangeCount = 0;
	// Count Units within the specified Radius
	
	
	for (AActor* Unit : Units) {
		if (Unit) {
			FVector UnitLocation = Unit->GetActorLocation();
			AUnitBase* UnitBase = Cast<AUnitBase>(Unit);
			if(AutoCamPlayerOnly)
			{
				if(UnitBase->IsPlayer)
					if (FVector::Dist(UnitLocation, OrbitPositions[OrbitRotatorIndex]) <= OrbitRadiuses[OrbitRotatorIndex]) {
						UnitsInRangeCount++;
						SumPosition += UnitLocation;
						UnitCount++;
					}

			}
			else
			{
				if (FVector::Dist(UnitLocation, OrbitPositions[OrbitRotatorIndex]) <= OrbitRadiuses[OrbitRotatorIndex]) {
					UnitsInRangeCount++;
					SumPosition += UnitLocation;
					UnitCount++;
				}
			}
		}
	}
	float UnitTimePart = UnitsInRangeCount*UnitCountOrbitTimeMultiplyer;
	float MaxTime = OrbitTimes[OrbitRotatorIndex] + UnitTimePart;

	if(OrbitLocationControlTimer >= MaxTime)
	{
		OrbitRotatorIndex++;
		OrbitLocationControlTimer = 0.f;
	}
	UnitCountInRange = UnitsInRangeCount;
	
	if(OrbitRotatorIndex >= OrbitPositions.Num())
		OrbitRotatorIndex = 0;

	
	Units.Empty();

	if (UnitCount == 0) return OrbitPositions[OrbitRotatorIndex];
	return SumPosition / UnitCount;
}

void ACameraControllerBase::GetAutoCamWaypoints()
{
	TArray<AActor*> Waypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAutoCamWaypoint::StaticClass(), Waypoints);

	//TArray<AAutoCamWaypoint*> Waypoints;
	AAutoCamWaypoint* StartWaypoint = nullptr;
    
	// Find the first Waypoint that has a NextWaypoint assigned
	for (AActor* ActorWaypoint : Waypoints)
	{
		AAutoCamWaypoint* Waypoint = Cast<AAutoCamWaypoint>(ActorWaypoint);
		
		if (Waypoint && Waypoint->NextWaypoint)
		{
			StartWaypoint = Waypoint;
			break;
		}
	}
    
	// If a valid StartWaypoint is found, setup the orbit positions
	if (StartWaypoint)
	{
		OrbitPositions.Empty(); // Clear existing waypoints
		OrbitTimes.Empty();
		
		AAutoCamWaypoint* CurrentWaypoint = StartWaypoint;
		do
		{
			OrbitPositions.Add(CurrentWaypoint->GetActorLocation());
			OrbitTimes.Add(CurrentWaypoint->OrbitTime);
			CurrentWaypoint = CurrentWaypoint->NextWaypoint;
		} 
		while (CurrentWaypoint && CurrentWaypoint != StartWaypoint); // Continue until loop completes or returns to start
	}
	
}

void ACameraControllerBase::SetCameraAveragePosition(ACameraBase* Camera, float DeltaTime) {

	FVector CameraPosition = CalculateUnitsAverage(DeltaTime);

	Camera->SetActorLocation(FVector(CameraPosition.X, CameraPosition.Y, Camera->GetActorLocation().Z)); // Z-Koordinate bleibt unverändert
}

void ACameraControllerBase::StopAllCameraMovement()
{
	WIsPressedState = 0;
	SIsPressedState = 0;
	AIsPressedState = 0;
	DIsPressedState = 0;
	CamIsRotatingLeft = false;
	CamIsRotatingRight = false;
	CamIsZoomingInState = 0;
	CamIsZoomingOutState = 0;
	ZoomOutToPosition = false;
	ZoomInToPosition = false;
	ScrollZoomCount = 0.f;
	MiddleMouseIsPressed = false;
	// Held ability keys are input state too: blocking controls stops their release from arriving.
	ClearHeldAbilityInputs();

	if (CameraBase)
	{
		CameraBase->BlockControls = true;
		if (CameraBase->GetCharacterMovement())
		{
			CameraBase->GetCharacterMovement()->StopMovementImmediately();
		}
	}
}

void ACameraControllerBase::CameraBaseMachine(float DeltaTime)
{
	if(!CameraBase) return;

	if (CameraBase && SelectedUnits.Num() && LockCameraToUnit)
	{
		CameraBase->LockOnUnit(SelectedUnits[0]);
		LockCameraToCharacter = true;
	}

	if(CameraBase->BlockControls) return;
	
	FVector PanDirection = GetCameraPanDirection();
	
	if(CameraBase)
	{
		switch (CameraBase->GetCameraState())
		{
		case CameraData::UseScreenEdges:
			CameraState_UseScreenEdges();
			break;
		case CameraData::MoveWASD:
			CameraState_MoveWASD(DeltaTime);
			break;
		case CameraData::ZoomIn:
			CameraState_ZoomIn();
			break;
		case CameraData::ZoomOut:
			CameraState_ZoomOut();
			break;
		case CameraData::ScrollZoomIn:
			CameraState_ScrollZoomIn();
			break;
		case CameraData::ScrollZoomOut:
			CameraState_ScrollZoomOut();
			break;
		case CameraData::ZoomOutPosition:
			CameraState_ZoomOutPosition();
			break;
		case CameraData::ZoomInPosition:
			CameraState_ZoomInPosition();
			break;
		case CameraData::HoldRotateLeft:
			CameraState_HoldRotateLeft();
			break;
		case CameraData::HoldRotateRight:
			CameraState_HoldRotateRight();
			break;
		case CameraData::RotateLeft:
			CameraState_RotateLeft();
			break;
		case CameraData::RotateRight:
			CameraState_RotateRight();
			break;
		case CameraData::LockOnCharacter:
			CameraState_LockOnCharacter();
			break;
		case CameraData::LockOnCharacterWithTag:
			CameraState_LockOnCharacterWithTag(DeltaTime);
			break;
		case CameraData::LockOnSpeaking:
			CameraState_LockOnSpeaking();
			break;
		case CameraData::ZoomToNormalPosition:
			CameraState_ZoomToNormalPosition();
			break;
		case CameraData::ZoomToThirdPerson:
			CameraState_ZoomToThirdPerson();
			break;
		case CameraData::ThirdPerson:
			CameraState_ThirdPerson();
			break;
		case CameraData::RotateToStart:
			CameraState_RotateToStart();
			break;
		case CameraData::MoveToPosition:
			CameraState_MoveToPosition(DeltaTime);
			break;
		case CameraData::OrbitAtPosition:
			CameraState_OrbitAtPosition();
			break;
		case CameraData::MoveToClick:
			CameraState_MoveToClick(DeltaTime);
			break;
		case CameraData::LockOnActor:
			CameraState_LockOnActor();
			break;
		case CameraData::OrbitAndMove:
			CameraState_OrbitAndMove(DeltaTime);
			break;
		default:
			CameraBase->SetCameraState(CameraData::UseScreenEdges);
			break;
		}
	}
}

void ACameraControllerBase::CameraState_UseScreenEdges()
{
	if(!CameraBase->DisableEdgeScrolling)
		CameraBase->PanMoveCamera(GetCameraPanDirection()*CameraBase->EdgeScrollCamSpeed);

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState) CameraBase->SetCameraState(CameraData::MoveWASD);
	else if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
	{
		return;
	}

	AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
	if (!DraggedWorkArea)
	{
		return;
	}

	if (IsLocalController())
	{
		CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}
	if (IsLocalController() && !HasAuthority())
	{
		Server_ZoomInToPosition(CameraBase->ZoomPosition, FVector::ZeroVector);
	}
}

void ACameraControllerBase::CameraState_MoveWASD(float DeltaTime)
{
	LockCameraToCharacter = false;

	FVector MoveDirection = FVector::ZeroVector;

	if(WIsPressedState == 1)
	{
		MoveDirection.X += 1.0f;
	}
	if(SIsPressedState == 1)
	{
		MoveDirection.X -= 1.0f;
	}
	if(AIsPressedState == 1)
	{
		MoveDirection.Y -= 1.0f;
	}
	if(DIsPressedState == 1)
	{
		MoveDirection.Y += 1.0f;
	}

	if (IsLocalController() && CameraBase)
	{
		// Also called with zero input so the pan velocity can decelerate to a stop (AccelerationRate/DecelerationRate on CameraBase).
		CameraBase->MoveInDirection(MoveDirection, DeltaTime);

		const bool bIsPanning = !FMath::IsNearlyZero(CameraBase->CurrentCamSpeed.X) || !FMath::IsNearlyZero(CameraBase->CurrentCamSpeed.Y);
		// Sende die neue Position zum Server (nicht die Bewegung!)
		// Der Server speichert sie nur, überschreibt aber nicht die Client-Position
		if (!HasAuthority() && bIsPanning)
		{
			Server_SyncCameraPosition(CameraBase->GetActorLocation());
		}
	}

	if(CamIsRotatingLeft)
	{
		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, !CamIsRotatingLeft);
		}
	}

	if(CamIsRotatingRight)
	{
		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, !CamIsRotatingRight);
		}
	}

	// Stay in the WASD state while the camera is still coasting so the deceleration keeps ticking.
	const bool bPanVelocityZero = FMath::IsNearlyZero(CameraBase->CurrentCamSpeed.X) && FMath::IsNearlyZero(CameraBase->CurrentCamSpeed.Y);
	if(MoveDirection.IsNearlyZero() && bPanVelocityZero && CameraBase->CurrentRotationValue == 0.0f)
	{
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
	{
		return;
	}

	AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
	if (!DraggedWorkArea)
	{
		return;
	}

	if (IsLocalController())
	{
		CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}
	if (IsLocalController() && !HasAuthority())
	{
		Server_ZoomInToPosition(CameraBase->ZoomPosition, FVector::ZeroVector);
	}
}

void ACameraControllerBase::CameraState_ZoomIn()
{
	if(CamIsZoomingInState == 1)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f);
		}
	}
	if(CamIsZoomingInState == 2)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f, true);
		}
	}
	if(CamIsZoomingInState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingInState = 0;

	if(CamIsZoomingInState != 1 && CamIsZoomingOutState == 1)SetCameraState(CameraData::ZoomOut);

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
	else if(!CamIsZoomingInState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
}

void ACameraControllerBase::CameraState_ZoomOut()
{
	if(CamIsZoomingOutState == 1)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f);
		}
	}
	if(CamIsZoomingOutState == 2)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f, true);
		}
	}
	if(CamIsZoomingOutState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingOutState = 0;

	if(CamIsZoomingOutState != 1 && CamIsZoomingInState == 1)SetCameraState(CameraData::ZoomIn);

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
	else if(!CamIsZoomingOutState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
}

void ACameraControllerBase::HandleScrollZoomIn()
{
	if(ScrollZoomCount > 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f);
			CameraBase->RotateSpringArm(false);
		}

		SetCameraZDistance(0);
	}
	if(ScrollZoomCount <= 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f, true);
			CameraBase->RotateSpringArm(false);
		}

		SetCameraZDistance(0);
	}

	if(ScrollZoomCount > 0.f)
		ScrollZoomCount -= 0.25f;
}

void ACameraControllerBase::HandleScrollZoomOut()
{
	if(ScrollZoomCount < 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f);
			CameraBase->RotateSpringArm(true);
		}

		SetCameraZDistance(0);
	}
	if(ScrollZoomCount >= 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f, true);
			CameraBase->RotateSpringArm(true);
		}

		SetCameraZDistance(0);
	}

	if(ScrollZoomCount < 0.f)
		ScrollZoomCount += 0.25f;
}

void ACameraControllerBase::CameraState_ScrollZoomIn()
{
	HandleScrollZoomIn();

	if(ScrollZoomCount < 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		CameraBase->SetCameraState(CameraData::ScrollZoomOut);
	}else if(ScrollZoomCount == 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else if(!CamIsZoomingInState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if(LockCameraToCharacter)
	{
		LockCamToCharacter(0);
	}
}

void ACameraControllerBase::CameraState_ScrollZoomOut()
{
	HandleScrollZoomOut();

	if(ScrollZoomCount > 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		CameraBase->SetCameraState(CameraData::ScrollZoomIn);
	}else if(ScrollZoomCount == 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else if(!CamIsZoomingOutState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if(LockCameraToCharacter)
	{
		LockCamToCharacter(0);
	}
}

void ACameraControllerBase::CameraState_ZoomOutPosition()
{
	ZoomOutToPosition = true;

	if (IsLocalController())
	{
		CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition);
	}
}

void ACameraControllerBase::CameraState_ZoomInPosition()
{
	ZoomOutToPosition = false;
	ZoomInToPosition = true;

	bool bZoomComplete = false;
	if (IsLocalController())
	{
		bZoomComplete = CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}

	if(bZoomComplete)
	{
		SetCameraZDistance(0);
		ZoomInToPosition = false;
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_HoldRotateLeft()
{
	CamIsRotatingRight = false;
	if(LockCameraToCharacter)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::LockOnCharacter);
	}

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, !CamIsRotatingLeft);
	}

	if(!CamIsRotatingLeft && CameraBase->CurrentRotationValue == 0.f)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_HoldRotateRight()
{
	CamIsRotatingLeft = false;

	if(LockCameraToCharacter)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::LockOnCharacter);
	}

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, !CamIsRotatingRight);
	}

	if(!CamIsRotatingRight  && CameraBase->CurrentRotationValue == 0.f)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_RotateLeft()
{
	CamIsRotatingRight = false;
	CamIsRotatingLeft = true;

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	bool bRotationComplete = false;
	if (IsLocalController() && CameraBase)
	{
		bRotationComplete = CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
	}

	if(bRotationComplete)
	{
		CamIsRotatingLeft = false;
		if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_RotateRight()
{
	CamIsRotatingLeft = false;
	CamIsRotatingRight = true;

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	bool bRotationComplete = false;
	if (IsLocalController() && CameraBase)
	{
		bRotationComplete = CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
	}

	if(bRotationComplete)
	{
		CamIsRotatingRight = false;
		if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_LockOnCharacter()
{
	LockCamToCharacter(0);
}

void ACameraControllerBase::CameraState_LockOnCharacterWithTag(float DeltaTime)
{
	LockCamToCharacterWithTag(DeltaTime);
}

void ACameraControllerBase::CameraState_LockOnSpeaking()
{
	if(SpeakingUnit)
		LockCamToSpecificUnit(SpeakingUnit);
	else
		SetCameraState(CameraData::ZoomToNormalPosition);
}

void ACameraControllerBase::CameraState_ZoomToNormalPosition()
{
	bool bZoomComplete = false;
	if (IsLocalController())
	{
		bZoomComplete = CameraBase->ZoomOutToPosition(CameraBase->ZoomPosition);
	}

	if(bZoomComplete)
	{
		bool bRotationComplete = false;

		if (IsLocalController() && CameraBase)
		{
			bRotationComplete = CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
		}

		if(bRotationComplete)
		{
			CamIsRotatingRight = false;
			CamIsRotatingLeft = false;
			if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
			else CameraBase->SetCameraState(CameraData::LockOnCharacter);
		}
	};
}

void ACameraControllerBase::CameraState_ZoomToThirdPerson()
{
	if( SelectedUnits.Num())
	{
		FVector SelectedActorLocation = SelectedUnits[0]->GetActorLocation();

		CameraBase->LockOnUnit(SelectedUnits[0]);
		if (!CameraBase->IsCameraInAngle())
		{
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
			}
		}
		else
		{
			bool bZoomComplete = false;
			if (IsLocalController())
			{
				bZoomComplete = CameraBase->ZoomInToThirdPerson(SelectedActorLocation);
			}

			if(bZoomComplete)
			{
				LockCameraToCharacter = false;
				CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
				CameraBase->SetCameraState(CameraData::ThirdPerson);
			}
		}

	}
}

void ACameraControllerBase::CameraState_ThirdPerson()
{
	if( SelectedUnits.Num())
	{
		float YawActor = SelectedUnits[0]->GetActorRotation().Yaw;
		float YawCamera = CameraBase->GetActorRotation().Yaw;

		CameraBase->LockOnUnit(SelectedUnits[0]);

		if(YawCamera-YawActor < -90)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor > 90)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor < -25)
			CameraBase->RotateCamLeftTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor > 25)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
	}
}

void ACameraControllerBase::CameraState_RotateToStart()
{
	if (FMath::IsNearlyEqual(CameraBase->SpringArmRotator.Yaw, CameraBase->CameraAngles[0], CameraBase->RotationIncreaser))
		CameraBase->SetCameraState(CameraData::MoveToPosition);

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation/3, false);
	}
}

void ACameraControllerBase::CameraState_MoveToPosition(float DeltaTime)
{
	MoveCamToPosition(DeltaTime, CameraBase->OrbitLocation);
}

void ACameraControllerBase::CameraState_OrbitAtPosition()
{
	CamIsRotatingLeft = true;
	CameraBase->OrbitCamLeft(CameraBase->OrbitSpeed);

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
	{
		CamIsRotatingLeft = false;
		CameraBase->SetCameraState(CameraData::MoveWASD);
	}
}

void ACameraControllerBase::CameraState_MoveToClick(float DeltaTime)
{
	if(ClickedActor)
		MoveCamToClick(DeltaTime, ClickedActor->GetActorLocation());
}

void ACameraControllerBase::CameraState_LockOnActor()
{
	CameraBase->LockOnActor(ClickedActor);
}

void ACameraControllerBase::CameraState_OrbitAndMove(float DeltaTime)
{
	CamIsRotatingLeft = true;
	CameraBase->OrbitCamLeft(CameraBase->OrbitSpeed);

	CalcControlTimer += DeltaTime;
	OrbitLocationControlTimer += DeltaTime;


	if(CalcControlTimer >= OrbitAndMovePauseTime)
	{
		FVector CameraPosition = CalculateUnitsAverage(DeltaTime);
		CameraBase->OrbitLocation = FVector(CameraPosition.X, CameraPosition.Y, CameraBase->GetActorLocation().Z);
		CalcControlTimer = 0.f;
	}

	MoveCam(DeltaTime, CameraBase->OrbitLocation);

	// UnitCountInRange schwankt in einer laufenden Schlacht staendig (gemessen 49..57 in
	// fuenf Sekunden). Weil das Zoomziel jeden Frame direkt daraus berechnet wurde, wanderte
	// es permanent um UnitZoomScaler * Schwankung hin und her - bei Scaler 45 also um rund
	// 360 Einheiten. Das war das sichtbare Zoom-Ruckeln, nicht die Zoomgeschwindigkeit.
	// Deshalb wird die Zahl hier zeitbasiert geglaettet und nur der geglaettete Wert benutzt.
	SmoothedUnitCountInRange = FMath::FInterpTo(
		SmoothedUnitCountInRange, static_cast<float>(UnitCountInRange),
		DeltaTime, UnitCountSmoothingSpeed);

	if(SmoothedUnitCountInRange >= UnitCountToZoomOut)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOutAutoCam(CameraBase->ZoomPosition+UnitZoomScaler*SmoothedUnitCountInRange);
		}
	}
	else
	{
		if (IsLocalController())
		{
			CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
		}
	}

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
	{
		CamIsRotatingLeft = false;
		CameraBase->SetCameraState(CameraData::MoveWASD);
	}
}



void ACameraControllerBase::OrbitAtLocation(FVector Destination, float OrbitSpeed)
{
	CameraBase->OrbitLocation = Destination;
	CameraBase->OrbitSpeed = OrbitSpeed;
	CameraBase->OrbitRotationValue = 0.f;
	AIsPressedState = 0;
	DIsPressedState = 0;
	WIsPressedState = 0;
	SIsPressedState = 0;
	CameraBase->SetCameraState(CameraData::RotateToStart);
}

void ACameraControllerBase::Server_MoveCamToPosition_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::OrbitAtPosition);
	}
}

void ACameraControllerBase::Server_SetCameraLocation_Implementation(FVector NewLocation)
{
	if (CameraBase)
	{
		CameraBase->SetActorLocation(NewLocation);
	}
}

void ACameraControllerBase::MoveCamToPosition(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction - Lokale Ausführung für sofortige Reaktion
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		// Die Rampe lief pro Frame (+-10), die Bewegung selbst aber pro Sekunde. Dadurch
		// beschleunigte die Kamera auf einer schnellen Maschine viel abrupter. Auf 60 FPS
		// normiert, damit der eingestellte Verlauf erhalten bleibt.
		const float RampStep = 10.f * CameraBase->GetFrameScale();
		if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
			CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? RampStep : 0.f;
		else
			CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? RampStep : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

		if (Distance <= 50.f) {
			CameraBase->SetCameraState(CameraData::OrbitAtPosition);
		}
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCamToPosition(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::Server_MoveCamToClick_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 2000.0f? 200.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::LockOnActor);
	}
}

void ACameraControllerBase::MoveCamToClick(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 2000.0f? 200.f : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

		if (Distance <= 50.f) {
			CameraBase->SetCameraState(CameraData::LockOnActor);
		}
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCamToClick(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::Server_MoveCam_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	if (Distance <= 50.f) return;

	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);
}

void ACameraControllerBase::MoveCam(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		if (Distance <= 50.f) return;

		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		// Beschleunigungsrampe pro Sekunde statt pro Frame - siehe MoveCamToPosition.
		// Das ist die Rampe, die die AutoCam benutzt, wenn sie dem Schwerpunkt der
		// Schlacht folgt; sie war der Grund fuer das kantige Anfahren.
		const float RampStep = 10.f * CameraBase->GetFrameScale();
		if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
			CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? RampStep : 0.f;
		else
			CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? RampStep : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCam(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::ToggleLockCamToCharacter()
{
	if(IsCtrlPressed)
	{
		LockCameraToCharacter = !LockCameraToCharacter;

		// With an active CameraUnit the camera is already in LockOnCharacterWithTag (the mouse-follow state).
		// Do NOT switch the camera state out of it here: nothing ever re-enters LockOnCharacterWithTag (it is
		// only set at login), so leaving it would permanently kill the mouse-follow. Instead we keep that state
		// and let LockCamToCharacterWithTag read LockCameraToCharacter to center the camera over the unit while
		// it keeps following the mouse.
		if (CameraUnitWithTag)
			return;

		if(LockCameraToCharacter)
			CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else
		{
			CameraBase->SetCameraState(CameraData::UseScreenEdges);
		}
	}
}


void ACameraControllerBase::LockCamToSpecificUnit(AUnitBase* SUnit)
{
	ASpeakingUnit* Unit = Cast<ASpeakingUnit>(SUnit);
	
	if( Unit)
	{
		FVector SelectedActorLocation = Unit->GetActorLocation();
		
		CameraBase->LockOnUnit(Unit);

		CamIsRotatingLeft = true;

		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotationSpeaking/100, false);
		}

		if(CamIsZoomingInState)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomIn(1.f);
			}
		}
		else if(CamIsZoomingOutState)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomOut(1.f);
			}
		}
		else if(ZoomOutToPosition)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition, SelectedActorLocation);
			}
		}
		else if(ZoomInToPosition)
		{
			bool bZoomComplete = false;
			if (IsLocalController())
			{
				bZoomComplete = CameraBase->ZoomInToPosition(CameraBase->ZoomPosition, SelectedActorLocation);
			}

			if(bZoomComplete) ZoomInToPosition = false;
		}
		else if(CameraBase->IsCharacterDistanceTooHigh(Unit->SpeakZoomPosition, SelectedActorLocation))
		{
			if (IsLocalController())
			{
				CameraBase->ZoomInToPosition(Unit->SpeakZoomPosition, SelectedActorLocation);
				CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - Unit->GetActorLocation().Z);
			}
		}

	}else
	{ 
		LockCameraToCharacter = !LockCameraToCharacter;
		CameraBase->SetCameraState(CameraData::ZoomInPosition);
	}
}

void ACameraControllerBase::LockCamToCharacter(int Index)
{
	if( SelectedUnits.Num() && SelectedUnits[Index])
	{
		AUnitBase* TargetUnit = SelectedUnits[Index];
		FVector TargetLocation = TargetUnit->GetActorLocation();

		// --- Interpolation Logic ---
		if(CameraBase)
		{
			const float DeltaTime = GetWorld()->GetDeltaSeconds();
			const float InterpSpeed = 5.0f;

			FVector DesiredCameraLocation = FVector(TargetLocation.X, TargetLocation.Y, CameraBase->GetActorLocation().Z);

			FVector NewCameraLocation = FMath::VInterpTo(CameraBase->GetActorLocation(), DesiredCameraLocation, DeltaTime, InterpSpeed);

			// Client-Side Prediction
			if (IsLocalController())
			{
				CameraBase->SetActorLocation(NewCameraLocation);
			}

			// Server-RPC (nur wenn Client)
			if (IsLocalController() && !HasAuthority())
			{
				Server_SetCameraLocation(NewCameraLocation);
			}
		}
		// --- End Interpolation Logic ---


		if(ScrollZoomCount > 0.f)
		{
			CameraBase->SetCameraState(CameraData::ScrollZoomIn);
		}else if(ScrollZoomCount < 0.f)
		{
			CameraBase->SetCameraState(CameraData::ScrollZoomOut);
		}

		if(RotateBehindCharacterIfLocked)
		{
			float CameraYaw = FMath::Fmod(static_cast<float>(CameraBase->SpringArmRotator.Yaw) + 360.f, 360.f);
			float ActorYaw = FMath::Fmod(static_cast<float>(SelectedUnits[Index]->GetActorRotation().Yaw) + 360.f, 360.f);
			float DeltaYaw = FMath::Fmod(FMath::Fmod(static_cast<float>(ActorYaw + 180.f), 360.f) - CameraYaw + 540.f, 360.f) - 180.f;

			if(!FMath::IsNearlyEqual(CameraYaw, ActorYaw, 10.f))
			{
				float RotationDirection = (DeltaYaw > 0) ? -1.0f : 1.0f;

				if (IsLocalController() && CameraBase)
				{
					CameraBase->RotateCamera(RotationDirection, CameraBase->AddCamRotation*2, false);
				}
			}
		}

		if(CamIsRotatingRight)
		{
			CamIsRotatingLeft = false;
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
			}
		}

		if(CamIsRotatingLeft)
		{
			CamIsRotatingRight = false;
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
			}
		}
	}else
	{
		LockCameraToCharacter = !LockCameraToCharacter;
		CameraBase->SetCameraState(CameraData::ZoomInPosition);
	}
}

void ACameraControllerBase::Server_MoveInDirection_Implementation(FVector Direction, float DeltaTime)
{
	UE_LOG(LogTemp, Warning, TEXT("Server_MoveInDirection called - Direction: %s, DeltaTime: %f"), *Direction.ToString(), DeltaTime);

	if (CameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraBase is valid, calling MoveInDirection"));
		CameraBase->MoveInDirection(Direction, DeltaTime);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CameraBase is NULL in Server_MoveInDirection!"));
	}
}

void ACameraControllerBase::Server_SyncCameraPosition_Implementation(FVector NewPosition)
{
	if (CameraBase)
		CameraBase->SetActorLocation(NewPosition);
}

void ACameraControllerBase::Server_RotateCamera_Implementation(float Direction, float Add, bool stopCam)
{
	if (CameraBase)
	{
		CameraBase->RotateCamera(Direction, Add, stopCam);
	}
}

void ACameraControllerBase::Server_RotateSpringArm_Implementation(bool Invert)
{
	if (CameraBase)
	{
		CameraBase->RotateSpringArm(Invert);
	}
}

void ACameraControllerBase::Server_ZoomIn_Implementation(float Value, bool Stop)
{
	if (CameraBase)
	{
		CameraBase->ZoomIn(Value, Stop);
	}
}

void ACameraControllerBase::Server_ZoomOut_Implementation(float Value, bool Stop)
{
	if (CameraBase)
	{
		CameraBase->ZoomOut(Value, Stop);
	}
}

void ACameraControllerBase::Server_ZoomInToPosition_Implementation(float Distance, FVector OptionalLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomInToPosition(Distance, OptionalLocation);
	}
}

void ACameraControllerBase::Server_ZoomOutToPosition_Implementation(float Distance, FVector OptionalLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomOutToPosition(Distance, OptionalLocation);
	}
}

void ACameraControllerBase::Server_ZoomInToThirdPerson_Implementation(FVector SelectedActorLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomInToThirdPerson(SelectedActorLocation);
	}
}

void ACameraControllerBase::Server_ZoomOutAutoCam_Implementation(float Position)
{
	if (CameraBase)
	{
		CameraBase->ZoomOutAutoCam(Position);
	}
}

void ACameraControllerBase::LockCamToCharacterWithTag(float DeltaTime)
{
	CameraUnitUpdateTimer -= DeltaTime;
	CameraSyncTimer -= DeltaTime;

	if (CameraUnitWithTag)
        {
        	bool bCanMove = true;
			if (bIsCameraMovementHaltedByUI)
			{
				bCanMove = false;
			}
        	FMassEntityManager* EntityManager = nullptr;
        	FMassEntityHandle EntityHandle;
        	bool bHasCastingTag = false;
        	if (CameraUnitWithTag->GetMassEntityData(EntityManager, EntityHandle) && EntityManager)
        	{
        		bHasCastingTag = DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStateCastingTag::StaticStruct());
        	}

        	// ORIGINAL, unveraendert: gilt weiterhin fuer den Maus-Folgen-Betrieb.
        	if (bHasCastingTag ||
				CameraUnitWithTag->GetUnitState() == UnitData::Casting ||
				CameraUnitWithTag->ActivatedAbilityInstance != nullptr ||
				CurrentDraggedAbilityIndicator != nullptr)
        	{
        		bCanMove = false;
        	}

        	// ====================================================================================
        	// LUX-ANPASSUNG 2/3 â€” eigene Sperr-Regel NUR fuer die Direktsteuerung (16.08.2026)
        	// Greift ausschliesslich bei CameraUnit + CameraUnitMouseFollow == false; das
        	// Verhalten aller anderen Modi bleibt unangetastet (`bCanMove` oben unveraendert).
        	// Unterschied: hier haelt NUR ein echter Cast an. Eine bloss laufende Faehigkeit
        	// (Schiessen) oder ein gezogener Indikator duerfen die Bewegung nicht stoppen.
        	// ====================================================================================
        	// ------------------------------------------------------------------------------------
        	// LUX-ANPASSUNG (28.08.2026) - Faehigkeiten, die die Einheit festhalten sollen.
        	// Silvan: "Waehrend der Character Granaten und CC-Faehigkeit ausfuehrt soll er sich
        	// nicht bewegen koennen."
        	//
        	// bStopMovementOnActivation ist im Template bereits DER Schalter dafuer, ob eine
        	// laufende Faehigkeit Bewegung erlaubt - er wird an drei weiteren Stellen genau so
        	// gelesen (GameplayAbilityBase, ExtendedControllerBase, CustomControllerBase) und ist
        	// beim Schuss aus, bei Granate/CC an. Er hielt die Einheit bisher aber nur EINMAL bei
        	// der Aktivierung an: die Direktsteuerung setzt jeden Frame ein neues Laufziel, also
        	// lief sie sofort weiter. Deshalb hier zusaetzlich als Dauer-Sperre lesen.
        	//
        	// Auf dem Client gibt es keine Instanz - dort traegt der replizierte Snapshot die
        	// Klasse; das CDO reicht, weil das Flag eine Einstellung ist.
        	// ------------------------------------------------------------------------------------
        	const UGameplayAbilityBase* LuxRunningForMove = CameraUnitWithTag->ActivatedAbilityInstance
        		? CameraUnitWithTag->ActivatedAbilityInstance
        		: (CameraUnitWithTag->CurrentSnapshot.AbilityClass
        			? CameraUnitWithTag->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>()
        			: nullptr);
        	const bool bLuxAbilityHoldsUnit = LuxRunningForMove && LuxRunningForMove->bStopMovementOnActivation;

        	const bool bCanMoveDirect = !bIsCameraMovementHaltedByUI
        		&& !bHasCastingTag
        		&& CameraUnitWithTag->GetUnitState() != UnitData::Casting
        		&& !bLuxAbilityHoldsUnit;
        	// ===================== ENDE LUX-ANPASSUNG 2/3 =======================================
        	
        	// Calculate movement direction based on input states
        	// Only add direction when state is 1 (active press), not 2 (decelerate)
        	FVector MoveDirection = FVector::ZeroVector;

        	// ====================================================================================
        	// LUX-ANPASSUNG 2b/3 â€” die Sperre, die das Schiessen im Laufen verhindert hat.
        	// Diese Klammer baut den Richtungsvektor. Sie hing am ORIGINALEN bCanMove, und das
        	// enthaelt "ActivatedAbilityInstance != nullptr" - waehrend des Schusses war die
        	// Richtung damit immer null, egal was weiter unten steht. Gemessen: WTaste=1 (Taste
        	// erkannt), aber Eingabe=0 (Richtung leer) und 0 Aufrufe von
        	// Server_UpdateCameraUnitMovement.
        	// In der Direktsteuerung gilt deshalb bCanMoveDirect (nur ein echter Cast sperrt),
        	// in jedem anderen Modus unveraendert bCanMove.
        	// Original: if(bCanMove)
        	// ====================================================================================
        	const bool bCanBuildDirection = (bUnitDirectControl && !CameraUnitMouseFollow)
        		? bCanMoveDirect
        		: bCanMove;

        	if(bCanBuildDirection)
        	{
	        	if(WIsPressedState == 1)
	        	{
	        		MoveDirection.X += 1.0f; // Forward
	        	}
	        	if(SIsPressedState == 1)
	        	{
	        		MoveDirection.X -= 1.0f; // Backward
	        	}
	        	if(AIsPressedState == 1)
	        	{
	        		MoveDirection.Y -= 1.0f; // Left
	        	}
	        	if(DIsPressedState == 1)
	        	{
	        		MoveDirection.Y += 1.0f; // Right
	        	}
        	}

        	// ====================================================================================
        	// LUX-ANPASSUNG 1/3 â€” Schalter fuer die Direktsteuerung (16.08.2026)
        	// Eine CameraUnit ist gesetzt und sie folgt NICHT der Maus: dann bewegt WASD die
        	// Einheit selbst, und die Kamera bleibt ueber ihr stehen - ein Kamera-Schwenk waere
        	// hier falsch, weil die Kamera der Einheit folgen soll.
        	// ====================================================================================
        	const bool bDirectUnitControl = bUnitDirectControl && !CameraUnitMouseFollow;

        	// When locked over the CameraUnit (Ctrl+G toggles LockCameraToCharacter), keep the camera centered
        	// on the unit instead of WASD-panning, so the unit can keep following the mouse while the camera stays
        	// above it. We deliberately stay in CameraData::LockOnCharacterWithTag (see ToggleLockCamToCharacter).
        	if (LockCameraToCharacter || bDirectUnitControl)
        	{
        		if (IsLocalController() && CameraBase)
        		{
        			const FVector UnitLoc = CameraUnitWithTag->GetActorLocation();
        			const FVector DesiredCamLoc = FVector(UnitLoc.X, UnitLoc.Y, CameraBase->GetActorLocation().Z);
        			// Bei Direktsteuerung zieht die Kamera straffer nach, sonst haengt sie der
        			// selbst gesteuerten Einheit sichtbar hinterher.
        			const float FollowSpeed = bDirectUnitControl ? UnitDirectCamFollowSpeed : 5.0f;
        			const FVector NewCamLoc = FMath::VInterpTo(CameraBase->GetActorLocation(), DesiredCamLoc, DeltaTime, FollowSpeed);
        			CameraBase->SetActorLocation(NewCamLoc);

        			// Keep the server in sync (client-only); mirrors the WASD-pan sync below.
        			if (!HasAuthority() && CameraSyncTimer <= 0.f && !CameraBase->GetActorLocation().Equals(LastSyncedCameraLocation, 25.0f))
        			{
        				Server_SyncCameraPosition(NewCamLoc);
        				LastSyncedCameraLocation = NewCamLoc;
        				CameraSyncTimer = CameraSyncInterval;
        			}
        		}
        	}
        	// Execute movement locally for immediate response (Client-Side Prediction)
        	else
        	{
        		// Execute locally for all local controllers (Client and Server)
        		// Also runs with zero input so the pan velocity can decelerate to a stop.
        		if (IsLocalController() && CameraBase)
        		{
        			CameraBase->MoveInDirection(MoveDirection, DeltaTime);

        			// Sende die neue Kamera-Position zum Server (unreliable für Performance)
        			if (!HasAuthority() && CameraSyncTimer <= 0.f && !CameraBase->GetActorLocation().Equals(LastSyncedCameraLocation, 25.0f))
        			{
        				Server_SyncCameraPosition(CameraBase->GetActorLocation());
        				LastSyncedCameraLocation = CameraBase->GetActorLocation();
        				CameraSyncTimer = CameraSyncInterval;
        			}
        		}
        	}

        	if (bDirectUnitControl)
        	{
        		// ================================================================================
        		// LUX-ANPASSUNG 3/3 â€” WASD bewegt die Einheit (16.08.2026)
        		// Statt einer Physik-Kraft wird das Mass-Laufziel ein Stueck VOR die Einheit
        		// gesetzt und laufend nachgefuehrt. Das ist derselbe Pfad, den auch das
        		// Maus-Folgen nutzt (Server_UpdateCameraUnitMovement -> MoveTargetFragment auf
        		// dem Server, ClientPredictionFragment auf dem Client), damit Navigation,
        		// Ausweichen und Replikation unveraendert weiterarbeiten. Eine direkte Kraft
        		// wuerde beides umgehen und auf Clients auseinanderlaufen.
        		// ================================================================================
        		UnitDirectMoveTimer -= DeltaTime;

        		// ================================================================================
        		// LUX-ANPASSUNG 7/7 (17.08.2026) - Zielrichtung muss beim Server ankommen.
        		// Silvan: "Wenn ich auf dem Client spiele kommt die Rotation nicht beim Server an."
        		//
        		// Die Einheit dreht sich zur Maus; die dafuer noetige Mausposition schickt bisher
        		// AUSSCHLIESSLICH UMassRotateToMouseProcessor::Execute an den Server - und der
        		// steigt in Zeile 1 wieder aus:
        		//     if (EntityQuery.GetNumMatchingEntities() == 0) return;
        		// Die Query verlangt FMassRotateToMouseTag. Solange auf dem CLIENT keine Entity
        		// diesen Tag traegt, wird UpdateMouseLocationWithThrottling nie aufgerufen, der
        		// Server behaelt eine veraltete ReplicatedMouseLocation und dreht die Einheit
        		// woanders hin. Der Client dreht lokal richtig - beide ziehen gegeneinander, das
        		// ist das Wackeln beim Laufen und Schiessen.
        		//
        		// Die Mausposition haengt hier nicht mehr am Tag: wer eine Einheit direkt steuert,
        		// zielt. Der Versand bleibt gedrosselt (20 Hz bzw. >15 Einheiten Bewegung), es
        		// entsteht also kein zusaetzlicher Netzverkehr gegenueber dem alten Pfad.
        		// ================================================================================
        		if (!HasAuthority() && IsLocalController())
        		{
        			FHitResult MausTreffer;
        			if (GetHitResultUnderCursor(ECC_Visibility, false, MausTreffer))
        			{
        				UpdateMouseLocationWithThrottling(MausTreffer.Location);
        			}
        		}

        		const FVector UnitLoc = CameraUnitWithTag->GetMassActorLocation();


        		// ================================================================================
        		// LUX-ANPASSUNG 4/4 â€” Bewegungssperre beim Zielen fuer DIESE Einheit aufheben.
        		// FMassStopWhileAimingTag wird an zwei Stellen gesetzt: in GameplayAbilityBase
        		// (dort greift die Ausnahme korrekt) und in
        		// AExtendedControllerBase::BatchSetRotateToMouseTagLocally - dem lokalen Spiegel,
        		// der die Faehigkeit noch gar nicht kennt (gemessen: "Ctrl: setzeSperre=1
        		// Instanz=0"). Statt dort zu raten, wird die Sperre hier wieder entfernt,
        		// solange die laufende Faehigkeit Bewegung ausdruecklich erlaubt. Das deckt
        		// beide Pfade ab und bleibt auf die direkt gesteuerte CameraUnit begrenzt.
        		// ================================================================================
        		{
        			const UGameplayAbilityBase* LuxRunning = CameraUnitWithTag->ActivatedAbilityInstance
        				? CameraUnitWithTag->ActivatedAbilityInstance
        				: (CameraUnitWithTag->CurrentSnapshot.AbilityClass
        					? CameraUnitWithTag->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>()
        					: nullptr);

        			if (LuxRunning && !LuxRunning->bStopMovementOnActivation)
        			{
        				FMassEntityManager* EM = nullptr;
        				FMassEntityHandle EH;
        				if (CameraUnitWithTag->GetMassEntityData(EM, EH) && EM
        					&& DoesEntityHaveTag(*EM, EH, FMassStopWhileAimingTag::StaticStruct()))
        				{
        					EM->Defer().RemoveTag<FMassStopWhileAimingTag>(EH);
        				}
        			}
        		}
        		// ===================== ENDE LUX-ANPASSUNG 4/4 ===================================
        		// bCanMoveDirect statt bCanMove: nur hier gilt die gelockerte Sperr-Regel.
        		const bool bHasInput = bCanMoveDirect && !MoveDirection.IsNearlyZero();

        		if (bHasInput && IsLocalController() && CameraBase)
        		{
        			// Eingabe in Weltkoordinaten drehen - exakt wie ACameraBase::MoveInDirection,
        			// damit sich WASD bei gedrehter Kamera identisch anfuehlt.
        			FVector Dir = MoveDirection.GetSafeNormal();
        			const float YawRad = CameraBase->SpringArmRotator.Yaw * PI / 180.f;
        			const float CosYaw = FMath::Cos(YawRad);
        			const float SinYaw = FMath::Sin(YawRad);

        			FVector WorldDir;
        			WorldDir.X = Dir.X * CosYaw - Dir.Y * SinYaw;
        			WorldDir.Y = Dir.X * SinYaw + Dir.Y * CosYaw;
        			WorldDir.Z = 0.f;

        			// Das Ziel MUSS deutlich weiter weg liegen als die Einheit zwischen zwei
        			// Updates schafft (~700 uu/s * Intervall) und als ihr Akzeptanzradius
        			// (MovementAcceptanceRadius, 50). Sonst erreicht sie das Ziel, haelt wegen
        			// IntentAtGoal = Stand an und laeuft beim naechsten Update wieder los -
        			// genau das fuehlte sich "laggy" an. Eine Rampe auf kleine Werte stand
        			// hier zuerst und war die Ursache; das Antippen loest stattdessen der
        			// harte Stopp beim Loslassen (Server_StopCameraUnitDirect).
        			UnitDirectHoldTime += DeltaTime;
        			const FVector Target = UnitLoc + WorldDir * UnitDirectMoveLookAhead;
        			LastUnitDirectWorldDir = WorldDir; // fuers Auslaufen beim Loslassen

        			// LUX-ANPASSUNG 5/5 - JEDEN Frame lokal vorhersagen, nicht nur im RPC-Takt.
        			// Der RPC bleibt gedrosselt (Bandbreite), die lokale Reaktion darf es nicht sein:
        			// sonst haengt die Einheit auf dem Client an Roundtrip + Replikation.
        			if (!HasAuthority())
        			{
        				// !bUnitDirectWasMoving == erster Takt dieser Bewegung.
        				ApplyDirectMovePredictionLocally(Target, false, !bUnitDirectWasMoving);
        			}

        			if (UnitDirectMoveTimer <= 0.f)
        			{
        				LastCameraUnitMovementLocation = Target;
        				Server_UpdateCameraUnitMovement(Target);
        				UnitDirectMoveTimer = UnitDirectMoveUpdateInterval;
        			}
        			bUnitDirectWasMoving = true;
        		}
        		else if (bUnitDirectWasMoving)
        		{
        			// Taste losgelassen: HART anhalten (Stand + Speed 0). Der Umweg ueber
        			// Server_UpdateCameraUnitMovement(UnitLoc) liess die Einheit ausrollen,
        			// weil dort UpdateMoveTarget mit voller BaseRunSpeed gesetzt wird.
        			LastCameraUnitMovementLocation = UnitLoc;

        			// LUX-ANPASSUNG 5/5 - dasselbe Auslaufziel lokal vorhersagen, das der Server
        			// gleich setzt. Ohne das haelt die Einheit auf dem Client erst an, wenn der
        			// Stopp vom Server zurueckkommt - sie schoesse sichtbar ueber.
        			if (!HasAuthority())
        			{
        				const FVector GlideTarget =
        					(UnitDirectStopGlide > 1.f && !LastUnitDirectWorldDir.IsNearlyZero())
        						? UnitLoc + LastUnitDirectWorldDir * UnitDirectStopGlide
        						: UnitLoc;
        				ApplyDirectMovePredictionLocally(GlideTarget, true);
        			}

        			Server_StopCameraUnitDirect();
        			UnitDirectMoveTimer = 0.f;
        			UnitDirectHoldTime = 0.f;
        			bUnitDirectWasMoving = false;
        		}
        		else
        		{
        			UnitDirectHoldTime = 0.f;
        		}
        		// ===================== ENDE LUX-ANPASSUNG 3/3 ===================================
        	}
        	else if (APawn* ControlledPawn = GetPawn())
        	{
        		const bool bIsLocal = IsLocalController();
        		FVector MoveTargetLocation;

        		if (bCanMove)
        		{
        			MoveTargetLocation = ControlledPawn->GetActorLocation();
        			if (CameraUnitMouseFollow && bIsLocal)
        			{
        				FHitResult Hit;
        				if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
        				{
        					MoveTargetLocation = Hit.Location;
        				}
        			}
        		}
        		else
        		{
        			// STOP: Use current unit location
        			MoveTargetLocation = CameraUnitWithTag->GetMassActorLocation();
        		}

        		if (CameraUnitUpdateTimer <= 0.f && !MoveTargetLocation.Equals(LastCameraUnitMovementLocation, 50.0f))
        		{
        			if (!CameraUnitMouseFollow || bIsLocal || !bCanMove)
        			{
        				LastCameraUnitMovementLocation = MoveTargetLocation;
        				Server_UpdateCameraUnitMovement(MoveTargetLocation);
        				CameraUnitUpdateTimer = CameraUnitUpdateInterval;
        			}
        		}
        	}
        	
        	if (ScrollZoomCount > 0.f)
            {

        		HandleScrollZoomIn();
            }
            else if (ScrollZoomCount < 0.f)
            {
            	HandleScrollZoomOut();
            }
        	
     
        	
            // ====================================================================================
            // Kamera sanft hinter die Einheit schwenken (22.08.2026)
            //
            // Greift nur bei Direktsteuerung (CameraUnitMouseFollow == false): beim Maus-Folgen
            // bestimmt der Zeiger die Blickrichtung, ein selbsttaetiges Nachdrehen wuerde dort
            // gegen die Zielhilfe arbeiten.
            //
            // Dreht der Spieler gerade selbst (Q/E), setzt das Nachfuehren aus und beginnt erst
            // nach RotateCamBehindDelayAfterManual wieder - sonst zoege die Automatik jede
            // manuelle Drehung sofort zurueck.
            // ====================================================================================
            if (bRotateCamBehindCharacter && !CameraUnitMouseFollow && CameraUnitWithTag)
            {
                const bool bSpielerDrehtSelbst = CamIsRotatingLeft || CamIsRotatingRight;

                if (bSpielerDrehtSelbst)
                {
                    RotateCamBehindCooldown = RotateCamBehindDelayAfterManual;
                }
                else if (RotateCamBehindCooldown > 0.f)
                {
                    RotateCamBehindCooldown = FMath::Max(0.f, RotateCamBehindCooldown - DeltaTime);
                }
                else if (IsLocalController() && CameraBase)
                {
                    const float ZielYaw = CameraUnitWithTag->GetActorRotation().Yaw + RotateCamBehindYawOffset;
                    CameraBase->RotateCamYawTowards(ZielYaw, RotateCamBehindSpeed, DeltaTime, RotateCamBehindDeadzone);
                }
            }

            if (RotateBehindCharacterIfLocked)
            {
                float CameraYaw = FMath::Fmod(static_cast<float>(CameraBase->SpringArmRotator.Yaw) + 360.f, 360.f);
                float ActorYaw = FMath::Fmod(static_cast<float>(CameraUnitWithTag->GetActorRotation().Yaw) + 360.f, 360.f);
                float DeltaYaw = FMath::Fmod(FMath::Fmod(static_cast<float>(ActorYaw + 180.f), 360.f) - CameraYaw + 540.f, 360.f) - 180.f;

                if (!FMath::IsNearlyEqual(CameraYaw, ActorYaw, 10.f))
                {
                    float RotationDirection = (DeltaYaw > 0) ? -1.0f : 1.0f;

                    if (IsLocalController() && CameraBase)
                    {
                        CameraBase->RotateCamera(RotationDirection, CameraBase->AddCamRotation * 2, false);
                    }
                }
            }

            if (CamIsRotatingRight)
            {
                CamIsRotatingLeft = false;
                if (IsLocalController() && CameraBase)
                {
                    CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
                }
            }

            if (CamIsRotatingLeft)
            {
                CamIsRotatingRight = false;
                if (IsLocalController() && CameraBase)
                {
                    CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
                }
            }
          
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unit does not have the required tag."));
        }

}


void ACameraControllerBase::LockZDistanceToCharacter()
{
	if(ZoomInToPosition == false &&
		ZoomOutToPosition == false &&
		CamIsZoomingInState == 0 &&
		CamIsZoomingOutState == 0 &&
		CameraBase &&
		SelectedUnits.Num())
	{
		
		const FVector SelectedActorLocation = SelectedUnits[0]->GetActorLocation();
		const FVector CameraBaseLocation = CameraBase->GetActorLocation();
		
		const float NewCameraDistanceToCharacter = (CameraBaseLocation.Z - SelectedActorLocation.Z);
		float ZChange = CameraBase->CameraDistanceToCharacter - NewCameraDistanceToCharacter;
		
		const float CosYaw = FMath::Cos(CameraBase->SpringArmRotator.Yaw*PI/180);
		const float SinYaw = FMath::Sin(CameraBase->SpringArmRotator.Yaw*PI/180);
		const FVector NewPawnLocation = FVector(SelectedActorLocation.X - CameraBase->CameraDistanceToCharacter * 0.7*CosYaw, SelectedActorLocation.Y - CameraBase->CameraDistanceToCharacter * 0.7*SinYaw, CameraBaseLocation.Z+ZChange);

		// Client-Side Prediction
		if (IsLocalController())
		{
			CameraBase->SetActorLocation(NewPawnLocation);
			CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
		}

		// Server-RPC (nur wenn Client)
		if (IsLocalController() && !HasAuthority())
		{
			Server_SetCameraLocation(NewPawnLocation);
		}
	}
}
