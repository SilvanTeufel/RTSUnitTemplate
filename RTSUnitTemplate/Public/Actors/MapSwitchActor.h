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

    UFUNCTION(BlueprintPure, Category = "Map Switch")
    FName GetDestinationSwitchTagToEnable() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCapsuleComponent* OverlapCapsule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWidgetComponent* MarkerWidgetComponent;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UMapSwitchWidget> MapSwitchWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Map Switch", meta = (AllowedClasses = "World"))
    TSoftObjectPtr<UWorld> TargetMap;

    // Eindeutiger Tag für diesen Switch auf der aktuellen Map
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Switch")
    FName SwitchTag;

    // Tag, der auf der Ziel-Map aktiviert werden soll, wenn dieser Switch benutzt wird
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Switch")
    FName DestinationSwitchTagToEnable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Switch")
    bool bIsEnabled = true;

    UPROPERTY(EditAnywhere, Category = "UI")
    FText MarkerDisplayText;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    UMapSwitchWidget* ActiveWidget;
};
