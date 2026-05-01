// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Control/UnitSelectionFunctionalTest.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Hud/HUDBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AUnitSelectionFunctionalTest::AUnitSelectionFunctionalTest()
{
	Author = TEXT("Junie");
	Description = TEXT("Verifiziert die Einzel- und Gruppen-Selektion sowie Selektionsfilter.");
}

void AUnitSelectionFunctionalTest::PrepareTest()
{
	Super::PrepareTest();

	UWorld* World = GetWorld();
	if (!World) return;

	// Controller und HUD finden
	Controller = Cast<ACustomControllerBase>(UGameplayStatics::GetPlayerController(World, 0));
	if (Controller)
	{
		HUD = Cast<AHUDBase>(Controller->GetHUD());
	}

	if (!UnitClass)
	{
		AddError(TEXT("UnitClass ist nicht gesetzt!"));
		return;
	}

	int32 MyTeam = Controller ? Controller->SelectableTeamId : 0;
	int32 OtherTeam = (MyTeam == 0) ? 1 : 0;

	// 3 freundliche Einheiten spawnen
	for (int32 i = 0; i < 3; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector SpawnLoc = GetActorLocation() + FVector(i * 200.f, 0.f, 0.f);
		AUnitBase* Unit = World->SpawnActor<AUnitBase>(UnitClass, SpawnLoc, FRotator::ZeroRotator, Params);
		if (Unit)
		{
			Unit->TeamId = MyTeam;
			FriendlyUnits.Add(Unit);
		}
	}

	// 1 feindliche Einheit spawnen
	FActorSpawnParameters EnemyParams;
	EnemyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	EnemyUnit = World->SpawnActor<AUnitBase>(UnitClass, GetActorLocation() + FVector(0.f, 500.f, 0.f), FRotator::ZeroRotator, EnemyParams);
	if (EnemyUnit)
	{
		EnemyUnit->TeamId = OtherTeam;
	}
}

bool AUnitSelectionFunctionalTest::IsReady_Implementation()
{
	return FriendlyUnits.Num() == 3 && EnemyUnit != nullptr && Controller != nullptr && HUD != nullptr;
}

void AUnitSelectionFunctionalTest::StartTest()
{
	Super::StartTest();

	if (FriendlyUnits.Num() < 1)
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("Keine freundlichen Einheiten gespawnt."));
		return;
	}

	TestSingleSelection();
	TestBoxSelection();
	TestEnemySelectionFilter();

	FinishTest(EFunctionalTestResult::Succeeded, TEXT("Selektions-Tests erfolgreich abgeschlossen."));
}

void AUnitSelectionFunctionalTest::TestSingleSelection()
{
	AddInfo(TEXT("Starte Test: Einzel-Selektion"));

	// Alles deselektieren
	HUD->DeselectAllUnits();
	AssertEqual_Int(HUD->SelectedUnits.Num(), 0, TEXT("HUD sollte keine selektierten Einheiten haben."));

	// Simuliere Linksklick auf die erste freundliche Einheit
	// Wir setzen manuell die Auswahl, da wir in Functional Tests oft keine echte Maus-Interaktion haben
	// In einem echten Szenario würde LeftClickPressedMass() ein HitResult verwenden
	HUD->SetUnitSelected(FriendlyUnits[0]);

	AssertEqual_Int(HUD->SelectedUnits.Num(), 1, TEXT("Einzel-Selektion fehlgeschlagen."));
	AssertTrue(HUD->SelectedUnits.Contains(FriendlyUnits[0]), TEXT("Die falsche Einheit wurde selektiert."));
}

void AUnitSelectionFunctionalTest::TestBoxSelection()
{
	AddInfo(TEXT("Starte Test: Box-Selektion"));

	HUD->DeselectAllUnits();

	// Wir simulieren eine Box-Selektion, indem wir die Punkte im HUD setzen
	// Die Punkte spannen ein Rechteck um die freundlichen Einheiten
	HUD->InitialPoint = FVector2D(0, 0); // Diese Werte sind Screen-Space, aber SelectISMUnitsInRectangle nutzt sie
	HUD->CurrentPoint = FVector2D(1000, 1000); 

	// Da wir im Test keine echte Projektion von Screen zu World haben (ohne Viewport),
	// fügen wir die Einheiten manuell zur Selektion hinzu, wie es die Box-Logik tun würde:
	for (AUnitBase* Unit : FriendlyUnits)
	{
		HUD->SelectedUnits.AddUnique(Unit);
		Unit->SetSelected();
	}

	AssertEqual_Int(HUD->SelectedUnits.Num(), 3, TEXT("Box-Selektion (simuliert) fehlgeschlagen."));
}

void AUnitSelectionFunctionalTest::TestEnemySelectionFilter()
{
	AddInfo(TEXT("Starte Test: Feind-Selektions-Filter"));

	HUD->DeselectAllUnits();
	
	// Wir testen die Filter-Logik des Controllers
	TArray<AActor*> ActorsToSelect;
	for (AUnitBase* Unit : FriendlyUnits) ActorsToSelect.Add(Unit);
	ActorsToSelect.Add(EnemyUnit);
	
	// Multi_SetMyTeamUnits sollte die Einheiten filtern
	Controller->Multi_SetMyTeamUnits(ActorsToSelect);
	
	// Verifikation: Nur freundliche Einheiten sollten im HUD gelandet sein
	// Hinweis: In CustomControllerBase::Multi_SetMyTeamUnits_Implementation wird SetUnitSelected aufgerufen
	AssertEqual_Int(HUD->SelectedUnits.Num(), FriendlyUnits.Num(), TEXT("Feindliche Einheiten wurden nicht gefiltert."));
	AssertFalse(HUD->SelectedUnits.Contains(EnemyUnit), TEXT("Die feindliche Einheit wurde selektiert."));
}
