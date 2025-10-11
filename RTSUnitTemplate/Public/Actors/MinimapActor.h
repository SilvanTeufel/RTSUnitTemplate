// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "MinimapActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AMinimapActor : public AActor
{
    GENERATED_BODY()

public:
    AMinimapActor();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    USceneCaptureComponent2D* SceneCaptureComponent;

    /** Die Textur, in die das "Foto" der Karte gerendert wird. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    UTextureRenderTarget2D* TopographyRenderTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    UBoxComponent* MapBoundsComponent;
public:

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void CaptureMapTopography();

    // Getter-Funktionen, damit das Widget die Texturen sicher abrufen kann
    UFUNCTION(BlueprintPure, Category = "Minimap")
    UTexture2D* GetDynamicDataTexture() const { return MinimapTexture; } // Die Textur für Nebel/Einheiten

    UFUNCTION(BlueprintPure, Category = "Minimap")
    UTextureRenderTarget2D* GetTopographyTexture() const { return TopographyRenderTarget; } // Die neue Textur für die Karte

    /** The team this minimap belongs to. Only this team's fog will be revealed. */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 TeamId = 0;

    /** The generated minimap texture, ready to be used in a UMG widget. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    UTexture2D* MinimapTexture;

    /** The resolution of the minimap texture (e.g., 256 for a 256x256 texture). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 MinimapTexSize = 256;

    /** The background color of the map where fog of war is not active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor BackgroundColor = FColor(50, 60, 50, 255);

    /** The color of the fog of war covering unexplored areas. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor FogColor = FColor(20, 25, 20, 255);

    /** The color used to draw friendly units on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor FriendlyUnitColor = FColor(0, 255, 0, 255);

    /** The color used to draw enemy units on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor EnemyUnitColor = FColor(255, 0, 0, 255);

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    //float Size = 250.f;
    /** World-space bounds for the minimap. Should match your FogActor's bounds. */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, Category = "Minimap")
    FVector2D MinimapMinBounds = FVector2D(-250*40.f, -250*40.f);
    //FVector2D MinimapMinBounds = FVector2D(-Size*40.f, -Size*40.f);

    /** World-space bounds for the minimap. Should match your FogActor's bounds. */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated,Category = "Minimap")
    FVector2D MinimapMaxBounds = FVector2D(250*40.f, 250*40.f);
   // FVector2D MinimapMaxBounds = FVector2D(Size*40.f, Size*40.f);
    
    UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "Minimap")
    void Multicast_UpdateMinimap(
        const TArray<AUnitBase*>& UnitRefs, // NEUER PARAMETER
        const TArray<FVector_NetQuantize>& Positions,
        const TArray<float>& UnitRadii,
        const TArray<float>& FogRadii,
        const TArray<uint8>& UnitTeamIds);

private:
    /** Initializes the transient texture and pixel buffer. */
    void InitMinimapTexture();

    /** Helper function to draw a filled circle into the pixel array. */
    void DrawFilledCircle(TArray<FColor>& Pixels, int32 TexSize, int32 CenterX, int32 CenterY, int32 Radius, const FColor& Color);

    /** The raw pixel data for the minimap texture. */
    UPROPERTY()
    TArray<FColor> MinimapPixels;
};