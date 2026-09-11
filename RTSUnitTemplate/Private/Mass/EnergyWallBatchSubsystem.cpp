// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/EnergyWallBatchSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

#include "Actors/EnergyWall.h"
#include "Characters/Unit/BuildingBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

UEnergyWallBatchSubsystem* UEnergyWallBatchSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UEnergyWallBatchSubsystem>() : nullptr;
}

bool UEnergyWallBatchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UEnergyWallBatchSubsystem::Deinitialize()
{
	for (int32 i = 0; i < static_cast<int32>(EEnergyWallPart::Anzahl); ++i)
	{
		ISMs[i] = nullptr;
		FreieIndizes[i].Reset();
	}

	if (IsValid(BatchAktor))
	{
		BatchAktor->Destroy();
	}
	BatchAktor = nullptr;
	BelegteAnzahl = 0;

	Super::Deinitialize();
}

UInstancedStaticMeshComponent* UEnergyWallBatchSubsystem::HoleOderBaue(EEnergyWallPart Teil,
	const UInstancedStaticMeshComponent* Vorlage)
{
	const int32 Index = static_cast<int32>(Teil);
	if (Index < 0 || Index >= static_cast<int32>(EEnergyWallPart::Anzahl))
	{
		return nullptr;
	}

	if (ISMs[Index])
	{
		// Abweichendes Mesh einmal melden. Ein Batch kann nur EIN Mesh je Teil zeichnen; kaeme eine
		// Wand mit einem anderen, waere sie im falschen Aussehen zu sehen - das gehoert ins Log und
		// nicht stillschweigend uebergangen.
		if (Vorlage && Vorlage->GetStaticMesh() && ISMs[Index]->GetStaticMesh() != Vorlage->GetStaticMesh()
			&& !bMeshAbweichungGemeldet[Index])
		{
			bMeshAbweichungGemeldet[Index] = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[EnergyWallBatch] Teil %d: eine Wand bringt ein anderes Mesh mit ('%s' statt '%s'). ")
				TEXT("Der gemeinsame Batch zeichnet weiterhin das zuerst gesehene."),
				Index, *Vorlage->GetStaticMesh()->GetName(), *ISMs[Index]->GetStaticMesh()->GetName());
		}
		return ISMs[Index];
	}

	UWorld* World = GetWorld();
	if (!World || !Vorlage || !Vorlage->GetStaticMesh())
	{
		return nullptr;
	}

	if (!IsValid(BatchAktor))
	{
		FActorSpawnParameters Parameter;
		Parameter.Name = TEXT("EnergyWallBatch");
		Parameter.ObjectFlags |= RF_Transient;
		BatchAktor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameter);

		if (!IsValid(BatchAktor))
		{
			return nullptr;
		}

		BatchAktor->SetActorLabel(TEXT("EnergyWallBatch"));
		// Der Aktor ist eine reine Zeichenhuelle: keine Replikation, keine Kollision, kein Speichern.
		BatchAktor->SetReplicates(false);
		USceneComponent* Wurzel = NewObject<USceneComponent>(BatchAktor, TEXT("BatchRoot"));
		Wurzel->RegisterComponent();
		BatchAktor->SetRootComponent(Wurzel);
	}

	const FName Name = *FString::Printf(TEXT("EnergyWallISM_%d"), Index);
	UInstancedStaticMeshComponent* Neu = NewObject<UInstancedStaticMeshComponent>(BatchAktor, Name);
	if (!Neu)
	{
		return nullptr;
	}

	Neu->SetStaticMesh(Vorlage->GetStaticMesh());

	// Materialien der Vorlage uebernehmen. Sie liegen im Blueprint der Wand; das Plugin darf sie
	// nicht kennen.
	const int32 Materialzahl = Vorlage->GetNumMaterials();
	for (int32 m = 0; m < Materialzahl; ++m)
	{
		if (UMaterialInterface* Material = Vorlage->GetMaterial(m))
		{
			Neu->SetMaterial(m, Material);
		}
	}

	// Zwei Custom-Data-Werte je Instanz: Aufloesebeginn und Sichtbarkeit. Das Material liest sie
	// ueber PerInstanceCustomData - siehe Kopfkommentar.
	Neu->NumCustomDataFloats = CustomDataAnzahl;

	Neu->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Neu->SetMobility(EComponentMobility::Movable);
	Neu->SetCastShadow(Vorlage->CastShadow);
	Neu->SetupAttachment(BatchAktor->GetRootComponent());
	Neu->RegisterComponent();

	ISMs[Index] = Neu;
	return Neu;
}

int32 UEnergyWallBatchSubsystem::BelegePlatz(EEnergyWallPart Teil,
	const UInstancedStaticMeshComponent* Vorlage, const FTransform& WeltTransform)
{
	UInstancedStaticMeshComponent* ISM = HoleOderBaue(Teil, Vorlage);
	if (!ISM)
	{
		return INDEX_NONE;
	}

	const int32 TeilIndex = static_cast<int32>(Teil);
	int32 Platz = INDEX_NONE;

	if (FreieIndizes[TeilIndex].Num() > 0)
	{
		Platz = FreieIndizes[TeilIndex].Pop(EAllowShrinking::No);
		ISM->UpdateInstanceTransform(Platz, WeltTransform, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	}
	else
	{
		Platz = ISM->AddInstance(WeltTransform, /*bWorldSpace=*/true);
	}

	if (Platz == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	ISM->SetCustomDataValue(Platz, CustomDataDespawnStart, -1.f, /*bMarkRenderStateDirty=*/false);
	ISM->SetCustomDataValue(Platz, CustomDataSichtbar, 1.f, /*bMarkRenderStateDirty=*/true);

	++BelegteAnzahl;
	return Platz;
}

void UEnergyWallBatchSubsystem::SetzeTransform(EEnergyWallPart Teil, int32 Index, const FTransform& WeltTransform)
{
	const int32 TeilIndex = static_cast<int32>(Teil);
	if (Index == INDEX_NONE || TeilIndex < 0 || TeilIndex >= static_cast<int32>(EEnergyWallPart::Anzahl) || !ISMs[TeilIndex])
	{
		return;
	}

	ISMs[TeilIndex]->UpdateInstanceTransform(Index, WeltTransform, /*bWorldSpace=*/true,
		/*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
}

void UEnergyWallBatchSubsystem::SetzeSichtbar(EEnergyWallPart Teil, int32 Index, bool bSichtbar)
{
	const int32 TeilIndex = static_cast<int32>(Teil);
	if (Index == INDEX_NONE || TeilIndex < 0 || TeilIndex >= static_cast<int32>(EEnergyWallPart::Anzahl) || !ISMs[TeilIndex])
	{
		return;
	}

	ISMs[TeilIndex]->SetCustomDataValue(Index, CustomDataSichtbar, bSichtbar ? 1.f : 0.f, /*bMarkRenderStateDirty=*/true);
}

void UEnergyWallBatchSubsystem::SetzeAufloesebeginn(EEnergyWallPart Teil, int32 Index, float Weltzeit)
{
	const int32 TeilIndex = static_cast<int32>(Teil);
	if (Index == INDEX_NONE || TeilIndex < 0 || TeilIndex >= static_cast<int32>(EEnergyWallPart::Anzahl) || !ISMs[TeilIndex])
	{
		return;
	}

	ISMs[TeilIndex]->SetCustomDataValue(Index, CustomDataDespawnStart, Weltzeit, /*bMarkRenderStateDirty=*/true);
}

void UEnergyWallBatchSubsystem::GibPlatzFrei(EEnergyWallPart Teil, int32 Index)
{
	const int32 TeilIndex = static_cast<int32>(Teil);
	if (Index == INDEX_NONE || TeilIndex < 0 || TeilIndex >= static_cast<int32>(EEnergyWallPart::Anzahl) || !ISMs[TeilIndex])
	{
		return;
	}

	// NICHT RemoveInstance: das schiebt die letzte Instanz in die Luecke und macht jeden anderswo
	// gemerkten Index ungueltig. Stattdessen unsichtbar machen und den Platz zur Wiederverwendung
	// zurueckgeben.
	FTransform Weg = FTransform::Identity;
	Weg.SetScale3D(FVector::ZeroVector);
	ISMs[TeilIndex]->UpdateInstanceTransform(Index, Weg, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	ISMs[TeilIndex]->SetCustomDataValue(Index, CustomDataSichtbar, 0.f, /*bMarkRenderStateDirty=*/true);

	FreieIndizes[TeilIndex].AddUnique(Index);
	BelegteAnzahl = FMath::Max(0, BelegteAnzahl - 1);
}


// ---------------------------------------------------------------------------------------------
// Diagnose
//
// `RTS.EnergyWall.Stats` zaehlt, was gerade gezeichnet wird. Zeichenaufrufe fuer diese Meshes
// entsprechen den ISM-Komponenten mit Instanzen: vorher drei JE WAND, jetzt drei INSGESAMT.
//
// `RTS.EnergyWall.BatchTest <N>` belegt N mal drei Plaetze mit einem Wuerfelmesh und meldet, wie
// viele Komponenten daraus werden - ein Nachweis des Verfahrens ohne laufende Partie. Danach gibt
// er die Plaetze wieder frei und belegt sie erneut, um zu zeigen, dass die Freiliste dieselben
// Indizes wiederverwendet statt die Instanzreihe wachsen zu lassen.
// ---------------------------------------------------------------------------------------------

static void EnergyWallBatchStats(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
{
	if (!World)
	{
		Ar.Log(TEXT("[EnergyWallBatch] Keine Welt."));
		return;
	}

	int32 Waende = 0;
	for (TActorIterator<AEnergyWall> It(World); It; ++It)
	{
		++Waende;
	}

	UEnergyWallBatchSubsystem* Batch = World->GetSubsystem<UEnergyWallBatchSubsystem>();
	if (!Batch)
	{
		Ar.Logf(TEXT("[EnergyWallBatch] %d Waende, aber kein Batch-Subsystem in dieser Welt."), Waende);
		return;
	}

	int32 Komponenten = 0;
	int32 Instanzen = 0;
	int32 Frei = 0;
	for (int32 i = 0; i < static_cast<int32>(EEnergyWallPart::Anzahl); ++i)
	{
		if (const UInstancedStaticMeshComponent* ISM = Batch->GibISM(static_cast<EEnergyWallPart>(i)))
		{
			++Komponenten;
			Instanzen += ISM->GetInstanceCount();
		}
		Frei += Batch->GibFreieAnzahl(static_cast<EEnergyWallPart>(i));
	}

	Ar.Logf(TEXT("[EnergyWallBatch] %d Waende | Batch-Komponenten %d, Instanzen %d, freie Plaetze %d, belegt %d"),
		Waende, Komponenten, Instanzen, Frei, Batch->GetBelegtePlaetze());
	Ar.Logf(TEXT("[EnergyWallBatch] Zeichenaufrufe fuer diese Meshes: vorher %d (3 je Wand), jetzt %d."),
		Waende * 3, Komponenten);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GEnergyWallBatchStatsCmd(
	TEXT("RTS.EnergyWall.Stats"),
	TEXT("Zeigt Waende, Batch-Komponenten und Instanzen der Energiewaende."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&EnergyWallBatchStats));

static void EnergyWallBatchTest(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (!World || !World->IsGameWorld())
	{
		Ar.Log(TEXT("[EnergyWallBatch] Test braucht eine laufende Spielwelt."));
		return;
	}

	UEnergyWallBatchSubsystem* Batch = World->GetSubsystem<UEnergyWallBatchSubsystem>();
	if (!Batch)
	{
		Ar.Log(TEXT("[EnergyWallBatch] Kein Batch-Subsystem in dieser Welt."));
		return;
	}

	const int32 Anzahl = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 5000) : 50;

	UStaticMesh* Wuerfel = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Wuerfel)
	{
		Ar.Log(TEXT("[EnergyWallBatch] Wuerfelmesh nicht ladbar - Test nicht moeglich."));
		return;
	}

	// Eine wegwerfbare Vorlage: sie liefert nur Mesh und Materialien, gezeichnet wird sie nie.
	UInstancedStaticMeshComponent* Vorlage = NewObject<UInstancedStaticMeshComponent>(GetTransientPackage());
	Vorlage->SetStaticMesh(Wuerfel);

	TArray<TStaticArray<int32, 3>> Plaetze;
	Plaetze.Reserve(Anzahl);

	FTransform Lage = FTransform::Identity;
	for (int32 i = 0; i < Anzahl; ++i)
	{
		Lage.SetLocation(FVector(i * 200.f, 0.f, 0.f));

		TStaticArray<int32, 3> Drei;
		for (int32 t = 0; t < 3; ++t)
		{
			Drei[t] = Batch->BelegePlatz(static_cast<EEnergyWallPart>(t), Vorlage, Lage);
		}
		Plaetze.Add(Drei);
	}

	int32 Komponenten = 0;
	int32 Instanzen = 0;
	for (int32 t = 0; t < 3; ++t)
	{
		if (const UInstancedStaticMeshComponent* ISM = Batch->GibISM(static_cast<EEnergyWallPart>(t)))
		{
			++Komponenten;
			Instanzen += ISM->GetInstanceCount();
		}
	}

	Ar.Logf(TEXT("[EnergyWallBatch] Test: %d Wandsaetze belegt -> %d Komponenten, %d Instanzen. Einzeln waeren es %d Komponenten."),
		Anzahl, Komponenten, Instanzen, Anzahl * 3);

	// Freigeben und erneut belegen: die Instanzreihe darf dabei NICHT wachsen.
	for (const TStaticArray<int32, 3>& Drei : Plaetze)
	{
		for (int32 t = 0; t < 3; ++t)
		{
			Batch->GibPlatzFrei(static_cast<EEnergyWallPart>(t), Drei[t]);
		}
	}

	for (int32 i = 0; i < Anzahl; ++i)
	{
		Lage.SetLocation(FVector(i * 200.f, 500.f, 0.f));
		for (int32 t = 0; t < 3; ++t)
		{
			Batch->BelegePlatz(static_cast<EEnergyWallPart>(t), Vorlage, Lage);
		}
	}

	int32 InstanzenNachher = 0;
	for (int32 t = 0; t < 3; ++t)
	{
		if (const UInstancedStaticMeshComponent* ISM = Batch->GibISM(static_cast<EEnergyWallPart>(t)))
		{
			InstanzenNachher += ISM->GetInstanceCount();
		}
	}

	Ar.Logf(TEXT("[EnergyWallBatch] Test: nach Freigeben und erneutem Belegen %d Instanzen (vorher %d) - %s."),
		InstanzenNachher, Instanzen,
		InstanzenNachher == Instanzen ? TEXT("Freiliste greift") : TEXT("ACHTUNG: Reihe waechst"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GEnergyWallBatchTestCmd(
	TEXT("RTS.EnergyWall.BatchTest"),
	TEXT("Belegt N Wandsaetze im Batch und meldet Komponenten, Instanzen und Wiederverwendung."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&EnergyWallBatchTest));


// ---------------------------------------------------------------------------------------------
// `RTS.EnergyWall.TestSpawn [Hoechstabstand]`
//
// Verbindet die beiden naechstgelegenen Gebaeude desselben Teams, die eine Wandklasse mitbringen,
// mit einer Energiewand - so wie es sonst der Spieler ueber die Baugeste tut.
//
// Ohne diesen Befehl entsteht eine Wand ausschliesslich aus einer Bediengeste heraus. Damit war
// weder das Batch-Zeichnen noch die Aufzeichnung im Replay in einem Kommandozeilenlauf pruefbar.
// ---------------------------------------------------------------------------------------------

static void EnergyWallTestSpawn(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (!World || !World->IsGameWorld())
	{
		Ar.Log(TEXT("[EnergyWallBatch] TestSpawn braucht eine laufende Spielwelt."));
		return;
	}

	const float MaxAbstand = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 4000.f;

	// Zweites Argument: eine Gebaeudeklasse, die notfalls selbst aufgestellt wird.
	//
	// Wandtuerme baut die KI so gut wie nie, und eine Wand entsteht nur zwischen zweien davon. Ohne
	// diesen Weg liesse sich die Wand in einem Kommandozeilenlauf gar nicht herstellen.
	if (Args.Num() > 1)
	{
		int32 Vorhanden = 0;
		for (TActorIterator<ABuildingBase> Zaehl(World); Zaehl; ++Zaehl)
		{
			if (IsValid(*Zaehl) && Zaehl->EnergyWallClass)
			{
				++Vorhanden;
			}
		}

		if (Vorhanden < 2)
		{
			UClass* Klasse = LoadClass<ABuildingBase>(nullptr, *Args[1]);
			if (!Klasse)
			{
				Ar.Logf(TEXT("[EnergyWallBatch] TestSpawn: Klasse '%s' nicht ladbar."), *Args[1]);
				return;
			}

			FVector Mitte = FVector::ZeroVector;
			if (const APlayerController* PC = World->GetFirstPlayerController())
			{
				FVector Ort;
				FRotator Dreh;
				PC->GetPlayerViewPoint(Ort, Dreh);
				Mitte = Ort + Dreh.Vector() * 2000.f;
				Mitte.Z = Ort.Z;
			}

			// Auf den Boden setzen: die Wand zieht ihre Hoehe aus einer Bodenmessung am Mittelpunkt.
			auto AufDenBoden = [World](FVector Punkt) -> FVector
			{
				FHitResult Treffer;
				const FVector Von = Punkt + FVector(0, 0, 5000.f);
				const FVector Nach = Punkt - FVector(0, 0, 10000.f);
				if (World->LineTraceSingleByChannel(Treffer, Von, Nach, ECC_Visibility))
				{
					Punkt.Z = Treffer.Location.Z;
				}
				return Punkt;
			};

			FActorSpawnParameters Parameter;
			Parameter.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			for (int32 i = 0; i < 2; ++i)
			{
				const FVector Wo = AufDenBoden(Mitte + FVector(0.f, i == 0 ? -750.f : 750.f, 0.f));
				AActor* Neu = World->SpawnActor<AActor>(Klasse, Wo, FRotator::ZeroRotator, Parameter);
				Ar.Logf(TEXT("[EnergyWallBatch] TestSpawn: Turm %d %s bei %.0f/%.0f/%.0f."),
					i + 1, Neu ? TEXT("gestellt") : TEXT("FEHLGESCHLAGEN"), Wo.X, Wo.Y, Wo.Z);
			}
		}
	}

	TArray<ABuildingBase*> Gebaeude;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		ABuildingBase* BB = *It;
		if (IsValid(BB) && BB->EnergyWallClass)
		{
			Gebaeude.Add(BB);
		}
	}

	if (Gebaeude.Num() < 2)
	{
		Ar.Logf(TEXT("[EnergyWallBatch] TestSpawn: nur %d Gebaeude mit Wandklasse gefunden - zu wenig."), Gebaeude.Num());
		return;
	}

	ABuildingBase* A = nullptr;
	ABuildingBase* B = nullptr;
	float Beste = MaxAbstand;

	for (int32 i = 0; i < Gebaeude.Num(); ++i)
	{
		for (int32 j = i + 1; j < Gebaeude.Num(); ++j)
		{
			if (Gebaeude[i]->TeamId != Gebaeude[j]->TeamId
				|| Gebaeude[i]->EnergyWallClass != Gebaeude[j]->EnergyWallClass)
			{
				continue;
			}

			const float D = FVector::Dist2D(Gebaeude[i]->GetActorLocation(), Gebaeude[j]->GetActorLocation());
			if (D < Beste && D > 1.f)
			{
				Beste = D;
				A = Gebaeude[i];
				B = Gebaeude[j];
			}
		}
	}

	if (!A || !B)
	{
		Ar.Logf(TEXT("[EnergyWallBatch] TestSpawn: kein Gebaeudepaar desselben Teams naeher als %.0f uu."), MaxAbstand);
		return;
	}

	B->SpawnEnergyWall(A->EnergyWallClass, A);

	Ar.Logf(TEXT("[EnergyWallBatch] TestSpawn: Wand zwischen '%s' und '%s' (Team %d, Abstand %.0f uu)."),
		*A->GetName(), *B->GetName(), A->TeamId, Beste);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GEnergyWallTestSpawnCmd(
	TEXT("RTS.EnergyWall.TestSpawn"),
	TEXT("Verbindet die zwei naechstgelegenen Gebaeude desselben Teams mit einer Energiewand."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&EnergyWallTestSpawn));
