// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/FogOfWarManager.h"
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


void AFogOfWarManager::OnMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
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
    }
}

void AFogOfWarManager::OnMeshEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

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
