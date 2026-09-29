// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Animations/UnitBaseAnimInstance.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h" // LUX-ANPASSUNG (16.08.2026): FMassVelocityFragment fuer MassSpeed
#include "Mass/MassActorBindingComponent.h"
#include "Animations/UnitAnimationProcessor.h"   // RTSDiagIstAusgewaehlt
#include "HAL/IConsoleManager.h"

/**
 * Vergleichsmessung Soldier gegen MassShooter.
 *
 * Drei Erklaerungen fuer "Mischung aus Schuss- und Idle-Animation" waren nicht zu unterscheiden:
 * falsche Mischpunkte, falsche Laufrichtung, oder ein Zustand der zu schnell hin und her
 * schaltet. Die Zeile zeigt alle drei Groessen nebeneinander - dann entscheidet die Messung.
 */
static TAutoConsoleVariable<int32> CVarRTS_AnimBlendDiag(
	TEXT("rts.anim.blend.diag"),
	0,
	TEXT("1 = protokolliert Zustand, Mischpunkte, Tempo und Richtung je Einheit."),
	ECVF_Default);
#include "MassExecutionContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"

UUnitBaseAnimInstance::UUnitBaseAnimInstance() {
	CharAnimState = UnitData::Idle;
}


void UUnitBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UUnitBaseAnimInstance::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUnitBaseAnimInstance, AnimDataTable);
}

void UUnitBaseAnimInstance::NativeUpdateAnimation(float Deltaseconds)
{
	Super::NativeUpdateAnimation(Deltaseconds);
	/*
	ControlTimer += Deltaseconds;
	if(ControlTimer < UpdateTime) return;
	ControlTimer = 0.f;
	*/
	bMassSpeedValid = false;

	AActor* OwningActor = GetOwningActor();

	if (OwningActor != nullptr) {
		AUnitBase* UnitBase = Cast<AUnitBase>(OwningActor);
		if (UnitBase != nullptr && UnitBase->IsOnViewport) {

			if (UnitBase->MassActorBindingComponent)
			{
				const FMassEntityHandle Entity = UnitBase->MassActorBindingComponent->GetEntityHandle();
				if (Entity.IsValid())
				{
					UWorld* World = UnitBase->GetWorld();
					UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
					if (EntitySubsystem)
					{
						const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
						if (EntityManager.IsEntityValid(Entity))
						{
							FUnitAnimationFragment* AnimFrag = EntityManager.GetFragmentDataPtr<FUnitAnimationFragment>(Entity);
							if (AnimFrag)
							{
								BlendPoint_1 = AnimFrag->TargetBlendPoint_1;
								BlendPoint_2 = AnimFrag->TargetBlendPoint_2;
								CurrentBlendPoint_1 = AnimFrag->CurrentBlendPoint_1;
								CurrentBlendPoint_2 = AnimFrag->CurrentBlendPoint_2;
								TransitionRate_1 = AnimFrag->TransitionRate_1;
								TransitionRate_2 = AnimFrag->TransitionRate_2;
								Resolution_1 = AnimFrag->Resolution_1;
								Resolution_2 = AnimFrag->Resolution_2;
								Sound = AnimFrag->Sound;
								ContinuousPlayRate = AnimFrag->PlayRate;
								ContinuousAnimationPosition = AnimFrag->AnimationPosition;

								// ============================================================
								// LUX-ANPASSUNG (16.08.2026) â€” Mass-Geschwindigkeit fuer den
								// AnimBP. Siehe Kommentar an MassSpeed in der .h.
								//
								// Bewusst HIER und nicht im UnitAnimationProcessor: dessen
								// AnimInstance-Zugriff steht in einem Zweig, der nur bei
								// ZUSTANDSWECHSELN laeuft (LastProcessedState != CurrentState).
								// Gemessen blieb MassSpeed dort auf 0, obwohl die Einheit lief.
								// Pro Frame ginge es dort nur mit einem zusaetzlichen
								// GetAnimInstance() + Cast je Entity - teurer als der eine
								// Fragment-Lookup hier, der zudem nur fuer sichtbare Einheiten
								// laeuft (IsOnViewport). Rein additiv.
								// ============================================================
								if (const FMassVelocityFragment* VelFrag =
									EntityManager.GetFragmentDataPtr<FMassVelocityFragment>(Entity))
								{
									MassVelocity = VelFrag->Value;
									MassSpeed = MassVelocity.Size2D();
									bMassSpeedValid = true;
								}
								// ===================== ENDE LUX-ANPASSUNG ===================

								/*
								if (World && World->GetNetMode() == NM_Client)
								{
								}
								*/
							}
							else
							{
							}
						}
					}
				}
			}

			CharAnimState = UnitBase->GetUnitState();

			// Anzeige-Zustand einer Faehigkeit hat Vorrang, solange er laeuft. Siehe
			// AnimStateOverride in der .h - der echte Zustand der Einheit bleibt unberuehrt.
			bool bStateBlendPointVorrang = false;
			float StateBlendPointVorrang_1 = 0.f;
			float StateBlendPointVorrang_2 = 0.f;
			if (AnimStateOverride != UnitData::None)
			{
				const UWorld* OverrideWorld = UnitBase->GetWorld();
				if (OverrideWorld && OverrideWorld->GetTimeSeconds() <= AnimStateOverrideUntil)
				{
					CharAnimState = AnimStateOverride;

					// Die MISCHPUNKTE muessen mit - sonst ist der Vorrang wirkungslos.
					//
					// Gemessen (26.09.2026): die Mischpunkte kommen vom UUnitAnimationProcessor
					// aus der Zeile des ECHTEN Zustands. Steht die Einheit auf Idle, sind das
					// (25, 75). Ein bloss umgeschriebenes CharAnimState laesst den
					// Attack-Zustand dann BS_Wraith bei (25, 75) abtasten - dort liegt
					// Idle_NonCombat, nicht Fire_A_Slow bei (25, 25). Das ergibt die gemeldete
					// "Mischung aus Schuss- und Idle-Animation".
					//
					// Beim WeaponModule-Charakter wechselt der ECHTE Zustand, deshalb setzt der
					// Prozessor dort die Attack-Zeile und es sieht richtig aus.
					if (AnimDataTable)
					{
						for (const TPair<FName, uint8*>& Eintrag : AnimDataTable->GetRowMap())
						{
							const FUnitAnimData* Zeile = reinterpret_cast<const FUnitAnimData*>(Eintrag.Value);
							if (Zeile && Zeile->AnimState == AnimStateOverride)
							{
								// NUR die Zustandswerte - CurrentBlendPoint gehoert den Beinen.
								StateBlendPointVorrang_1 = Zeile->BlendPoint_1;
								StateBlendPointVorrang_2 = Zeile->BlendPoint_2;
								bStateBlendPointVorrang = true;
								break;
							}
						}
					}
				}
				else
				{
					AnimStateOverride = UnitData::None;
				}
			}

			// Der Anzeige-Zustand bringt seine eigenen Zustandswerte mit.
			if (bStateBlendPointVorrang)
			{
				CurrentBlendPoint_1 = StateBlendPointVorrang_1;
				CurrentBlendPoint_2 = StateBlendPointVorrang_2;
			}

			// Vollkoerper-Todespose: siehe bIsDeadPose in der .h.
			bIsDeadPose = (CharAnimState == UnitData::Dead);

			// Vergleichsmessung, siehe rts.anim.blend.diag.
			if (CVarRTS_AnimBlendDiag.GetValueOnGameThread() != 0)
			{
				++BlendDiagZaehler;
				if (BlendDiagZaehler % 20 == 1)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[AnimMisch] %s Zustand=%d (Einheit=%d Vorrang=%d) Mischpunkte=(%.1f, %.1f) Tempo=%.0f gueltig=%d Richtung=%.0f"),
						*GetNameSafe(UnitBase), (int32)CharAnimState.GetValue(),
						(int32)UnitBase->GetUnitState(), (int32)AnimStateOverride.GetValue(),
						CurrentBlendPoint_1, CurrentBlendPoint_2, MassSpeed,
						bMassSpeedValid ? 1 : 0, LocomotionDirection);
				}
			}

			// ================================================================================
			// LUX-ANPASSUNG (26.08.2026) - Laufrichtung und Tempo. Siehe Kommentar an
			// LocomotionDirection in der .h.
			//
			// Bewusst hier und nicht im UnitAnimationProcessor: dessen AnimInstance-Zweig
			// laeuft nur bei Zustandswechseln, Richtung und Tempo brauchen aber jeden Frame
			// einen frischen Wert. Rein additiv - beschreibt nur neue Felder.
			// ================================================================================
			if (bMassSpeedValid && MassSpeed > IdleAnimSpeedThreshold)
			{
				// Geschwindigkeit in den lokalen Raum der Einheit drehen: X = vorwaerts,
				// Y = rechts. Atan2(Y, X) ergibt damit direkt den Winkel gegen die
				// Blickrichtung, 0 = vorwaerts, +/-180 = rueckwaerts.
				const FVector LokaleGeschwindigkeit =
					UnitBase->GetMassActorRotation().UnrotateVector(MassVelocity);
				LocomotionDirection = FMath::RadiansToDegrees(
					FMath::Atan2(LokaleGeschwindigkeit.Y, LokaleGeschwindigkeit.X));

				const float Referenz = FMath::Max(LocomotionReferenceSpeed, 1.0f);
				LocomotionPlayRate = FMath::Clamp(MassSpeed / Referenz,
					LocomotionMinPlayRate, LocomotionMaxPlayRate);
			}
			else
			{
				// Im Stand keinen Winkel aus dem Restrauschen ableiten - das liesse die
				// Beine auf der Stelle rotieren. Tempo zurueck auf neutral.
				LocomotionDirection = 0.0f;
				LocomotionPlayRate = 1.0f;
			}

			// Umleitung der Blendpunkte auf die Bewegung - nur wenn ausdruecklich gewuenscht.
			// Der Wert aus dem Fragment wurde weiter oben gelesen und wird hier bewusst
			// ueberschrieben; der Fragment-Wert ist zustandsbasiert und kennt keine Richtung.
			// KORREKTUR (26.09.2026): im Tod NICHT ueberschreiben - sonst faellt die
			// Todesanimation aus.
			//
			// Der Block lief bisher jeden Frame und ungefiltert. Beim Tod setzt der
			// UUnitAnimationProcessor die Mischpunkte aus der Zeile "Dead" der
			// DT_UnitAnimData auf (100, 0) - zwei Zeilen spaeter standen hier aber wieder
			// Richtung (~0) und Tempo (0). Der Dead-Zustand des AnimBP tastet seinen
			// Blendspace damit bei (0,0) ab statt bei (100,0): die Todesanimation ist
			// vorhanden und auf dem richtigen Skelett, sie wird nur nie angesteuert.
			//
			// Betrifft nur Einheiten mit bUseDirectionalLocomotion - also den
			// BP_Soldier_Weapon_AH; an den Xeno-AnimBPs steht der Schalter auf false,
			// weshalb deren Todesanimation immer lief.
			//
			// CharAnimState ist hier schon frisch: es wird weiter oben aus
			// UnitBase->GetUnitState() gesetzt.
			if (bUseDirectionalLocomotion && CharAnimState != UnitData::Dead)
			{
				// Bewusst NICHT CurrentBlendPoint: das sind die Zustandswerte fuer BS_Wraith.
				// Siehe LocomotionBlendPoint_1 in der .h.
				LocomotionBlendPoint_1 = LocomotionDirection;
				LocomotionBlendPoint_2 = bMassSpeedValid ? MassSpeed : 0.0f;
			}

			// Diagnose: einmal je Einheit beim Uebergang in den Tod. Zeigt, ob die
			// Mischpunkte der Dead-Zeile jetzt wirklich stehenbleiben.
			if (CharAnimState == UnitData::Dead && !bTodesmischpunkteGemeldet)
			{
				bTodesmischpunkteGemeldet = true;
				UE_LOG(LogTemp, Warning,
					TEXT("[TodAnim] %s Zustand=Dead Richtungsmodus=%d Mischpunkte=(%.1f, %.1f)"),
					*GetNameSafe(UnitBase), bUseDirectionalLocomotion ? 1 : 0,
					CurrentBlendPoint_1, CurrentBlendPoint_2);
			}
			// ===================== ENDE LUX-ANPASSUNG (26.08.2026) ==========================

			// Steht die Einheit, sieht sie auch stehend aus - selbst wenn ihr Zustand Laufen sagt.
			//
			// Run/Chase/PatrolRandom und die GoTo-Zustaende bleiben aktiv, waehrend die Einheit
			// stillsteht: die Pfadsuche laeuft, das Ziel ist erreicht, oder der Weg ist versperrt.
			// Das AnimBP zeigte trotzdem die Laufanimation - Laufen auf der Stelle. Geaendert wird
			// nur die an das AnimBP gemeldete Anzeige, der Zustand der Einheit bleibt unberuehrt.
			bool bAufIdleKorrigiert = false;
			if (bMassSpeedValid && MassSpeed <= IdleAnimSpeedThreshold)
			{
				switch (CharAnimState.GetValue())
				{
				case UnitData::Run:
				case UnitData::Chase:
				case UnitData::Patrol:
				case UnitData::PatrolRandom:
				case UnitData::GoToBase:
				case UnitData::GoToBuild:
				case UnitData::GoToResourceExtraction:
					CharAnimState = UnitData::Idle;
					bAufIdleKorrigiert = true;
					break;
				default:
					break;
				}
			}

			// Die Mischpunkte MUESSEN mit, sonst ist die Korrektur darueber wirkungslos.
			//
			// Am 14.09.2026 am AnimBP nachgesehen: der General-Zustand von BP_UnitBaseAnimVector
			// enthaelt genau drei Knoten - CurrentBlendPoint_1 auf X, CurrentBlendPoint_2 auf Y,
			// und den Blendspace. CharAnimState kommt darin NICHT vor. Die Laufpose haengt also
			// allein an den Mischpunkten; ein umgeschriebenes CharAnimState aendert an ihr nichts.
			// Im Blendspace BS_Vector liegt (25,75) auf Idle_NonCombat und (75,75) auf Jog_Fwd.
			//
			// Gemessen sah das so aus:
			//   [AnimUebergabe] V0 Aktor=6 Anzeige=0 Tempo=4.0 | Ziel=(75.0,75.0) Jetzt=(75.0,75.0)
			// Anzeige stand also bereits auf Idle, die Mischpunkte aber weiter auf Laufen - und
			// gespielt wird, was in den Mischpunkten steht.
			//
			// Der Grund fuer die Luecke: die Mischpunkte setzt der UUnitAnimationProcessor, und
			// dessen Stillstandserkennung verlangt 0,5 s durchgehend gemessenen Stillstand. Die
			// Erkennung hier arbeitet dagegen sofort auf dem Geschwindigkeitsfragment. In dem
			// Fenster dazwischen widersprechen sich beide - und die Mischpunkte gewinnen.
			//
			// Deshalb hier dieselbe Zeile ziehen, aus der die Einheit auch sonst ihre Werte
			// bezieht: die Idle-Zeile ihrer eigenen Animationstabelle. Keine festen Zahlen, damit
			// Einheiten mit abweichendem Blendspace weiter stimmen.
			if (bAufIdleKorrigiert && AnimDataTable)
			{
				static const FString Kontext(TEXT("UnitBaseAnimInstance Idle"));
				if (const FUnitAnimData* IdleZeile = AnimDataTable->FindRow<FUnitAnimData>(FName("Idle"), Kontext, false))
				{
					BlendPoint_1 = IdleZeile->BlendPoint_1;
					BlendPoint_2 = IdleZeile->BlendPoint_2;
					CurrentBlendPoint_1 = IdleZeile->BlendPoint_1;
					CurrentBlendPoint_2 = IdleZeile->BlendPoint_2;
				}
			}
			// ================================================================================
			// Waehrend eines Casts mit laufender Montage gewinnt die Montage.
			//
			// Im AnimGraph haengen Laufblendspace und Montage-Slot nebeneinander in einem
			// LayeredBoneBlend. Laeuft die Laufanimation weiter, ueberlagert sie die Montage in
			// allen Knochen, die nicht im Layer stehen - beim Nachladen sah man dann wieder die
			// Laufbewegung. Die Einheit KANN sich waehrend des Casts ohnehin nicht bewegen, also
			// wird die Bewegungsseite hier stillgelegt.
			//
			// Bewusst an den Casting-Zustand gebunden und nicht an "irgendeine Montage": das
			// Schiessen laeuft ebenfalls ueber eine Montage, darf aber im Laufen stattfinden.
			// ================================================================================
			// [AnimUebergabe] - das LETZTE Glied der Kette, alle 2 s je Einheit.
			//
			// Bis hierher ist am 14.09.2026 alles nachgewiesen richtig: das Fragment liefert die
			// Idle-Zeile (25,75), LastProcessedState steht auf Idle, die Ueberblendung ist
			// durchgelaufen, und die Korrektur oben setzt CharAnimState ebenfalls auf Idle.
			// Trotzdem laeuft die Laufanimation. Was hier ausgegeben wird, ist genau das, was das
			// AnimBP bekommt - steht darin Idle und es laeuft dennoch, liegt es im AnimBP-Graph.
			{
				static TMap<TWeakObjectPtr<AUnitBase>, float> NaechsteAusgabe;
				const float Jetzt = UnitBase->GetWorld() ? UnitBase->GetWorld()->GetTimeSeconds() : 0.f;
				float& Faellig = NaechsteAusgabe.FindOrAdd(UnitBase);
				// Seit 16.09.2026 standardmaessig NUR fuer die ausgewaehlte Einheit (Schalter
				// RTS.StandDiagNurAuswahl, geteilt mit [StandDiag] im Processor). Dafuer haeufiger:
				// bei einer einzelnen Einheit kostet ein halbsekuendlicher Takt nichts und zeigt
				// den Verlauf, statt nur alle 2 s eine Momentaufnahme.
				static IConsoleVariable* CVarNurAuswahl =
					IConsoleManager::Get().FindConsoleVariable(TEXT("RTS.StandDiagNurAuswahl"));
				const bool bNurAuswahl = !CVarNurAuswahl || CVarNurAuswahl->GetInt() != 0;
				const bool bDarfSchreiben = !bNurAuswahl || RTSDiagIstAusgewaehlt(UnitBase);
				if (bDarfSchreiben && bMassSpeedValid && MassSpeed <= IdleAnimSpeedThreshold && Jetzt >= Faellig)
				{
					Faellig = Jetzt + (bNurAuswahl ? 0.5f : 2.f);
				}
			}

			if (CharAnimState == UnitData::Casting && IsAnyMontagePlaying())
			{
				if (!bMontageHaltActive)
				{
					bMontageHaltActive = true;
					UE_LOG(LogTemp, Log,
						TEXT("[AnimInstance] %s: Montage laeuft im Cast - Laufanimation ausgesetzt (%s)"),
						*UnitBase->GetName(),
						GetCurrentActiveMontage() ? *GetCurrentActiveMontage()->GetName() : TEXT("?"));
				}
				CharAnimState = UnitData::Idle;
				LocomotionDirection = 0.0f;
				LocomotionPlayRate = 1.0f;
				CurrentBlendPoint_1 = 0.0f;
				CurrentBlendPoint_2 = 0.0f;
				BlendPoint_1 = 0.0f;
				BlendPoint_2 = 0.0f;
				MassSpeed = 0.0f;
			}
			else if (bMontageHaltActive)
			{
				bMontageHaltActive = false;
			}

			// SetBlendPoints(UnitBase, Deltaseconds); // Processor übernimmt das jetzt

			if(LastAnimState != CharAnimState)
			{
				SoundTimer = 0.f;
				LastAnimState = CharAnimState;

				// Sound wird jetzt über den UnitAnimationProcessor im Fragment gesetzt.
			}
			
			if(Sound && UnitBase)
			{
				if(SoundTimer == 0.f)
					UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetMassActorLocation(), 1.f);

				SoundTimer += Deltaseconds;
				
			}
		}
	}
}


void UUnitBaseAnimInstance::SetBlendPoints(AUnitBase* Unit, float Deltaseconds)
{
	TEnumAsByte<UnitData::EState> AnimState = Unit->GetUnitState();
	
	if (AnimDataTable)
	{
		for(auto it : AnimDataTable->GetRowMap())
		{
			FString Key = it.Key.ToString();
			UnitAnimData = reinterpret_cast<FUnitAnimData*>(it.Value);
			if(UnitAnimData->AnimState == AnimState && UnitAnimData->AnimState != UnitData::Speaking)
			{
				BlendPoint_1 = UnitAnimData->BlendPoint_1;
				BlendPoint_2 = UnitAnimData->BlendPoint_2;
				TransitionRate_1 = UnitAnimData->TransitionRate_1;
				TransitionRate_2 = UnitAnimData->TransitionRate_2;
				Resolution_1 = UnitAnimData->Resolution_1;
				Resolution_2 = UnitAnimData->Resolution_2;
				Sound = UnitAnimData->Sound;
			}else
			{
				ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);

				if(SpeakingUnit && SpeakingUnit->SpeechBubble)
				{
					SpeakingUnit->SpeechBubble->AnimationTime += Deltaseconds;
					if(SpeakingUnit->SpeechBubble->AnimationTime <= SpeakingUnit->SpeechBubble->MaxAnimationTime)
					{
						BlendPoint_1 = SpeakingUnit->SpeechBubble->BlendPoint_1;
						BlendPoint_2 = SpeakingUnit->SpeechBubble->BlendPoint_2;
					}else
					{
						BlendPoint_1 = UnitAnimData->BlendPoint_1;
						BlendPoint_2 = UnitAnimData->BlendPoint_2;
					}
					

					TransitionRate_1 = UnitAnimData->TransitionRate_1;
					TransitionRate_2 = UnitAnimData->TransitionRate_2;
					Resolution_1 = UnitAnimData->Resolution_1;
					Resolution_2 = UnitAnimData->Resolution_2;
				}
			}
		}
		
		
	}

}

void UUnitBaseAnimInstance::SetAnimStateOverride(TEnumAsByte<UnitData::EState> NewState, float DurationSeconds)
{
	AnimStateOverride = NewState;
	const UWorld* World = GetWorld();
	AnimStateOverrideUntil = (World ? World->GetTimeSeconds() : 0.f) + FMath::Max(0.f, DurationSeconds);
}
