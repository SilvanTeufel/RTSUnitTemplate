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

	// Vollstaendiger Geschwindigkeitsvektor in Weltkoordinaten. Fuer Laufrichtung
	// (z. B. Strafe-Winkel gegen die Blickrichtung).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	FVector MassVelocity = FVector::ZeroVector;
	// ===================== ENDE LUX-ANPASSUNG ===================================================

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