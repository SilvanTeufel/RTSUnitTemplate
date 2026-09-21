// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DamageNumberSubsystem.generated.h"

/**
 * Eine schwebende Schadenszahl - reine Daten, kein UObject, kein Aktor, kein Tick.
 */
USTRUCT()
struct FDamageNumberEntry
{
	GENERATED_BODY()

	/** Startpunkt in der Welt, bereits mit dem Versatz ueber der Einheit. */
	FVector StartLocation = FVector::ZeroVector;

	/** Seitliche Abdrift je Sekunde - ersetzt das Zufallszittern des alten Aktors. */
	FVector2D DriftPerSecond = FVector2D::ZeroVector;

	float Damage = 0.f;
	FLinearColor HighColor = FLinearColor::Red;
	FLinearColor LowColor = FLinearColor::White;
	float ColorOffset = 0.f;

	/** Weltzeit der Entstehung. Alter und Ausblenden haengen daran. */
	float SpawnTime = 0.f;

	bool bActive = false;
};

/**
 * Haelt alle sichtbaren Schadenszahlen EINER Welt.
 *
 * Ersetzt seit dem 20.09.2026 den alten Weg, je Treffer einen AIndicatorActor mit
 * UWidgetComponent zu spawnen. Der kostete pro Zahl einen Aktor, einen eigenen Slate-Baum,
 * einen Tick und einen zuverlaessigen Multicast - bei einem Gefecht mit hunderten Treffern je
 * Sekunde der teuerste denkbare Weg.
 *
 * Hier liegt stattdessen ein Ringpuffer fester Groesse. Gezeichnet wird einmal je Bild fuer
 * alle Zahlen zusammen (AHUDBase::DrawDamageNumbers). Zahlen sind ohnehin bildschirmgross und
 * sollen nicht perspektivisch schrumpfen - der Umweg ueber 3D-Widgets kaufte nichts.
 *
 * NEBEL DES KRIEGES: hier wird NICHT gefiltert. Der Filter sitzt weiterhin in
 * APerformanceUnit::SpawnDamageIndicator, also je Client vor dem Einreihen - genau dort, wo er
 * vorher auch sass.
 */
UCLASS()
class RTSUNITTEMPLATE_API UDamageNumberSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Reiht eine Zahl ein. Ist der Ringpuffer voll, wird der aelteste Eintrag ueberschrieben -
	 * in einer Schlacht ist die aelteste Zahl ohnehin die, die gleich verschwindet.
	 */
	void AddNumber(const FVector& WorldLocation, float Damage,
	               FLinearColor HighColor, FLinearColor LowColor, float ColorOffset);

	/** Alle Eintraege; der Zeichner ueberspringt die inaktiven und die abgelaufenen selbst. */
	const TArray<FDamageNumberEntry>& GetEntries() const { return Entries; }

	/** Lebensdauer einer Zahl in Sekunden. Entspricht AIndicatorActor::MaxLifeTime. */
	static constexpr float MaxLifeSeconds = 2.f;

	/** Steiggeschwindigkeit. Der alte Aktor stieg ZDrift (5 uu) JE TICK - bei 60 Bildern also 300 uu/s. */
	static constexpr float RisePerSecond = 300.f;

	/** Versatz ueber der Einheit. Entspricht AIndicatorActor::DamageIndicatorCompLocation. */
	static constexpr float HeightOffset = 50.f;

private:
	/**
	 * Feste Groesse, bewusst kein TArray, das mitwaechst: eine Schlacht darf die Bildrate nicht
	 * dadurch druecken, dass sich Zahlen aufstauen. 256 gleichzeitig sichtbare Zahlen sind mehr,
	 * als ein Mensch lesen kann.
	 */
	static constexpr int32 Capacity = 256;

	UPROPERTY()
	TArray<FDamageNumberEntry> Entries;

	int32 NextIndex = 0;
};
