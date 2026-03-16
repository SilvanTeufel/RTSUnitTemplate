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
    UBoxComponent* MapBoundsComponent;

    /** CPU-generated topography texture (LineTrace-based, no SceneCapture). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    UTexture2D* TopographyTexture;

public:

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void CaptureMapTopography();

    // Getter-Funktionen, damit das Widget die Texturen sicher abrufen kann
    UFUNCTION(BlueprintPure, Category = "Minimap")
    UTexture2D* GetDynamicDataTexture() const { return MinimapTexture; } // Die Textur für Nebel/Einheiten

    UFUNCTION(BlueprintPure, Category = "Minimap")
    UTexture2D* GetTopographyTexture() const { return TopographyTexture; } // CPU-generierte Topographie

    /** The team this minimap belongs to. Only this team's fog will be revealed. */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 TeamId = 0;

    /** Brightness adjustment for the topography (offsets the color). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    float MinimapBrightness = 0.0f;

    /** Contrast adjustment for the topography (multiplies contrast). 1.0 is default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    float MinimapContrast = 1.0f;

    /** The generated minimap texture, ready to be used in a UMG widget. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
    UTexture2D* MinimapTexture;

    /** The resolution of the minimap texture (e.g., 256 for a 256x256 texture). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 MinimapTexSize = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float DotMultiplier = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float MapSwitcherDotMultiplier = 4.0f;

    /** The delay before capturing the map topography. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float DelayTime = 2.f;
    /** The background color of the map where fog of war is not active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor BackgroundColor = FColor(50, 60, 50, 0);

    /** The color of the fog of war covering unexplored areas. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor FogColor = FColor(54, 54, 54, 0.194);

    /** Opacity of the fog of war (0.0 = fully transparent, 1.0 = fully opaque). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FogOpacity = 0.8f;

    /** The color used to draw friendly units on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor FriendlyUnitColor = FColor(0, 255, 0, 255);

    /** The color used to draw enemy units on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor EnemyUnitColor = FColor(255, 0, 0, 255);

    /** If true, the MapSwitchActor positions will be updated on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bLiveUpdateMapSwitcher = false;

    /** The color used to draw MapSwitchActors on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
    FColor MapSwitcherColor = FColor(255, 255, 0, 255);

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

    // --- LineTrace Topography Settings ---
    
    /** Fallback color for the lowest terrain point (used when no material color can be sampled). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    FColor TopoLowColor = FColor(0, 0, 0, 255);

    /** Fallback color for the highest terrain point (used when no material color can be sampled). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    FColor TopoHighColor = FColor(255, 255, 255, 255);

    /** Z height from which LineTraces start (should be above the highest point in the map). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    float TraceHeightStart = 50000.f;

    /** Z height where LineTraces end (should be below the lowest point in the map). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    float TraceHeightEnd = -10000.f;

    /** How much the height-based shading affects the final color (0 = pure material color, 1 = full height shading). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HeightShadingStrength = 0.5f;

    // --- Tag-based Color Overrides (highest priority) ---

    /** Color for actors tagged with "ColorA". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|TagColors")
    FLinearColor TagColorA = FLinearColor(0.8f, 0.2f, 0.2f);  // Red

    /** Color for actors tagged with "ColorB". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|TagColors")
    FLinearColor TagColorB = FLinearColor(0.2f, 0.6f, 0.8f);  // Blue

    /** Color for actors tagged with "ColorC". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|TagColors")
    FLinearColor TagColorC = FLinearColor(0.8f, 0.7f, 0.1f);  // Yellow

    /** Color for actors tagged with "ColorD". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|TagColors")
    FLinearColor TagColorD = FLinearColor(0.6f, 0.2f, 0.8f);  // Purple

    // --- Topography Color Overrides by Actor Type ---

    /** Color for Landscape terrain (dominant surface). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|ActorColors")
    FLinearColor LandscapeColor = FLinearColor(0.18f, 0.35f, 0.10f);

    /** Color for StaticMeshActors (buildings, rocks, props). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|ActorColors")
    FLinearColor StaticMeshColor = FLinearColor(0.45f, 0.42f, 0.38f);

    /** Color for foliage instances (trees, bushes). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|ActorColors")
    FLinearColor FoliageColor = FLinearColor(0.08f, 0.28f, 0.05f);

    /** Default color when actor type is unknown. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|ActorColors")
    FLinearColor DefaultActorColor = FLinearColor(0.3f, 0.28f, 0.2f);

    // --- WorkArea Minimap Colors ---

    /** Color for Primary WorkAreas on the topography map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas")
    FColor WorkAreaPrimaryColor = FColor(0, 200, 0, 255);

    /** Color for Secondary WorkAreas on the topography map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas")
    FColor WorkAreaSecondaryColor = FColor(0, 100, 255, 255);

    /** Color for Tertiary WorkAreas on the topography map. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas")
    FColor WorkAreaTertiaryColor = FColor(255, 50, 50, 255);

    /** Radius of WorkArea markers on the topography map (in pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas", meta = (ClampMin = "1", ClampMax = "20"))
    int32 WorkAreaMarkerRadius = 5;

    /** If true, draws an outline around resource WorkArea markers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas")
    bool bDrawResourceOutline = true;

    /** Color of the resource outline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas", meta = (EditCondition = "bDrawResourceOutline"))
    FColor ResourceOutlineColor = FColor(0, 0, 0, 255);

    /** Thickness of the resource outline (in pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|WorkAreas", meta = (EditCondition = "bDrawResourceOutline", ClampMin = "1", ClampMax = "5"))
    int32 ResourceOutlineThickness = 1;

    // --- Height Edge Outlines (terrain slope detection) ---

    /** If true, draws outlines at strong height differences (ramps, cliffs). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|HeightEdges")
    bool bDrawHeightEdges = true;

    /** Color of the height-edge outlines. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|HeightEdges", meta = (EditCondition = "bDrawHeightEdges"))
    FColor HeightEdgeColor = FColor(30, 30, 30, 255);

    /** Height difference threshold (normalized 0-1) above which an edge is drawn. Lower = more edges. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|HeightEdges", meta = (EditCondition = "bDrawHeightEdges", ClampMin = "0.01", ClampMax = "1.0"))
    float HeightEdgeThreshold = 0.6f;

    // --- Landscape Noise/Pattern ---

    /** If true, applies a subtle noise pattern to landscape pixels (varies by height). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|LandscapePattern")
    bool bDrawLandscapePattern = true;

    /** Strength of the landscape noise pattern (0 = none, 1 = strong). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|LandscapePattern", meta = (EditCondition = "bDrawLandscapePattern", ClampMin = "0.0", ClampMax = "1.0"))
    float LandscapePatternStrength = 0.15f;

    /** Scale of the noise pattern. Higher = coarser pattern. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography|LandscapePattern", meta = (EditCondition = "bDrawLandscapePattern", ClampMin = "1.0", ClampMax = "50.0"))
    float LandscapePatternScale = 8.0f;

    // --- Unit Outlines (runtime minimap texture) ---

    /** If true, draws an outline around unit dots on the runtime minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors|UnitOutline")
    bool bDrawUnitOutline = true;

    /** Color of the unit outline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors|UnitOutline", meta = (EditCondition = "bDrawUnitOutline"))
    FColor UnitOutlineColor = FColor(0, 0, 0, 255);

    /** Thickness of the unit outline (in pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors|UnitOutline", meta = (EditCondition = "bDrawUnitOutline", ClampMin = "1", ClampMax = "5"))
    int32 UnitOutlineThickness = 1;

    /** Additional actor classes to ignore during topography LineTraces (editable in Detail Panel). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Topography")
    TArray<TSubclassOf<AActor>> TopographyIgnoredActorClasses;

    // --- Viewport Indicator ---

    /** If true, draws the player camera viewport rectangle on the minimap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Viewport")
    bool bDrawViewport = true;

    /** Color of the viewport rectangle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Viewport", meta = (EditCondition = "bDrawViewport"))
    FColor ViewportColor = FColor(255, 255, 255, 255);

    /** Thickness of the viewport rectangle outline (in pixels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Viewport", meta = (EditCondition = "bDrawViewport", ClampMin = "1", ClampMax = "5"))
    int32 ViewportLineThickness = 1;
    
    UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "Minimap")
    void Multicast_UpdateMinimap(
        const TArray<AUnitBase*>& UnitRefs, // NEUER PARAMETER
        const TArray<FVector_NetQuantize>& Positions,
        const TArray<float>& UnitRadii,
        const TArray<float>& FogRadii,
        const TArray<uint8>& UnitTeamIds);

    // Local, non-replicated update for client-side rendering
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void UpdateMinimap_Local(
        const TArray<AUnitBase*>& UnitRefs,
        const TArray<FVector_NetQuantize>& Positions,
        const TArray<float>& UnitRadii,
        const TArray<float>& FogRadii,
        const TArray<uint8>& UnitTeamIds);

private:
    /** Initializes the transient texture and pixel buffer. */
    void InitMinimapTexture();

    /** Helper function to draw a filled circle into the pixel array. */
    void DrawFilledCircle(TArray<FColor>& Pixels, int32 TexSize, int32 CenterX, int32 CenterY, int32 Radius, const FColor& Color);

    /** Helper function to draw a circle outline into the pixel array. */
    void DrawCircleOutline(TArray<FColor>& Pixels, int32 TexSize, int32 CenterX, int32 CenterY, int32 Radius, int32 Thickness, const FColor& Color);

    /** Helper function to draw a rectangle outline into the pixel array. */
    void DrawRectOutline(TArray<FColor>& Pixels, int32 TexSize, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Thickness, const FColor& Color);

    /** Tries to extract a base color from the material at the hit location. Returns true if successful. */
    bool TryGetMaterialColor(const FHitResult& Hit, FLinearColor& OutColor) const;

    /** The raw pixel data for the minimap texture. */
    UPROPERTY()
    TArray<FColor> MinimapPixels;

    FTimerHandle CaptureTimerHandle;
};
