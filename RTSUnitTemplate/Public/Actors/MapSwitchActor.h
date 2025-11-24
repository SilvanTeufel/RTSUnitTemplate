#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSwitchActor.generated.h"

class UCapsuleComponent;
class UUserWidget;
class AUnitBase;
class UMapSwitchWidget;
class UWidgetComponent;

UCLASS()
class RTSUNITTEMPLATE_API AMapSwitchActor : public AActor
{
    GENERATED_BODY()

public:
    AMapSwitchActor();

    virtual void Tick(float DeltaTime) override;
    
    UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
    FName GetDestinationSwitchTagToEnable() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
    UCapsuleComponent* OverlapCapsule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
    UWidgetComponent* MarkerWidgetComponent;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UMapSwitchWidget> MapSwitchWidgetClass;

    UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
    TSoftObjectPtr<UWorld> TargetMap;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName SwitchTag;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FName DestinationSwitchTagToEnable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    bool bIsEnabled = true;

    UPROPERTY(EditAnywhere, Category = "UI")
    FText MarkerDisplayText;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (MakeEditWidget = true))
    FVector CenterPoint;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float RotationRadius = 500.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float RotationSpeed = 0.f;
    
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    UMapSwitchWidget* ActiveWidget;

    float CurrentAngle = 0.f;
};
