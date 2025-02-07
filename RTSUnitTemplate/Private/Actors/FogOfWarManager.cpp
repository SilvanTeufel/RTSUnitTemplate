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

    bReplicates = false;
}


void AFogOfWarManager::BeginPlay()
{
    Super::BeginPlay();

    CheckForCollisions();
   // FTimerHandle CollisionCheckTimerHandle;
   // GetWorldTimerManager().SetTimer(CollisionCheckTimerHandle, this, &AFogOfWarManager::CheckForCollisions, 3.0f, false);
    
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
            if (PlayerTeamId != Unit->TeamId)
            {
                Unit->IsVisibileEnemy = true;
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
    APerformanceUnit* Unit = Cast<APerformanceUnit>(OtherActor);
    
    if (Unit && PlayerTeamId != Unit->TeamId)
    {
            Unit->IsVisibileEnemy = true;
            Unit->FogManagerOverlaps++;
    }
}

void AFogOfWarManager::OnMeshEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

    AUnitBase* Unit = Cast<AUnitBase>(OtherActor);

    
    if (Unit && PlayerTeamId != Unit->TeamId)
    {
        Unit->FogManagerOverlaps--;
        if(Unit->FogManagerOverlaps < 0) Unit->FogManagerOverlaps = 0;
        if(Unit->FogManagerOverlaps > 0) return;

        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [Unit]() {
            Unit->IsVisibileEnemy = false;
        }, 2.0f, false);
    }
  
}
