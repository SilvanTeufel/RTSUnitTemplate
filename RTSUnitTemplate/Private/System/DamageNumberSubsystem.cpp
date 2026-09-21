// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/DamageNumberSubsystem.h"
#include "Engine/World.h"

void UDamageNumberSubsystem::AddNumber(const FVector& WorldLocation, float Damage,
                                       FLinearColor HighColor, FLinearColor LowColor, float ColorOffset)
{
	const UWorld* World = GetWorld();
	if (!World || Damage <= 0.f)
	{
		return;
	}

	if (Entries.Num() < Capacity)
	{
		Entries.SetNum(Capacity);
	}

	FDamageNumberEntry& Entry = Entries[NextIndex];
	NextIndex = (NextIndex + 1) % Capacity;

	Entry.StartLocation = WorldLocation + FVector(0.f, 0.f, HeightOffset);
	Entry.Damage = Damage;
	Entry.HighColor = HighColor;
	Entry.LowColor = LowColor;
	Entry.ColorOffset = ColorOffset;
	Entry.SpawnTime = World->GetTimeSeconds();
	Entry.bActive = true;

	// Seitliche Abdrift EINMAL wuerfeln statt je Bild.
	//
	// Der alte Aktor zog je Tick einen neuen Zufallswert zwischen -10 und +10 - ein Zittern, das
	// mit der Bildrate schneller wurde und im Mittel nirgendwohin fuehrte. Eine feste Richtung je
	// Zahl sieht auf dem Schirm gleich aus, ist aber von der Bildrate unabhaengig und kostet
	// nichts.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Speed = FMath::FRandRange(20.f, 60.f);
	Entry.DriftPerSecond = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
}
