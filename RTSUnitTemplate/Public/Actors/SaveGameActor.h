#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaveGameActor.generated.h"

class UCapsuleComponent;
class USaveGameWidget;
class AUnitBase;

UCLASS()
class RTSUNITTEMPLATE_API ASaveGameActor : public AActor
{
    GENERATED_BODY()

public:
    ASaveGameActor();

    UFUNCTION()
    void CloseWidget();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCapsuleComponent* OverlapCapsule;

    // Widgetklasse, die beim Overlap angezeigt wird
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<USaveGameWidget> SaveGameWidgetClass;

    // Slotname, in den gespeichert wird
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
    FString SaveSlotName = TEXT("QuickSave");

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    USaveGameWidget* ActiveWidget;

    FTimerHandle WidgetCloseTimerHandle;
};
