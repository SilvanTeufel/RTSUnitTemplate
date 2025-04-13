// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/FogOfWarCentralManager.h"

#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Actors/FogOfWarManager.h"         // Your individual fog manager
#include "Characters/Unit/PerformanceUnit.h" // Change to appropriate header for your unit class
#include "Characters/Unit/UnitBase.h"

AFogOfWarCentralManager::AFogOfWarCentralManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = TickInterval; 
}

void AFogOfWarCentralManager::BeginPlay()
{
    Super::BeginPlay();

    // Optionally populate FogManagers array by scanning the world (if not set via editor)
    // for example:
    /*
    FogManagers.Empty();
    for (TActorIterator<AFogOfWarManager> It(GetWorld()); It; ++It)
    {
         FogManagers.Add(*It);
    }*/
}

void AFogOfWarCentralManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    UpdateFogCollisions();
}

void AFogOfWarCentralManager::UpdateFogCollisions()
{
    // Get all overlapping actors of interest (for example, Units that extend from APerformanceUnit)
    TArray<AActor*> OverlappingActors;
   

    // Do RemainingActors = LastOverlappingActors - OverlappingActors;
    

            // Example processing: you might want to update unit visibility centrally
            // or notify individual fog managers for further processing.
            // For instance, here we distribute the event to all fog managers that cover this area:
            for (AFogOfWarManager* FogManager : FogManagers)
            {
                if (!IsValid(FogManager)) continue;
                
                AUnitBase* OwningUnit = Cast<AUnitBase>(FogManager->OwningUnit);
                
                if (!IsValid(OwningUnit))  continue;
                    if (OwningUnit->GetUnitState() == UnitData::Dead) continue;
              
                FogManager->GetOverlappingActors(OverlappingActors, APerformanceUnit::StaticClass());
                TArray<AActor*> RemainingActors;
                
                for (AActor* LastActor : FogManager->LastOverlappingActors)
                {
                    if (!OverlappingActors.Contains(LastActor))
                    {
                        RemainingActors.Add(LastActor);
                    }
                }

                TArray<AActor*> NewActors;
                for (AActor* Actor : OverlappingActors)
                {
                    if (!FogManager->LastOverlappingActors.Contains(Actor))
                    {
                        NewActors.Add(Actor);
                    }
                }
                
                for (AActor* NewActor : NewActors)
                {
                    if (APerformanceUnit* Unit = Cast<APerformanceUnit>(NewActor))
                    {
                        if (FogManager->TeamId != Unit->TeamId)
                        {
                            // You might add a function to check if the Unit is inside the fog manager’s region.
                            // If so, update visibility.
                            // This sample assumes that the FogManager already has a method called ProcessMergedCollision.
                            //FogManager->HandleBeginOverlapDetection(Unit);
                            FogManager->AddUnitToChase(Unit);
                        }
                    }
                }

               
                for (AActor* RemainingActor : RemainingActors)
                {
                    FogManager->HandleEndOverlapDetection(RemainingActor);
                }
                
                FogManager->LastOverlappingActors = OverlappingActors;
            }
    
   
}
