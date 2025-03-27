// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/FogOfWarManager.h"

#include "Characters/Unit/HealingUnit.h"
#include "Hud/HUDBase.h"

#include "Characters/Unit/PerformanceUnit.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"


AFogOfWarManager::AFogOfWarManager()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetRenderInMainPass(false);
    Mesh->SetRenderInDepthPass(false);
    Mesh->SetCastShadow(false);
    
}


void AFogOfWarManager::BeginPlay()
{
    Super::BeginPlay();

   // CheckForCollisions();
}

void AFogOfWarManager::CheckForCollisions()
{
    // Ensure the overlap state is up to date
    Mesh->UpdateOverlaps();

    // Get all overlapping actors (you can filter by class if needed)
    TArray<AActor*> OverlappingActors;
    Mesh->GetOverlappingActors(OverlappingActors, APerformanceUnit::StaticClass());

    // Process each overlapping actor
    for (AActor* Actor : OverlappingActors)
    {
        if (APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor))
        {
            // Only process enemy units
            if (PlayerTeamId != Unit->TeamId) // PlayerTeamId
            {
                Unit->IsVisibleEnemy = true;
                Unit->FogManagerOverlaps++;
            }
        }
    }
}


void AFogOfWarManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
}


void AFogOfWarManager::AddUnitToChase(AActor* OtherActor)
{
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }
    
    // Get the current (detecting) unit from OwningUnit
    AUnitBase* CurrentUnit = Cast<AUnitBase>(OwningUnit);
    if (!CurrentUnit)
    {
        return;
    }

    if (PlayerController)
    {
        AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
        ControllerBase->AddUnitToChase(CurrentUnit, OtherActor);
    }
}

void AFogOfWarManager::RemoveUnitToChase_Implementation(AActor* OtherActor)
{
    // Check if the actor is valid, not pending kill, and its low-level memory is okay.
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }
    
    
    AUnitBase* DetectingUnit = Cast<AUnitBase>(OwningUnit);
    if (!DetectingUnit) return;


    if (PlayerController)
    {
        AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
        ControllerBase->RemoveUnitToChase(DetectingUnit, OtherActor);
    }
}

void AFogOfWarManager::HandleBeginOverlapDetection(AActor* OtherActor)
{

    if (!ManagerSetsVisibility) return;
    
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }
    
    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
    AUnitBase* DetectingUnit = Cast<AUnitBase>(OwningUnit);
    
    if (Unit && PlayerTeamId != Unit->TeamId)
    {
        if (DetectingUnit && DetectingUnit->CanDetectInvisible && Unit->IsInvisible)
        {
            Unit->IsInvisible = false;
            Unit->DetectorOverlaps++;
        }
        
        Unit->IsVisibleEnemy = true;
        Unit->FogManagerOverlaps++;

        /*
        // Delay adding the unit to allow its TeamId to be set
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Unit]()
        {
            AddUnitToChase(Unit);
        }, 0.1f, false);
        */
    }
    
}

void AFogOfWarManager::OnMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    HandleBeginOverlapDetection(OtherActor);
    AddUnitToChase(OtherActor);
}



// Existing code: Handle end overlap adjustments
void AFogOfWarManager::HandleEndOverlapDetection(AActor* OtherActor)
{

    if (!ManagerSetsVisibility) return;
    
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }
    
    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
    AUnitBase* DetectingUnit = Cast<AUnitBase>(OwningUnit);
    
    if (Unit && PlayerTeamId != Unit->TeamId)
    {
        Unit->FogManagerOverlaps--;
        if(Unit->FogManagerOverlaps < 0) Unit->FogManagerOverlaps = 0;

        if (DetectingUnit && DetectingUnit->CanDetectInvisible && Unit->DetectorOverlaps > 0)
        {
            Unit->DetectorOverlaps--;

            if (Unit->DetectorOverlaps <= 0)
            {
                FTimerHandle TimerHandleDetector;
                GetWorld()->GetTimerManager().SetTimer(TimerHandleDetector, [Unit]()
                {
                    Unit->IsInvisible = true;
                }, 1.f, false);
            }
        }
        
        if(Unit->FogManagerOverlaps > 0) return;

  
        if (Unit->Attributes->GetHealth() <= 0.f) // Unit->GetUnitState() == UnitData::Dead
        {
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, [Unit]() {
                Unit->IsVisibleEnemy = false;
            }, Unit->FogDeadVisibilityTime, false);
        }else
        {
            Unit->IsVisibleEnemy = false;
        }
    }
}

void AFogOfWarManager::OnMeshEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    HandleEndOverlapDetection(OtherActor);

    /*
    FTimerHandle TimerHandle;
 
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, OtherActor]() {
  
        RemoveUnitToChase(OtherActor);
    }, 1.5f, false);

    */
}
