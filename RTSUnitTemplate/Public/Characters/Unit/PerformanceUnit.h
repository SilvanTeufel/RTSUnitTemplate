// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
//#include "LevelUnit.h"
#include "MassEntityTypes.h"
#include "MassUnitBase.h"
#include "GameFramework/Character.h"
#include "Core/UnitData.h"
#include "Actors/DijkstraCenter.h"
#include "Core/DijkstraMatrix.h"
#include "Actors/Projectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/EngineTypes.h"
#include "Mass/UnitMassTag.h"
#include "PerformanceUnit.generated.h"

class UNiagaraComponent;
class UAudioComponent;
struct FTimerHandle;
class UUnitBaseHealthBar;
class UUnitTimerWidget;

USTRUCT(BlueprintType)
struct FActiveNiagaraEffect
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UNiagaraComponent> Component;

	UPROPERTY()
	int32 Id;

	FActiveNiagaraEffect() : Component(nullptr), Id(-1) {}
	FActiveNiagaraEffect(UNiagaraComponent* InComponent, int32 InId) : Component(InComponent), Id(InId) {}
};

UCLASS()
class RTSUNITTEMPLATE_API APerformanceUnit : public AMassUnitBase, public IMassVisibilityInterface
{
	GENERATED_BODY()

public:
	APerformanceUnit(const FObjectInitializer& ObjectInitializer);
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraSystem* MeleeImpactVFX;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MeeleImpactVFXDelay = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraSystem* DeadVFX;

	/** Optional pool of additional death VFX. When non-empty, ONE entry is picked at random per
	 *  death; the single DeadVFX above is added to that pool (so it still fires and no existing
	 *  assignment needs redoing). All entries share ScaleDeadVFX / DelayDeadVFX / DeadSound below.
	 *  Purely cosmetic, so each machine picks its own (host and clients may show different ones).
	 *  Leave empty to keep the classic single-DeadVFX behaviour. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<UNiagaraSystem*> DeadVFXArray;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ScaleDeadVFX = FVector(1.f, 1.f,1.f);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ScaleImpactVFX = FVector(1.f, 1.f,1.f);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator RotateImpactVFX = FRotator::ZeroRotator;
	
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* MeleeImpactSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MeleeImpactSoundDelay = 0.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DeadSound;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ScaleImpactSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ScaleDeadSound = 1.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelayDeadVFX = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelayDeadSound = 0.f;
	
	virtual void BeginPlay() override;

	virtual void Destroyed() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerMultiplier = FVector(0.01, 0.01, 200);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector FogManagerPositionOffset = FVector(0, 0, 50.f);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool StopVisibilityTick = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsInitialized = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnViewport = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsMyTeam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnemyStartVisibility = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsVisibleEnemy = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsInvisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogDeadVisibilityTime = 10.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnableFog = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int FogManagerOverlaps = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int DetectorOverlaps = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityOffset = 150.f;

	void UpdateWidgetPositions(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	virtual void SetCharacterVisibility(bool desiredVisibility);

	/**
	 * Versteckt den Koerper SOFORT, auch bei Mass-Einheiten.
	 *
	 * AActor::SetHidden reicht dafuer nicht: der sichtbare Koerper einer Mass-Einheit ist eine
	 * Instanz in einem gepoolten ISM des UUnitVisualManager, kein Actor-Mesh. Ein verstecktes
	 * Actor-Mesh aendert an der ISM-Instanz nichts - die Einheit bleibt stehen, wo sie war.
	 *
	 * Gedacht fuer den Moment, in dem etwas verschwinden soll, bevor die uebliche
	 * Sichtbarkeitsrechnung (Nebel, Bildausschnitt) das naechste Mal laeuft.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HideMassVisualNow();

	// --------------------------------------------------------------------------------------------
	// Meldet die Sichtbarkeit dieser Einheit im Nebel des Krieges an Blueprints.
	//
	// Gedacht fuer Anhaengsel, die der Einheit folgen sollen, aber nicht an ihren Komponenten
	// haengen - im AstraHelix-Projekt etwa der Blob-Schatten aus MaterialDrivenShadows.
	// Die Komponentensichtbarkeit taugt dafuer NICHT: Mass-Einheiten werden als gepoolte ISM
	// gezeichnet, ihre Actor-Komponenten bleiben dauerhaft unsichtbar.
	//
	// Der Wert ist bewusst der INHAERENTE (Nebel des Krieges), nicht der lokale mit
	// Viewport-Anteil: sonst flackert das Anhaengsel, sobald die Einheit kurz aus dem Bild
	// rutscht. Und er wird auf JEDER Maschine einzeln bestimmt - Server und Client haben
	// verschiedene Sicht, jeder soll seine eigenen Anhaengsel sehen.
	// --------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitTemplate|Sichtbarkeit")
	void OnFogVisibilityChanged(bool bVisible);


	
	/** Synchronizes visibility of any attached assets (e.g. WorkResource mesh) with the unit's local visibility state. */
	virtual void SyncAttachedAssetsVisibility() {}

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void VisibilityTickFog();

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnDamageIndicator(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset);
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ShowWorkAreaIfNoFog(AWorkArea* WorkArea);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ShowAbilityIndicator(AAbilityIndicator* AbilityIndicator);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AbilityIndicatorVisibility = false;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void HideAbilityIndicator(AAbilityIndicator* AbilityIndicator);
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffects(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay = 0.f, float SoundDelay = 0.f, int32 ID = -1);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireEffectsAtLocation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, const FVector Location, float KillDelay, FRotator Rotation = FRotator(0.0f, 0.0f, 0.0f), float EffectDelay = 0.f, float SoundDelay = 0.f, int32 ID = -1);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void StopAllEffects(bool bFadeAudio = true, float FadeTime = 0.15f);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void StopNiagaraByID(int32 ID, float FadeTime = 0.15f);
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckHealthBarVisibility();

	// Helpers to handle specific healthbar modes (no Blueprint exposure)
	void HandleStandardHealthBarVisibility();
	void HandleSquadHealthBarVisibility();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckTimerVisibility();
		
	virtual ENetMode GetUnitNetMode() const;
	
	UPROPERTY()
	uint8 NetModeOverride = 255;

	private:
		
		UPROPERTY(Transient)
		TArray<FActiveNiagaraEffect> ActiveNiagara;
		
		UPROPERTY(Transient)
		TArray<TWeakObjectPtr<UAudioComponent>> ActiveAudio;
		
	UPROPERTY(Transient)
		TArray<FTimerHandle> PendingEffectTimers;

		void StopNiagaraComponent(class UNiagaraComponent* NC, float FadeTime);
		void StopAudioComponent(class UAudioComponent* AC, bool bFade, float FadeTime);
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AProjectile> ProjectileBaseClass;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AProjectile* Projectile;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	APlayerController* OwningPlayerController;
	
	
	// IMassVisibilityInterface
	virtual void SetActorVisibility(bool bVisible) override { SetCharacterVisibility(bVisible); }
	virtual void SetEnemyVisibility(AActor* DetectingActor, bool bVisible) override;
	virtual bool ComputeLocalVisibility() const override;
	virtual bool ComputeInherentVisibility() const override;
	// End IMassVisibilityInterface
	
};
