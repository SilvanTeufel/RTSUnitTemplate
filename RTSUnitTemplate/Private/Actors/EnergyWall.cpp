// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EnergyWall.h"
#include "Mass/EnergyWallBatchSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "Blueprint/PCGClearingBlueprintLibrary.h"
#include "NavAreas/NavArea_EnergyWall.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Characters/Unit/BuildingBase.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/StaticMesh.h"

AEnergyWall::AEnergyWall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	WallRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WallRoot"));
	RootComponent = WallRoot;

	TopRodISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TopRodISM"));
	TopRodISM->SetupAttachment(WallRoot);

	BottomRodISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BottomRodISM"));
	BottomRodISM->SetupAttachment(WallRoot);

	ShieldISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ShieldISM"));
	ShieldISM->SetupAttachment(WallRoot);

	// Disable navigation on visual components to avoid interference
	TopRodISM->SetCanEverAffectNavigation(false);
	BottomRodISM->SetCanEverAffectNavigation(false);
	ShieldISM->SetCanEverAffectNavigation(false);

	// Ein Instanzdatum fuer die Scherung - siehe AEnergyWall::WallSlopePerUnit.
	ShieldISM->NumCustomDataFloats = 1;

	NavObstacleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("NavObstacleBox"));
	NavObstacleBox->SetupAttachment(WallRoot);
	// Collision for physical blocking and projectile interception, but NOT for NavMesh generation
	NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavObstacleBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavObstacleBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	NavObstacleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	NavObstacleBox->SetCanEverAffectNavigation(true);
	NavObstacleBox->OnComponentBeginOverlap.AddDynamic(this, &AEnergyWall::OnOverlapBegin);

	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->SetAreaClass(nullptr); 
	NavModifier->bAutoActivate = false;
	NavModifier->SetActive(false);
	//NavModifier->SetAreaClass(UNavArea_Obstacle::StaticClass());

	// Der Tag, an dem die PCG-Graphen die Bepflanzung aussparen.
	//
	// Nachgesehen in PCG_Landscape_3 (und gleichlautend in den uebrigen unter
	// /Game/RTSUnits/Material/Landscape/PCG): der Knoten GetActorData waehlt
	// actorFilter=AllWorldActors, actorSelection=ByTag, actorSelectionTag="Obstacle" und geht
	// von dort in einen Difference-Knoten - alles mit diesem Tag wird also aus der Streuung
	// herausgeschnitten. Jedes Gebaeude traegt ihn bereits.
	//
	// Hier im Konstruktor und nicht nur im Blueprint, damit er nicht an einer Asset-Aenderung
	// haengt: verschwindet er dort einmal, waechst die Vegetation stumm wieder durch die Wand,
	// und niemand sucht den Grund in einer Tag-Liste. AddUnique, damit ein Blueprint, der ihn
	// ebenfalls setzt, keinen doppelten Eintrag erzeugt.
	Tags.AddUnique(FName(TEXT("Obstacle")));
}

void AEnergyWall::BeginPlay()
{
	Super::BeginPlay();
}

void AEnergyWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsInitialized && CachedBuildingA && CachedBuildingB)
	{
		UpdateVisibility();
		if (bIsVisibleByFoW)
		{
			InitializeWallInternal();
		}
	}

	if (bIsInitialized && (bIsInitializing || bIsDespawning))
	{
		float Alpha = 0.f;
		if (bIsInitializing)
		{
			float Elapsed = GetWorldTimerManager().GetTimerElapsed(InitializationTimerHandle);
			if (Elapsed < 0.f) Elapsed = InitializationDuration;
			float TotalInitTime = FMath::Max(InitializationDuration, 0.001f);
			Alpha = FMath::Clamp(Elapsed / TotalInitTime, 0.f, 1.f);

			if (Alpha <= 0.5f)
			{
				CurrentScaleY = TargetScaleY * (Alpha * 2.f);
			}
			else
			{
				CurrentScaleY = TargetScaleY;
			}
		}
		else // bIsDespawning
		{
			float Elapsed = GetWorldTimerManager().GetTimerElapsed(InitializationTimerHandle);
			float TotalDespawnTime = FMath::Max(DespawnDelay, 0.001f);
			
			// Fallback to LifeSpan if timer is not active (e.g. real destruction)
			if (Elapsed < 0.f && GetLifeSpan() > 0.f)
			{
				float Remaining = GetLifeSpan();
				Alpha = 1.f - FMath::Clamp(Remaining / TotalDespawnTime, 0.f, 1.f);
			}
			else
			{
				Alpha = FMath::Clamp(Elapsed / TotalDespawnTime, 0.f, 1.f);
			}

			if (Alpha >= 0.5f)
			{
				float ScalingAlpha = (Alpha - 0.5f) * 2.f;
				CurrentScaleY = TargetScaleY * (1.f - ScalingAlpha);
			}
			else
			{
				CurrentScaleY = TargetScaleY;
			}
		}

		FTransform TopTransform;
		if (TopRodISM->GetInstanceTransform(0, TopTransform))
		{
			TopTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
		}

		FTransform BottomTransform;
		if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
		{
			BottomTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
		}

		FTransform ShieldTransform;
		if (ShieldISM->GetInstanceTransform(0, ShieldTransform))
		{
			ShieldTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			ShieldISM->UpdateInstanceTransform(0, ShieldTransform, false, true, true);
		}

		// Die eigenen Instanzen oben tragen die Y-Skalierung; der Batch bekommt sie in Weltlage.
		SchreibeBatchTransformationen();

		// Handle Shield Visibility (Flickering or hidden)
		if (!bIsVisibleByFoW)
		{
			SetzeSchildSichtbar(false);
		}
		else if (bIsInitializing)
		{
			if (Alpha > 0.5f)
			{
				if (bFlickerOnInitialize)
				{
					float FlickerAlpha = (Alpha - 0.5f) * 2.f;
					float VisibilityProb = FMath::Lerp(0.1f, 1.0f, FlickerAlpha);
					float Frequency = FMath::Lerp(25.f, 5.f, FlickerAlpha);
					const float SwitchProb = !bSchildZuletztSichtbar ? (Frequency * DeltaTime * VisibilityProb) : (Frequency * DeltaTime * (1.f - VisibilityProb));
					if (FMath::FRand() < SwitchProb)
					{
						SetzeSchildSichtbar(!bSchildZuletztSichtbar);
					}
				}
				else
				{
					SetzeSchildSichtbar(true);
				}
			}
			else
			{
				SetzeSchildSichtbar(false);
			}
		}
		else if (bIsDespawning)
		{
			if (Alpha <= 0.5f)
			{
				if (bFlickerOnDespawn)
				{
					float FlickerAlpha = Alpha * 2.f;
					float VisibilityProb = FMath::Lerp(0.9f, 0.0f, FlickerAlpha);
					float Frequency = FMath::Lerp(25.f, 5.f, FlickerAlpha);
					const float SwitchProb = !bSchildZuletztSichtbar ? (Frequency * DeltaTime * VisibilityProb) : (Frequency * DeltaTime * (1.f - VisibilityProb));
					if (FMath::FRand() < SwitchProb)
					{
						SetzeSchildSichtbar(!bSchildZuletztSichtbar);
					}
				}
				else
				{
					SetzeSchildSichtbar(true);
				}
			}
			else
			{
				SetzeSchildSichtbar(false);
			}
		}
	}
}

void AEnergyWall::Multicast_InitializeWall_Implementation(ABuildingBase* BuildingA, ABuildingBase* BuildingB)
{
	CachedBuildingA = BuildingA;
	CachedBuildingB = BuildingB;

	if (CachedBuildingA && CachedBuildingB)
	{
		InitializeWallInternal();
	}
}

void AEnergyWall::UpdateWallTransformAndDimensions()
{
	if (!CachedBuildingA || !CachedBuildingB) return;

	// SOCKEL, NICHT AKTORMITTE.
	//
	// GetActorLocation() liefert bei diesen Tuermen die KAPSELMITTE. Nimmt man sie als Anschluss,
	// sitzt die ganze Wand um eine halbe Kapselhoehe zu hoch - genau so gemeldet am 22.09.2026,
	// nachdem die Wandhoehe vom Bodentastwert auf die Turmhoehe umgestellt wurde.
	//
	// Die Unterkante ist der richtige Bezug: dort beginnt der Turm sichtbar, und dort soll die
	// Wand ansetzen.
	auto SockelVon = [](ABuildingBase* Gebaeude) -> FVector
	{
		// XY aus Mass, Z aus der KAPSEL DES AKTORS.
		//
		// Gemessen am 22.09.2026 an den 21 WallTowern auf Level_6_Survive: die Aktorhoehen sind
		// exakt zwei Werte (711.1 und 389.2, Halbhoehe je 199, Sockel also 512.1 und 190.2).
		// Tuerme derselben Ebene sind auf 0.1 uu identisch - eine Wand zwischen ihnen MUSS Steigung
		// null haben. Sie wurde trotzdem schraeg gebaut, und der Hoverpunkt sitzt bei denselben
		// Tuermen manchmal unter dem Turm: beide lesen die MASS-Position.
		//
		// ACHTUNG, OFFENE FRAGE: dass die Mass-Position nachhinkt, war daraus nur ERSCHLOSSEN.
		// Die Direktmessung am 22.09.2026 (rts.massz.dump, siehe ActorTransformSyncProcessor)
		// zeigt fuer alle sechs Startgebaeude von Level_6_Survive Abweichung 0.0 - die Drossel-
		// Erklaerung ist damit fuer diese Faelle WIDERLEGT. WallTower waren nicht dabei, weil sie
		// erst im Spiel entstehen; fuer sie fehlt die Messung noch.
		//
		// Die schiefen Waende koennen ebenso gut von der frueheren Bodenspur unter dem Mittelpunkt
		// gekommen sein, die zur selben Zeit entfernt wurde. Der Weg ueber die Kapsel bleibt
		// trotzdem richtig: er ist unabhaengig davon, wann das Fragment zuletzt geschrieben wurde.
		// Deshalb: XY weiter aus Mass, Z aus der Kapsel des Aktors.
		FVector Ort = Gebaeude->GetMassActorLocation();

		// Von der Mitte auf die Unterkante: die Kapsel ist hier das verlaessliche Mass, denn die
		// Aktorbounds schliessen Anbauten und Effekte mit ein und sitzen dadurch zu tief.
		if (const UCapsuleComponent* Kapsel = Gebaeude->FindComponentByClass<UCapsuleComponent>())
		{
			Ort.Z = Kapsel->GetComponentLocation().Z - Kapsel->GetScaledCapsuleHalfHeight();
		}

		return Ort;
	};

	FVector LocA = SockelVon(CachedBuildingA);
	FVector LocB = SockelVon(CachedBuildingB);

	FVector Midpoint = (LocA + LocB) * 0.5f;
	float Distance2D = FVector::Dist2D(LocA, LocB);

	// Perform ground trace at midpoint to position the wall base correctly
	FHitResult Hit;
	FVector Start = Midpoint + FVector(0, 0, 1000.f);
	FVector End = Midpoint - FVector(0, 0, 1000.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(CachedBuildingA);
	Params.AddIgnoredActor(CachedBuildingB);

	float GroundZ = Midpoint.Z;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		GroundZ = Hit.Location.Z;
	}

	// DIE TUERME BESTIMMEN DIE HOEHE, NICHT DER BODEN.
	//
	// Frueher wurde die Wand auf die getastete Bodenhoehe unter ihrem Mittelpunkt gesetzt. Bei
	// geneigter Wand ist das falsch: auf einer Kuppe liegt der Boden in der Mitte HOEHER als die
	// Mittelhoehe der beiden Turmanschluesse, und die Wand rutschte nach oben - gemeldet als
	// "die EnergyWall wurde zu weit oben gebaut".
	//
	// Die Wand darf durch das Gelaende laufen; entscheidend ist, dass Anfang und Ende an den
	// Tuermen sitzen. Deshalb zaehlt bei geneigter Wand der echte Mittelpunkt der Verbindung.
	// Ohne Neigung bleibt es beim Bodentastwert, damit sich an bestehenden Karten nichts aendert.
	// Die Tuerme bestimmen die Hoehe, nicht der Boden unter der Wandmitte - siehe die Meldung
	// "EnergyWall wurde zu weit oben gebaut" vom 22.09.2026 (auf einer Kuppe liegt der Boden in der
	// Mitte hoeher als die Mittelhoehe der beiden Anschluesse).
	SetActorLocation(bFollowTerrainSlope
		? Midpoint
		: FVector(Midpoint.X, Midpoint.Y, GroundZ));
	
	// Rotate wall to align its Y-axis (Length) with the vector between buildings
	//
	// Die Hoehendifferenz wurde hier frueher mit `Direction.Z = 0` weggeworfen - deshalb stand die
	// Wand auch zwischen unterschiedlich hohen Tuermen immer waagerecht. Bleibt sie stehen, neigt
	// sich die Wand und verbindet die Tuerme tatsaechlich. Auf ebenem Grund ist die Differenz null,
	// dort bleibt alles wie bisher.
	FVector Direction = (LocB - LocA);
	if (!bFollowTerrainSlope)
	{
		Direction.Z = 0;
	}
	// ROTATOREN NICHT ADDIEREN, sobald ein Nickwinkel im Spiel ist.
	//
	// `Direction.Rotation() + FRotator(0, -90, 0)` ging gut, solange Direction.Z auf null gezwungen
	// war: dann trug die Richtung nur einen Gierwinkel, und die Addition entsprach der Drehung.
	// Mit Nickwinkel ist die Addition KEINE Verkettung mehr - die -90 Grad wirken dann um die
	// falsche Achse und rollen die Wand, statt sie zu neigen. Von aussen sah sie deshalb weiter
	// waagerecht aus, obwohl die Hoehendifferenz laengst berechnet wurde.
	//
	// Richtig ist die Verkettung ueber Quaternionen: erst in die Richtung drehen, DANN lokal um
	// -90 Grad gieren, damit die Laengsachse (Y) auf der Verbindungslinie liegt.
	// NUR GIERWINKEL - die Neigung macht das MATERIAL, nicht der Aktor.
	//
	// Eine gekippte Instanz bleibt ein gekipptes RECHTECK: ihre Seitenkanten stehen dann schief zu
	// den senkrechten Tuermen. Gewuenscht ist ein Trapez - Seitenkanten senkrecht, Ober- und
	// Unterkante schraeg. Das ist eine SCHERUNG, und die kann eine Instanztransformation nicht
	// (sie kennt nur Verschiebung, Drehung, Skalierung).
	//
	// Deshalb: Aktor waagerecht ausrichten wie frueher, und die Hoehendifferenz als
	// Instanzdatum ans Schildmaterial geben, das die Vertices entlang der Laenge in Z verschiebt.
	FVector FlacheRichtung = Direction;
	FlacheRichtung.Z = 0.f;
	SetActorRotation(FlacheRichtung.Rotation() + FRotator(0, -90.f, 0.f));

	// Steigung je Laengeneinheit, entlang der lokalen Y-Achse (die Laengsachse der Wand).
	// Vorzeichen folgt der Reihenfolge A -> B, also derselben Richtung wie die Ausrichtung oben.
	const float LaengeXY = FMath::Max(Distance2D, 1.f);
	WallSlopePerUnit = (LocB.Z - LocA.Z) / LaengeXY;

	// Waagerechte Richtung A -> B, normiert. Das Material braucht sie, um den Abstand eines Vertex
	// vom Instanzmittelpunkt ENTLANG der Wand zu bestimmen - siehe CustomDataRichtungX/Y.
	FVector2D Richtung(LocB.X - LocA.X, LocB.Y - LocA.Y);
	WallDirectionXY = Richtung.IsNearlyZero() ? FVector2D(1.f, 0.f) : Richtung.GetSafeNormal();

	// Der Batch muss den neuen Wert sehen - die Wand kann sich neu ausrichten, wenn ein Turm
	// ersetzt wird oder die Karte nachtraeglich geformt wird.
	SendeSteigungAnBatch();

	float NativeTopZ = TopRodISM->GetRelativeLocation().Z;
	float NativeBottomZ = BottomRodISM->GetRelativeLocation().Z;
	float NativeHeight = FMath::Max(FMath::Abs(NativeTopZ - NativeBottomZ), 1.0f);
	float NativeLength = 100.0f;
	if (ShieldISM)
	{
		if (UStaticMesh* Mesh = ShieldISM->GetStaticMesh())
		{
			NativeLength = FMath::Max(Mesh->GetBounds().BoxExtent.Y * 2.0f * ShieldISM->GetRelativeScale3D().Y, 1.0f);
		}
	}

	// Bei geneigter Wand ist der 2D-Abstand zu KURZ: sie muss die Strecke schraeg ueberbruecken,
	// sonst endet sie vor dem hoeher stehenden Turm. Waagerecht sind beide Werte gleich.
	// Die Laenge liegt jetzt WAAGERECHT (die Hoehe macht die Scherung im Material), deshalb zaehlt
	// wieder der 2D-Abstand. Mit dem 3D-Abstand waere die Wand zu lang und stuende ueber den Turm
	// hinaus - der 3D-Abstand war nur richtig, solange die Instanz selbst gekippt wurde.
	const float Spannweite = Distance2D;

	TargetScaleY = Spannweite / NativeLength;
	TargetDistance2D = Distance2D;
	TargetWallHeight = NativeHeight;
}

void AEnergyWall::InitializeWallInternal()
{
	if (bIsInitialized || !CachedBuildingA || !CachedBuildingB) return;

	if (NavModifier) NavModifier->SetActive(false);

	UpdateWallTransformAndDimensions();

	// Setup instances for the rods and the shield
	TopRodISM->ClearInstances();
	BottomRodISM->ClearInstances();
	ShieldISM->ClearInstances();

	bIsInitialized = true;

	// Beim gemeinsamen Batch anmelden. Die eigenen Instanzen darunter bleiben bestehen: sie sind
	// die Vorlage fuer Mesh, Material und die relative Lage der drei Teile.
	MeldeBeimBatchAn();

	// Add instance for top rod with 0 scale at its Blueprint position
	TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));

	// Add instance for bottom rod with 0 scale at its Blueprint position
	BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));

	// Add instance for shield plane at its Blueprint position, and hide it
	ShieldISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, TargetScaleY, 1.f)));

	// HINWEIS: die eigenen ISMs sind unsichtbar geschaltet, gezeichnet wird aus dem Batch. Ein
	// Instanzdatum HIER hat keine sichtbare Wirkung - die Steigung geht ueber SendeSteigungAnBatch().
	SetzeSchildSichtbar(false);

	// A wall that replicated in already-deactivated (e.g. to a late-joining client) must stay
	// collapsed + hidden and NOT play the spawn/inflate animation. The instances above still exist
	// so a later Multicast_ActivateWall can inflate them normally.
	if (bIsDeactivated)
	{
		bIsInitializing = false;
		CurrentScaleY = 0.f;
		UpdateVisibility();
		return;
	}

	bIsInitializing = true;
	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnInitializationTimerComplete, InitializationDuration, false);

	UpdateVisibility();
}

void AEnergyWall::UpdateVisibility()
{
	bIsVisibleByFoW = false;

	if (CachedBuildingA && (CachedBuildingA->IsMyTeam || CachedBuildingA->IsVisibleEnemy))
	{
		bIsVisibleByFoW = true;
	}
	else if (CachedBuildingB && (CachedBuildingB->IsMyTeam || CachedBuildingB->IsVisibleEnemy))
	{
		bIsVisibleByFoW = true;
	}

	// Sichtbarkeit geht ueber die Instanzskalierung, nicht mehr ueber SetHiddenInGame.
	//
	// Im gemeinsamen Batch-ISM gibt es keine Sichtbarkeit JE INSTANZ - SetHiddenInGame wuerde ALLE
	// Waende ausblenden. Unsichtbar heisst hier Skalierung null; siehe SchreibeBatchTransformationen.
	if (!bIsInitializing && !bIsDespawning)
	{
		// Ausserhalb von Auf- und Abbau bestimmt der Nebel das Schild. Waehrend beider Phasen
		// steuert der Tick das Flackern und darf hier nicht ueberschrieben werden.
		bSchildZuletztSichtbar = bIsVisibleByFoW && !bIsDeactivated;
	}

	SchreibeBatchTransformationen();
}

void AEnergyWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnergyWall, CachedBuildingA);
	DOREPLIFETIME(AEnergyWall, CachedBuildingB);
	DOREPLIFETIME(AEnergyWall, bIsDeactivated);
	DOREPLIFETIME(AEnergyWall, TeamId);
}

void AEnergyWall::OnInitializationTimerComplete()
{
	bIsInitializing = false;
	CurrentScaleY = TargetScaleY;

	// Final scale update
	FTransform TopTransform;
	if (TopRodISM->GetInstanceTransform(0, TopTransform))
	{
		TopTransform.SetScale3D(FVector(1.f, TargetScaleY, 1.f));
		TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
	}

	FTransform BottomTransform;
	if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
	{
		BottomTransform.SetScale3D(FVector(1.f, TargetScaleY, 1.f));
		BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
	}

	// Update visibility based on FoW
	UpdateVisibility();

	RegisterObstacle(TargetDistance2D, TargetWallHeight);
}

void AEnergyWall::InitializeISMs()
{
	if (!NavObstacleBox) return;

	// Align Box Y (Length) with Actor Y (Wall Direction)
	NavObstacleBox->SetRelativeRotation(FRotator(0, 0.f, 0));

	float NativeTopZ = TopRodISM->GetRelativeLocation().Z;
	float NativeBottomZ = BottomRodISM->GetRelativeLocation().Z;
	float NativeHeight = FMath::Max(FMath::Abs(NativeTopZ - NativeBottomZ), 1.0f);
	float NativeLength = 100.0f;
	if (ShieldISM)
	{
		if (UStaticMesh* Mesh = ShieldISM->GetStaticMesh())
		{
			NativeLength = FMath::Max(Mesh->GetBounds().BoxExtent.Y * 2.0f * ShieldISM->GetRelativeScale3D().Y, 1.0f);
		}
	}

	float WallHeight = NativeHeight;

	// Update NavObstacleBox position to be centered between rods
	float MidZ = (NativeTopZ + NativeBottomZ) * 0.5f;
	NavObstacleBox->SetRelativeLocation(FVector(0, 0, MidZ));
	float PreviewScaleY = 500.f / NativeLength; // 500 units long

	// Top Rod
	if (TopRodISM)
	{
		TopRodISM->ClearInstances();
		TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
	}

	// Bottom Rod
	if (BottomRodISM)
	{
		BottomRodISM->ClearInstances();
		BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
	}

	// Shield
	if (ShieldISM)
	{
		ShieldISM->ClearInstances();
		ShieldISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
	}
}

void AEnergyWall::InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent)
{
	if (!InISMComponent || !InISMComponent->GetStaticMesh())
	{
		return;
	}

	const FTransform LocalIdentityTransform = FTransform::Identity;
	if (InISMComponent->GetInstanceCount() == 0)
	{
		InISMComponent->AddInstance(LocalIdentityTransform, false);
	}
	else
	{
		InISMComponent->UpdateInstanceTransform(0, LocalIdentityTransform, false, true, false);
	}
}

void AEnergyWall::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
	if (Unit)
	{
		UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
		if (ASC)
		{
			TSubclassOf<UGameplayEffect> EffectToApply = (Unit->TeamId == this->TeamId) ? FriendlyEffectClass : EnemyEffectClass;
			if (EffectToApply)
			{
				FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
				EffectContext.AddInstigator(this, this);

				FGameplayEffectSpecHandle NewHandle = ASC->MakeOutgoingSpec(EffectToApply, 1.0f, EffectContext);
				if (NewHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*NewHandle.Data.Get());
				}
			}
		}
	}
}

void AEnergyWall::RegisterObstacle(float Length, float Height)
{
	FVector StartPoint = CachedBuildingA ? CachedBuildingA->GetActorLocation() : FVector::ZeroVector;
	FVector EndPoint = CachedBuildingB ? CachedBuildingB->GetActorLocation() : FVector::ZeroVector;

	// Calculate a diagonal boost for the length padding to ensure no gaps at building connections
	FRotator Rotation = GetActorRotation();
	float AngleRad = FMath::DegreesToRadians(Rotation.Yaw);
	// DiagonalScale is 1.0 for axis-aligned and ~1.414 for 45-degree diagonal
	float DiagonalScale = FMath::Abs(FMath::Sin(AngleRad)) + FMath::Abs(FMath::Cos(AngleRad));
	// Map DiagonalScale [1.0, 1.414...] to [0, 1] for smoother control
	float Alpha = FMath::Clamp((DiagonalScale - 1.0f) / 0.41421356f, 0.0f, 1.0f);

	if (NavObstacleBox)
	{
		// Toggle collision instead of CanEverAffectNavigation for more reliable updates
		NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NavObstacleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		FVector BoxExtent;
		
		// Thickness adjusted based on angle - use more generous thickness for diagonals
		BoxExtent.X = FMath::Lerp(MinThickness, MaxThickness, Alpha) * 0.5f;
		
		// Set half-extent for Height (Z) with extra padding to ensure it reaches the ground on uneven terrain
		BoxExtent.Z = (Height * 0.5f) + NavigationZPadding;
		
		// Set half-extent for the distance (Length / 2) with padding
		float CurrentPadding = FMath::Lerp(MinPadding, MaxPadding, Alpha);
		BoxExtent.Y = (Length * 0.5f) + CurrentPadding + AgentRadiusPadding;
		
		// Set box properties and ensure navigation is triggered
		NavObstacleBox->SetBoxExtent(BoxExtent, true);
		NavObstacleBox->UpdateBounds();
		NavObstacleBox->UpdateComponentToWorld();
		
		// Position and rotate relative to actor - Actor is already rotated between buildings
		float MidZ = (TopRodISM->GetRelativeLocation().Z + BottomRodISM->GetRelativeLocation().Z) * 0.5f;
		NavObstacleBox->SetRelativeLocation(FVector(0, 0, MidZ));
		NavObstacleBox->SetRelativeRotation(FRotator::ZeroRotator);
		NavObstacleBox->UpdateComponentToWorld();

		if (NavModifier)
		{
			NavModifier->SetAreaClass(UNavArea_EnergyWall::StaticClass());
			NavModifier->FailsafeExtent = BoxExtent;
			NavModifier->SetActive(true);
		}
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (NavObstacleBox)
		{
			// Expand the dirty area to ensure neighboring NavMesh tiles are properly updated
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			
			// Mark area as dirty
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);

			// Update Octree
			NavSys->UpdateNavOctreeBounds(this);
			NavSys->UpdateActorInNavOctree(*this);

			// Refresh modifiers after the octree update
			if (NavModifier)
			{
				NavModifier->RefreshNavigationModifiers();
			}
		}
	}

	// Erst HIER, nicht frueher: vorher steht die Box noch nicht in ihrer endgueltigen Groesse,
	// und ein Freiraeumen nach einer halben Laenge liesse die zweite Haelfte bewachsen.
	RaeumePCGEntlangDerWand();
}

int32 AEnergyWall::RaeumePCGEntlangDerWand()
{
	if (!bClearPCGAlongWall || !NavObstacleBox)
	{
		return 0;
	}

	// Ort und Drehung der Box, aber OHNE ihre Skalierung.
	//
	// Das ist kein Schoenheitsfehler, sondern der Unterschied zwischen richtig und falsch:
	// TransformPosition mit der Inversen einer SKALIERTEN Transformation rechnet die Skalierung
	// heraus - die lokalen Koordinaten kaemen dann unskaliert heraus und wuerden gegen ein
	// skaliertes Halbmass geprueft. Bei Skalierung 1 faellt das nicht auf, bei jeder anderen
	// raeumt die Wand einen um genau diesen Faktor falschen Streifen frei. Und das Polster
	// waere ebenfalls nicht mehr in Zentimetern.
	//
	// Also: unskalierte Transformation plus skaliertes Halbmass. Beides dann in Weltmassstab.
	const FTransform BoxOrt(NavObstacleBox->GetComponentQuat(),
	                        NavObstacleBox->GetComponentLocation());
	const FVector Ausdehnung = NavObstacleBox->GetScaledBoxExtent();

	// Nicht repliziert und auf jeder Maschine ausgefuehrt - die Bepflanzung ist rein sichtbar,
	// jede Seite traegt ihre eigenen Instanzen. Genauso macht es der Radius-Weg der Gebaeude.
	const int32 Entfernt = UPCGClearingBlueprintLibrary::ClearPCGInstancesInBox(
		this,
		BoxOrt,
		Ausdehnung,
		PCGClearPadding,
		/*bIncludeVertical=*/false);

	// Verbose und nicht Warning: im Normalbetrieb unsichtbar, aber auf Wunsch nachpruefbar mit
	// -LogCmds="LogTemp Verbose". Ohne eine solche Zeile laesst sich "es wurde nichts geraeumt"
	// nicht von "es stand dort nichts" unterscheiden.
	UE_LOG(LogTemp, Verbose,
		TEXT("[PCGWand] %s: %d Instanzen entfernt, Box %.0f x %.0f (Halbmass) + %.0f Rand."),
		*GetName(), Entfernt, Ausdehnung.X, Ausdehnung.Y, PCGClearPadding);
	return Entfernt;
}

void AEnergyWall::DeactivateNavigation()
{
	if (NavObstacleBox)
	{
		// Use collision toggling for dynamic navigation state changes
		NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (NavModifier)
	{
		NavModifier->SetAreaClass(nullptr);
		NavModifier->SetActive(false);
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		// Force refresh of the octree and then modifiers
		NavSys->UpdateNavOctreeBounds(this);
		NavSys->UpdateActorInNavOctree(*this);

		if (NavModifier)
		{
			NavModifier->RefreshNavigationModifiers();
		}

		if (NavObstacleBox)
		{
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);
		}
	}
}

void AEnergyWall::ApplyDespawnEffects()
{
	// Aufloesezeit geht in Custom Data 0, nicht mehr auf ein dynamisches Material JE WAND.
	//
	// Im gemeinsamen Batch-ISM teilen sich alle Waende EIN Material - ein dynamisches Material zu
	// erzeugen haette allen dieselbe Aufloesezeit gegeben, und die zuerst gebaute Wand haette die
	// Animation aller uebrigen ausgeloest. Das Material liest den Wert aus PerInstanceCustomData
	// (Platz 0) statt aus dem Skalarparameter DespawnStartTimeParameterName.
	if (UEnergyWallBatchSubsystem* Batch = UEnergyWallBatchSubsystem::Get(this))
	{
		const float Jetzt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Batch->SetzeAufloesebeginn(EEnergyWallPart::TopRod,    BatchIndexTop,    Jetzt);
		Batch->SetzeAufloesebeginn(EEnergyWallPart::BottomRod, BatchIndexBottom, Jetzt);
		Batch->SetzeAufloesebeginn(EEnergyWallPart::Shield,    BatchIndexShield, Jetzt);
	}
}

void AEnergyWall::ActivateNavigation()
{
	RegisterObstacle(TargetDistance2D, TargetWallHeight);
}

void AEnergyWall::Multicast_DeactivateWall_Implementation()
{
	if (bIsDeactivated || bIsDespawning) return;

	bIsDespawning = true;
	bIsInitializing = false;
	bIsDeactivated = true;

	ApplyDespawnEffects();
	DeactivateNavigation();

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnDeactivationTimerComplete, DespawnDelay, false);
}

void AEnergyWall::Multicast_ActivateWall_Implementation()
{
	if (!bIsDeactivated && !bIsDespawning) return;

	UpdateWallTransformAndDimensions();
	
	bIsInitializing = true;
	bIsDespawning = false;
	bIsDeactivated = false;

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnInitializationTimerComplete, InitializationDuration, false);

	UpdateVisibility();
}

void AEnergyWall::OnDeactivationTimerComplete()
{
	bIsDespawning = false;
	CurrentScaleY = 0.f;

	// Final scale update to ensure it's hidden
	FTransform TopTransform;
	if (TopRodISM->GetInstanceTransform(0, TopTransform))
	{
		TopTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
		TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
	}

	FTransform BottomTransform;
	if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
	{
		BottomTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
		BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
	}

	UpdateVisibility();
}

void AEnergyWall::StartDespawn(AActor* DestroyedActor)
{
	// A real (active-wall) despawn is already running with its lifespan counting down: don't restart it.
	if (bIsDespawning && GetLifeSpan() > 0.0f)
	{
		return;
	}

	// Stop listening to the (now dying) buildings on every path.
	if (CachedBuildingA) CachedBuildingA->OnDestroyed.RemoveAll(this);
	if (CachedBuildingB) CachedBuildingB->OnDestroyed.RemoveAll(this);

	bIsInitializing = false;
	GetWorldTimerManager().ClearTimer(InitializationTimerHandle);

	// If the wall was already deactivated (rods collapsed to 0, shield hidden), tear it down
	// SILENTLY. We must NOT set bIsDespawning here: the Tick despawn branch would otherwise reset
	// CurrentScaleY = TargetScaleY (re-inflating the rods from 0 to full) and flicker the shield
	// back on -> the reported "deactivated wall reappears / replays a despawn animation on building
	// death" bug (and on clients, with no lifespan/timer, it would be stuck at full scale until the
	// replicated actor destroy arrives).
	if (bIsDeactivated)
	{
		bIsDespawning = false;
		CurrentScaleY = 0.f;

		FTransform InstTransform;
		if (TopRodISM && TopRodISM->GetInstanceTransform(0, InstTransform))
		{
			InstTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
			TopRodISM->UpdateInstanceTransform(0, InstTransform, false, true, true);
		}
		if (BottomRodISM && BottomRodISM->GetInstanceTransform(0, InstTransform))
		{
			InstTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
			BottomRodISM->UpdateInstanceTransform(0, InstTransform, false, true, true);
		}
		if (ShieldISM) SetzeSchildSichtbar(false);

		DeactivateNavigation(); // idempotent; nav is already down for a deactivated wall
		if (HasAuthority())
		{
			SetLifeSpan(DespawnDelay);
		}
		return;
	}

	// Normal active-wall despawn: unchanged shrink/dissolve animation.
	bIsDespawning = true;
	ApplyDespawnEffects();
	DeactivateNavigation();

	if (HasAuthority())
	{
		SetLifeSpan(DespawnDelay);
	}
}


// ============================================================================================
// Batch-Zeichnen (09.09.2026)
//
// Vorher hielt jede Wand drei eigene ISM-Komponenten mit je EINER Instanz - bei N Waenden also
// 3N Zeichenaufrufe. Die Komponenten bleiben als Mesh- und Materialquelle bestehen (das Blueprint
// setzt sie), werden aber ausgeblendet; gezeichnet wird ueber drei gemeinsame ISMs im
// UEnergyWallBatchSubsystem.
//
// Die Instanztransformationen waren rein LOKAL (Position und Drehung null, nur die Y-Skalierung
// aenderte sich) - die Lage kam von der Aktortransformation. Im gemeinsamen ISM muss beides
// verrechnet werden, deshalb Komponententransformation * Skalierung.
// ============================================================================================

void AEnergyWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MeldeVomBatchAb();
	Super::EndPlay(EndPlayReason);
}

void AEnergyWall::SendeSteigungAnBatch()
{
	// Alle DREI Teile brauchen denselben Wert: Ober- und Unterstange und das Schild werden
	// gemeinsam geschert, sonst laufen sie auseinander.
	UEnergyWallBatchSubsystem* Batch = UEnergyWallBatchSubsystem::Get(this);
	if (!Batch)
	{
		return;
	}

	Batch->SetzeSteigung(EEnergyWallPart::TopRod,    BatchIndexTop,    WallSlopePerUnit, WallDirectionXY);
	Batch->SetzeSteigung(EEnergyWallPart::BottomRod, BatchIndexBottom, WallSlopePerUnit, WallDirectionXY);
	Batch->SetzeSteigung(EEnergyWallPart::Shield,    BatchIndexShield, WallSlopePerUnit, WallDirectionXY);
}

void AEnergyWall::MeldeBeimBatchAn()
{
	UEnergyWallBatchSubsystem* Batch = UEnergyWallBatchSubsystem::Get(this);
	if (!Batch || BatchIndexTop != INDEX_NONE)
	{
		return;
	}

	// Eigene Komponenten nicht mehr zeichnen - sie bleiben nur als Vorlage stehen.
	if (TopRodISM)    { TopRodISM->SetVisibility(false, true); }
	if (BottomRodISM) { BottomRodISM->SetVisibility(false, true); }
	if (ShieldISM)    { ShieldISM->SetVisibility(false, true); }

	BatchIndexTop    = Batch->BelegePlatz(EEnergyWallPart::TopRod,    TopRodISM,    FTransform::Identity);
	BatchIndexBottom = Batch->BelegePlatz(EEnergyWallPart::BottomRod, BottomRodISM, FTransform::Identity);
	BatchIndexShield = Batch->BelegePlatz(EEnergyWallPart::Shield,    ShieldISM,    FTransform::Identity);

	// Direkt nach dem Belegen die Steigung nachreichen - der Platz ist frisch und steht auf 0.
	SendeSteigungAnBatch();

	SchreibeBatchTransformationen();
}

void AEnergyWall::SetzeSchildSichtbar(bool bSichtbar)
{
	if (bSchildZuletztSichtbar == bSichtbar)
	{
		return;
	}

	bSchildZuletztSichtbar = bSichtbar;
	SchreibeBatchTransformationen();
}

void AEnergyWall::MeldeVomBatchAb()
{
	UEnergyWallBatchSubsystem* Batch = UEnergyWallBatchSubsystem::Get(this);
	if (!Batch)
	{
		return;
	}

	Batch->GibPlatzFrei(EEnergyWallPart::TopRod,    BatchIndexTop);
	Batch->GibPlatzFrei(EEnergyWallPart::BottomRod, BatchIndexBottom);
	Batch->GibPlatzFrei(EEnergyWallPart::Shield,    BatchIndexShield);

	BatchIndexTop = INDEX_NONE;
	BatchIndexBottom = INDEX_NONE;
	BatchIndexShield = INDEX_NONE;
}

void AEnergyWall::SchreibeBatchTransformationen()
{
	UEnergyWallBatchSubsystem* Batch = UEnergyWallBatchSubsystem::Get(this);
	if (!Batch || BatchIndexTop == INDEX_NONE)
	{
		return;
	}

	// Die Y-Skalierung ist der Auf- und Abbau der Wand; Position und Drehung kommen aus der
	// Komponente, die am Wandaktor haengt.
	// Unsichtbar = Skalierung null.
	//
	// Bewusst NICHT ueber Custom Data: gemessen am 09.09.2026 hat KEINES der beiden Wandmaterialien
	// (M_Shield_AH_Inst, MI_Emissive_03) einen passenden Parameter - das Material haette also erst
	// umgebaut werden muessen. Ueber die Skalierung geht es ohne jeden Materialeingriff, und es
	// passt zum vorhandenen Aussehen: die Wand faehrt beim Auf- und Abbau ohnehin ueber die
	// Y-Skalierung hoch und runter.
	auto Weltlage = [this](const UInstancedStaticMeshComponent* Komponente, bool bSichtbar) -> FTransform
	{
		FTransform T = Komponente ? Komponente->GetComponentTransform() : GetActorTransform();
		if (!bSichtbar)
		{
			T.SetScale3D(FVector::ZeroVector);
			return T;
		}

		const FVector S = T.GetScale3D();
		T.SetScale3D(FVector(S.X, S.Y * CurrentScaleY, S.Z));
		return T;
	};

	// DIE STANGEN WERDEN GENEIGT, NICHT GESCHERT.
	//
	// Eine Stange ist ein Zylinder, also ein eindimensionales Teil - sie braucht keine Scherung,
	// sie muss nur schraeg stehen, dann bildet sie genau die Ober- bzw. Unterkante des Schildes.
	// Das spart den Eingriff in MI_Emissive_03, das sich die Stangen mit anderen Objekten im
	// Projekt TEILEN; dort eine Scherung einzubauen haette auf alles gewirkt, was es sonst noch
	// benutzt. Nur das Schild (eine Flaeche) wird im Material geschert.
	auto MitNeigung = [this](FTransform T) -> FTransform
	{
		if (FMath::IsNearlyZero(WallSlopePerUnit))
		{
			return T;
		}

		// Drehung um die LOKALE X-Achse (Roll): die Laengsachse der Stange ist Y, und eine Drehung
		// um X hebt das eine Ende und senkt das andere.
		// VORZEICHEN: negativ. Gemessen am Bild vom 22.09.2026 - mit positivem Roll liefen die
		// Stangen gegenlaeufig zum Schild und kreuzten es. Die Drehrichtung um die lokale X-Achse
		// ist der Steigung entgegengesetzt.
		const float WinkelGrad = -FMath::RadiansToDegrees(FMath::Atan(WallSlopePerUnit));
		T.SetRotation(T.GetRotation() * FQuat(FRotator(0.f, 0.f, WinkelGrad)));

		// Schraeg ist laenger als waagerecht: ohne diese Streckung endet die Stange vor dem
		// hoeheren Turm. Der Faktor ist die Hypotenuse zu 1 und Steigung.
		const FVector S = T.GetScale3D();
		T.SetScale3D(FVector(S.X, S.Y * FMath::Sqrt(1.f + WallSlopePerUnit * WallSlopePerUnit), S.Z));
		return T;
	};

	Batch->SetzeTransform(EEnergyWallPart::TopRod,    BatchIndexTop,    MitNeigung(Weltlage(TopRodISM,    bIsVisibleByFoW)));
	Batch->SetzeTransform(EEnergyWallPart::BottomRod, BatchIndexBottom, MitNeigung(Weltlage(BottomRodISM, bIsVisibleByFoW)));
	Batch->SetzeTransform(EEnergyWallPart::Shield,    BatchIndexShield, Weltlage(ShieldISM,    bIsVisibleByFoW && bSchildZuletztSichtbar));
}
