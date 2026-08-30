// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Core/UnitData.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "UnitBaseAnimInstance.generated.h"

USTRUCT(BlueprintType)
struct FUnitAnimData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> AnimState = UnitData::Idle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound = nullptr;
};

/**
 * 
 */
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType)
class RTSUNITTEMPLATE_API UUnitBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UUnitBaseAnimInstance();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TEnumAsByte<UnitData::EState> CharAnimState;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TEnumAsByte<UnitData::EState> LastAnimState = UnitData::None;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_1 = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_2 = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CurrentBlendPoint_1 = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CurrentBlendPoint_2 = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_1 = 0.5;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_2 = 0.5;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_1 = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_2 = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float ContinuousPlayRate = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float ContinuousAnimationPosition = 0.0f;

	// ============================================================================================
	// LUX-ANPASSUNG (16.08.2026) — Mass-Geschwindigkeit fuer den AnimBP verfuegbar machen.
	// Beim Uebernehmen ins Original-Template mitnehmen.
	//
	// Hintergrund: AActor::GetVelocity() ist bei diesen Einheiten IMMER null. Mass bewegt den
	// Actor per SetActorTransform (ActorTransformSyncProcessor), niemand schreibt die
	// CharacterMovement-Velocity - eine Volltextsuche danach liefert keinen Treffer. Wer im
	// AnimBP mit Geschwindigkeit arbeiten will, hatte bisher keine Quelle und musste sie aus
	// der Positionsdifferenz je Frame selbst ausrechnen.
	//
	// Rein additiv: nur neue, lesbare Werte. Es aendert sich kein Verhalten, weder im RTS-
	// noch im Hero-Modus. Befuellt in NativeUpdateAnimation aus dem FMassVelocityFragment,
	// genau dort, wo auch die Blendpunkte aus dem Fragment gezogen werden.
	// ============================================================================================

	// Geschwindigkeit in uu/s in der XY-Ebene (ohne Fallen/Steigen). Fuer Lauf-Blends.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float MassSpeed = 0.0f;

	/**
	 * True, solange MassSpeed in DIESEM Frame aus dem Velocity-Fragment gelesen wurde.
	 *
	 * Ohne diese Pruefung wuerde eine Einheit ohne Mass-Entity (Hero-Modus, gerade zerstoerte
	 * Entity) mit dem alten MassSpeed-Wert weiterrechnen und dauerhaft idle aussehen.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bMassSpeedValid = false;

	/**
	 * Unterhalb dieser Geschwindigkeit meldet CharAnimState Idle, obwohl die Einheit in einem
	 * Laufzustand steht.
	 *
	 * Verhindert das Laufen auf der Stelle: Run, Chase, PatrolRandom und die GoTo-Zustaende
	 * behalten ihre Laufanimation, solange sich die Einheit auch wirklich bewegt. Steht sie
	 * (Pfadsuche laeuft, Ziel erreicht, blockiert), sieht sie stehend aus. Der ZUSTAND selbst
	 * bleibt unangetastet - das hier ist rein optisch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (ClampMin = "0.0"))
	float IdleAnimSpeedThreshold = 5.0f;

	// Vollstaendiger Geschwindigkeitsvektor in Weltkoordinaten. Fuer Laufrichtung
	// (z. B. Strafe-Winkel gegen die Blickrichtung).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	FVector MassVelocity = FVector::ZeroVector;
	// ===================== ENDE LUX-ANPASSUNG ===================================================

	// ============================================================================================
	// LUX-ANPASSUNG (26.08.2026) - Laufrichtung und Tempo fuer richtungsabhaengige Blendspaces.
	// Beim Uebernehmen ins Original-Template mitnehmen.
	//
	// Hintergrund: Die Blendpunkte des Systems haengen am ZUSTAND - der UnitAnimationProcessor
	// setzt TargetBlendPoint_1/_2 nur bei einem Zustandswechsel aus einer DataTable-Zeile. Damit
	// laesst sich keine Bewegungsrichtung darstellen: eine seitwaerts laufende Einheit steht im
	// selben Zustand wie eine vorwaerts laufende.
	//
	// Diese Werte leiten Richtung und Tempo aus MassVelocity ab (AActor::GetVelocity() ist hier
	// immer null, siehe Kommentar an MassSpeed). Rein additiv: nur neue, lesbare Werte, kein
	// Eingriff in den geteilten Processor. Wer sie nicht im AnimGraph verdrahtet, merkt nichts.
	// ============================================================================================

	/**
	 * Laufrichtung relativ zur Blickrichtung in Grad: 0 = vorwaerts, +90 = rechts,
	 * -90 = links, +/-180 = rueckwaerts. Als X-Achse eines Richtungs-Blendspace gedacht.
	 *
	 * Steht die Einheit (Tempo unter IdleAnimSpeedThreshold), bleibt der Wert 0 - ohne das
	 * wuerde der Winkel beim Stehen aus dem Rauschen der Restgeschwindigkeit springen und die
	 * Beine im Stand rotieren lassen.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float LocomotionDirection = 0.0f;

	/**
	 * Abspieltempo der Laufanimation, gekoppelt an die tatsaechliche Geschwindigkeit
	 * (MassSpeed / LocomotionReferenceSpeed). Gegen Rutschen der Fuesse.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float LocomotionPlayRate = 1.0f;

	/** Geschwindigkeit, bei der die Laufanimation mit Tempo 1.0 laeuft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (ClampMin = "1.0"))
	float LocomotionReferenceSpeed = 600.0f;

	/** Untere und obere Schranke fuer LocomotionPlayRate, damit nichts einfriert oder flimmert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (ClampMin = "0.01"))
	float LocomotionMinPlayRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (ClampMin = "0.01"))
	float LocomotionMaxPlayRate = 2.0f;

	/**
	 * Speist CurrentBlendPoint_1/_2 aus LocomotionDirection/MassSpeed statt aus der
	 * zustandsbasierten DataTable-Zeile.
	 *
	 * Nur fuer AnimBPs gedacht, deren Blendspace eine Richtungsachse hat. Default aus, damit
	 * jede bestehende Einheit in allen drei Projekten unveraendert weiterlaeuft - der
	 * UnitAnimationProcessor wird dafuer NICHT angefasst.
	 *
	 * Warum die Umleitung hier und nicht im AnimGraph: die vorhandenen Get-Nodes lesen bereits
	 * CurrentBlendPoint_1/_2: es genuegt, diese beiden Werte anders zu befuellen, statt Nodes
	 * umzuhaengen (was ueber die Werkzeuge ohnehin nicht geht - der State-Graph laesst sich
	 * nicht als Blueprint aufloesen).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bUseDirectionalLocomotion = false;
	// ===================== ENDE LUX-ANPASSUNG (26.08.2026) ======================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ContinuousAttackSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ContinuousAttackStartDelayMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ContinuousAttackCycleRatio = 0.8f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float SoundTimer = 0.f;
/*
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ControlTimer = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateTime = 0.1f;
	*/
	UFUNCTION()
	virtual void NativeInitializeAnimation() override;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UFUNCTION()
	virtual void NativeUpdateAnimation(float Deltaseconds) override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UDataTable* AnimDataTable;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBlendPoints(AUnitBase* Unit, float Deltaseconds);

	FUnitAnimData* UnitAnimData;


};