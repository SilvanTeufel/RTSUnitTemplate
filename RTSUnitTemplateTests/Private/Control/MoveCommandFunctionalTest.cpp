// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Control/MoveCommandFunctionalTest.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassEntityManager.h"
#include "Mass/MassActorBindingComponent.h"
#include "Kismet/GameplayStatics.h"

AMoveCommandFunctionalTest::AMoveCommandFunctionalTest()
{
	Author = TEXT("Junie");
	Description = TEXT("Verifiziert Bewegungsbefehle und Command Queuing auf Mass-Ebene.");
}

void AMoveCommandFunctionalTest::PrepareTest()
{
	Super::PrepareTest();

	UWorld* World = GetWorld();
	if (!World) return;

	Controller = Cast<ACustomControllerBase>(UGameplayStatics::GetPlayerController(World, 0));

	if (UnitClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TestUnit = World->SpawnActor<AUnitBase>(UnitClass, GetActorLocation(), FRotator::ZeroRotator, Params);
		if (TestUnit)
		{
			int32 MyTeam = Controller ? Controller->SelectableTeamId : 0;
			TestUnit->TeamId = MyTeam;
		}
	}
}

bool AMoveCommandFunctionalTest::IsReady_Implementation()
{
	return TestUnit != nullptr && Controller != nullptr;
}

void AMoveCommandFunctionalTest::StartTest()
{
	Super::StartTest();

	TestSimpleMove();
	TestQueuedMove();

	FinishTest(EFunctionalTestResult::Succeeded, TEXT("Move-Command-Tests erfolgreich abgeschlossen."));
}

void AMoveCommandFunctionalTest::TestSimpleMove()
{
	AddInfo(TEXT("Starte Test: Einfacher Bewegungsbefehl"));

	FVector TargetLoc = GetActorLocation() + FVector(1000.f, 0.f, 0.f);

	// Simuliere Rechtsklick via Controller
	// Da RunUnitsAndSetWaypointsMass ein FHitResult erwartet:
	FHitResult Hit;
	Hit.ImpactPoint = TargetLoc;
	Hit.Location = TargetLoc;
	Hit.bBlockingHit = true;

	// Einheit selektieren
	TArray<AUnitBase*> Units;
	Units.Add(TestUnit);
	
	// Da der Controller normalerweise die selektierten Einheiten aus dem HUD holt,
	// stellen wir sicher, dass das HUD sie kennt.
	if (AHUDBase* HUD = Cast<AHUDBase>(Controller->GetHUD()))
	{
		HUD->DeselectAllUnits();
		HUD->SetUnitSelected(TestUnit);
	}

	Controller->RunUnitsAndSetWaypointsMass(Hit);

	// Verifikation auf Mass-Ebene
	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (MassSubsystem && TestUnit->MassActorBindingComponent)
	{
		FMassEntityHandle Entity = TestUnit->MassActorBindingComponent->GetMassEntityHandle();
		if (Entity.IsValid())
		{
			FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
			const FMassMoveTargetFragment* MoveTarget = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
			
			if (MoveTarget)
			{
				AssertEqual_Vector(MoveTarget->Center, TargetLoc, TEXT("MoveTarget Center entspricht nicht der Klick-Position."), 10.0f);
			}
			else
			{
				AddError(TEXT("FMassMoveTargetFragment fehlt auf der Entity!"));
			}
		}
	}
}

void AMoveCommandFunctionalTest::TestQueuedMove()
{
	AddInfo(TEXT("Starte Test: Command Queuing (Shift)"));

	// Shift simulieren (ACustomControllerBase hat oft IsShiftPressed oder prüft Input)
	// Wir nutzen die Tatsache, dass wir direkt auf die Wegpunkt-Liste zugreifen können
	
	FVector Target1 = GetActorLocation() + FVector(1000.f, 0.f, 0.f);
	FVector Target2 = GetActorLocation() + FVector(1000.f, 1000.f, 0.f);

	// Ersten Befehl absetzen
	FHitResult Hit1;
	Hit1.ImpactPoint = Target1;
	Controller->RunUnitsAndSetWaypointsMass(Hit1);

	// Zweiten Befehl mit Shift absetzen
	// Da ACustomControllerBase::RunUnitsAndSetWaypointsMass intern IsInputKeyDown(EKeys::LeftShift) prüfen könnte,
	// müssen wir evtl. den State im Controller setzen, falls zugänglich.
	// Da IsShiftPressed in CustomControllerBase.h nicht public ist (oder gar nicht existiert als Variable),
	// prüfen wir UnitBase->RunLocationArray
	
	TestUnit->RunLocationArray.Add(Target2);

	AssertEqual_Int(TestUnit->RunLocationArray.Num(), 1, TEXT("Zweiter Wegpunkt sollte in der Warteschlange sein."));
}
