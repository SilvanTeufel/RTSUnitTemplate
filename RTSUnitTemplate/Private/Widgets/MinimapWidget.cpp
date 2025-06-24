// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MinimapWidget.h" // Change this path to match your file structure
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/MinimapActor.h" // Change this path to match your file structure
#include "Materials/MaterialInstanceDynamic.h"
#include "Controller/PlayerController/CustomControllerBase.h" // Change this path to match your file structure
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"

void UMinimapWidget::InitializeForTeam(int32 TeamId)
{
    // --- 1. Find the correct MinimapActor for this player ---
    if (!GetWorld()) return;

    // Use the TeamId passed into this function
    const int32 PlayerTeamId = TeamId;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMinimapActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AMinimapActor* MinimapActor = Cast<AMinimapActor>(Actor);
        if (MinimapActor && MinimapActor->TeamId == PlayerTeamId)
        {
            MinimapActorRef = MinimapActor;
            break; // Found our actor, no need to search further
        }
    }
    if (MinimapActorRef && MinimapImage)
    {
        // 1. Erstellen Sie eine dynamische Instanz des Materials, das dem Image zugewiesen ist.
        UTextureRenderTarget2D* TopoTexture = MinimapActorRef->GetTopographyTexture();
        UTexture2D* DataTexture = MinimapActorRef->GetDynamicDataTexture();
        UMaterialInstanceDynamic* MID = MinimapImage->GetDynamicMaterial();

        if (MID)
        {
            // 2. Setzen Sie den Textur-Parameter mit dem Namen "MinimapTexture" auf unsere Laufzeit-Textur.
            if (TopoTexture)
            {
                MID->SetTextureParameterValue(TEXT("TopographyTexture"), TopoTexture);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TopographyTexture NOT FOUND for Team ID: %d"), PlayerTeamId);
            }
            
            if (DataTexture)
            {
                MID->SetTextureParameterValue(TEXT("MinimapTexture"), DataTexture);
            }else
            {
                UE_LOG(LogTemp, Warning, TEXT("MinimapWidget NOT FOUND for Team ID: %d"), PlayerTeamId);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MinimapWidget failed to find MinimapActor for Team ID %d or MinimapImage is not bound!"), PlayerTeamId);
    }
}

/*
void UMinimapWidget::InitializeForTeam(int32 TeamId)
{
    // --- 1. Find the correct MinimapActor for this player ---
    if (!GetWorld()) return;

    // Use the TeamId passed into this function
    const int32 PlayerTeamId = TeamId;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMinimapActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AMinimapActor* MinimapActor = Cast<AMinimapActor>(Actor);
        if (MinimapActor && MinimapActor->TeamId == PlayerTeamId)
        {
            MinimapActorRef = MinimapActor;
            break; // Found our actor, no need to search further
        }
    }

    // --- 2. Bind the texture to our Image widget ---
    if (MinimapActorRef && MinimapImage)
    {
        MinimapImage->SetBrushFromTexture(MinimapActorRef->MinimapTexture);
        UE_LOG(LogTemp, Log, TEXT("MinimapWidget successfully bound texture for Team ID: %d"), PlayerTeamId);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MinimapWidget failed to find MinimapActor for Team ID %d or MinimapImage is not bound!"), PlayerTeamId);
    }
}
*/
FReply UMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // We only care about the left mouse button for moving the camera.
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Get the position of the click in the widget's local space.
        FVector2D LocalMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        MoveCameraToMinimapLocation(LocalMousePosition, InGeometry);

        // Return FReply::Handled() to signify that we have processed this input event.
        return FReply::Handled();
    }

    // If it's any other button, let the event bubble up.
    return FReply::Unhandled();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // 1. Den Kamera-Pawn holen.
    ACameraBase* CameraPawn = Cast<ACameraBase>(GetOwningPlayerPawn());
    if (CameraPawn && MinimapImage)
    {
        // 2. Den aktuellen Yaw-Winkel der Kamera über unsere neue Getter-Funktion abfragen.
        const float CameraYaw = CameraPawn->SpringArmRotator.Yaw;

        // 3. Den finalen Winkel für das Bild berechnen.
        // Wir starten mit unserer Basis-Rotation von -90 Grad.
        // Wenn die Kamera sich dreht, muss die Minimap sich "gegenläufig" drehen,
        // damit die Ausrichtung wieder stimmt. Daher ziehen wir den Kamera-Yaw ab.
        CurrentMapAngle = -90.0f - CameraYaw;

        // 4. Den Rotationswinkel auf das Bild anwenden.
        MinimapImage->SetRenderTransformAngle(CurrentMapAngle);
    }
}

void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry)
{
    if (!MinimapActorRef) return;
    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn) return;

    const FVector2D WidgetSize = WidgetGeometry.GetLocalSize();
    if (WidgetSize.X <= 0 || WidgetSize.Y <= 0) return;

    // --- DYNAMISCHE ROTATIONS-LOGIK ---

    // 1. Den Mittelpunkt des Widgets berechnen.
    const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);

    // 2. Die Klick-Position relativ zum Mittelpunkt berechnen.
    const float dx = LocalMousePosition.X - Center.X;
    const float dy = LocalMousePosition.Y - Center.Y;

    // 3. Die "Gegen-Rotation" berechnen, um den Klick zu korrigieren.
    // Wir nehmen den negativen Winkel, den wir im Tick berechnet haben.
    const float AngleRad = FMath::DegreesToRadians(-CurrentMapAngle);
    const float CosAngle = FMath::Cos(AngleRad);
    const float SinAngle = FMath::Sin(AngleRad);

    // 4. Die volle 2D-Rotationsformel anwenden, um die korrigierte Position zu finden.
    const float RotatedX_relative = dx * CosAngle - dy * SinAngle;
    const float RotatedY_relative = dx * SinAngle + dy * CosAngle;

    // 5. Die rotierten Koordinaten zurück in den lokalen Widget-Raum umrechnen.
    const FVector2D CorrectedLocalPosition(RotatedX_relative + Center.X, RotatedY_relative + Center.Y);
    
    // --- ENDE DER DYNAMISCHEN LOGIK ---

    // 6. Die finalen UVs berechnen (dieser Teil bleibt gleich).
    const float U = FMath::Clamp(CorrectedLocalPosition.X / WidgetSize.X, 0.f, 1.f);
    const float V = FMath::Clamp(CorrectedLocalPosition.Y / WidgetSize.Y, 0.f, 1.f);

    // Der Rest der Funktion bleibt unverändert.
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    const float WorldExtentX = Max.X - Min.X;
    const float WorldExtentY = Max.Y - Min.Y;

    const float TargetWorldX = Min.X + (U * WorldExtentX);
    const float TargetWorldY = Min.Y + (V * WorldExtentY);

    const FVector CurrentLocation = PlayerPawn->GetActorLocation();
    const FVector NewLocation(TargetWorldX, TargetWorldY, CurrentLocation.Z);

    PlayerPawn->SetActorLocation(NewLocation);
}

/*
void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry)
{
    if (!MinimapActorRef) return;

    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn) return;

    // Schritt 1: Die Größe des Widgets auf dem Bildschirm abrufen.
    const FVector2D WidgetSize = WidgetGeometry.GetLocalSize();
    if (WidgetSize.X <= 0 || WidgetSize.Y <= 0) return;


    // --- KORREKTE LOGIK FÜR DIE 90-GRAD-ROTATIONSKORREKTUR ---

    // Schritt 2: Den Mittelpunkt des Widgets berechnen.
    const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);

    // Schritt 3: Die Klick-Position relativ zum Mittelpunkt berechnen.
    const float dx = LocalMousePosition.X - Center.X;
    const float dy = LocalMousePosition.Y - Center.Y;

    // Schritt 4: Eine -90-Grad-Rotation (gegen den Uhrzeigersinn) auf die relativen Koordinaten anwenden.
    // Dies kompensiert die visuelle Drehung.
    // Neue X-Koordinate = -alte Y-Koordinate
    // Neue Y-Koordinate =  alte X-Koordinate
    const float RotatedX_relative = -dy;
    const float RotatedY_relative = dx;

    // Schritt 5: Die rotierten Koordinaten zurück in den lokalen Widget-Raum umrechnen (Ursprung oben links).
    const FVector2D CorrectedLocalPosition(RotatedX_relative + Center.X, RotatedY_relative + Center.Y);
    
    // --- ENDE DER KORREKTUR-LOGIK ---


    // Schritt 6: Die finalen UV-Koordinaten (0.0 bis 1.0) mit der korrigierten Position berechnen.
    const float U = FMath::Clamp(CorrectedLocalPosition.X / WidgetSize.X, 0.f, 1.f);
    const float V = FMath::Clamp(CorrectedLocalPosition.Y / WidgetSize.Y, 0.f, 1.f);

    // Der Rest der Funktion zur Berechnung der Welt-Position bleibt unverändert.
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    const float WorldExtentX = Max.X - Min.X;
    const float WorldExtentY = Max.Y - Min.Y;

    const float TargetWorldX = Min.X + (U * WorldExtentX);
    const float TargetWorldY = Min.Y + (V * WorldExtentY);

    const FVector CurrentLocation = PlayerPawn->GetActorLocation();
    const FVector NewLocation(TargetWorldX, TargetWorldY, CurrentLocation.Z);

    PlayerPawn->SetActorLocation(NewLocation);
}
*/