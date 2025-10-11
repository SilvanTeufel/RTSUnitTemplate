// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MinimapWidget.h" // Change this path to match your file structure
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/MinimapActor.h" // Change this path to match your file structure
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
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
/*
void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry)
{
    if (!MinimapActorRef) return;
    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn) return;

    ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(PlayerPawn->GetController());

    if (!CameraPC) return;
    
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

    
    // --- MODIFIED: LineTrace to find Ground Z-Coordinate only if not flying ---
    
    float TargetWorldZ = CurrentLocation.Z; // Default to the pawn's current Z-height.
    
    // Attempt to get the character's movement component.
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn);
    UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

    // Only perform the ground trace if we have a valid movement component AND the character is not flying.
    if (MovementComponent && MovementComponent->MovementMode != EMovementMode::MOVE_Flying)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            const FVector TraceStart(TargetWorldX, TargetWorldY, 10000.0f);
            const FVector TraceEnd(TargetWorldX, TargetWorldY, -10000.0f);

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(PlayerPawn);

            if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
            {
                // If we hit the ground, use that Z-coordinate.
                TargetWorldZ = HitResult.ImpactPoint.Z + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }
        }
    }
    // If the character is flying, TargetWorldZ remains at the current actor Z, so they don't snap to the ground.
    
    // --- END of LineTrace Logic ---

    // Construct the new location.
    const FVector NewLocation(TargetWorldX, TargetWorldY, TargetWorldZ);
    
    PlayerPawn->SetActorLocation(NewLocation);
    CameraPC->Server_SetCameraLocation(NewLocation);
}*/


void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry)
{
    // Define a log category, or use an existing one like LogTemp.
    // Make sure "LogTemp" is defined in your project's logging setup.
    const FString LogPrefix = FString::Printf(TEXT("MinimapClick [%s]:"), *UKismetSystemLibrary::GetDisplayName(this));

    if (!MinimapActorRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s Aborting: MinimapActorRef is null."), *LogPrefix);
        return;
    }
    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s Aborting: GetOwningPlayerPawn is null."), *LogPrefix);
        return;
    }

    ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(PlayerPawn->GetController());
    if (!CameraPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s Aborting: PlayerController is not of type ACameraControllerBase."), *LogPrefix);
        return;
    }
    
    const FVector2D WidgetSize = WidgetGeometry.GetLocalSize();
    if (WidgetSize.X <= 0 || WidgetSize.Y <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s Aborting: WidgetSize is invalid (%s)."), *LogPrefix, *WidgetSize.ToString());
        return;
    }

    // --- LOG INITIAL INPUTS ---
    UE_LOG(LogTemp, Log, TEXT("%s 1. Initial Inputs: LocalMousePosition=%s, WidgetSize=%s"), *LogPrefix, *LocalMousePosition.ToString(), *WidgetSize.ToString());

    // --- DYNAMISCHE ROTATIONS-LOGIK ---

    // 1. Den Mittelpunkt des Widgets berechnen.
    const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);

    // 2. Die Klick-Position relativ zum Mittelpunkt berechnen.
    const float dx = LocalMousePosition.X - Center.X;
    const float dy = LocalMousePosition.Y - Center.Y;

    // --- LOG PRE-ROTATION DATA ---
    UE_LOG(LogTemp, Log, TEXT("%s 2. Pre-Rotation: Center=%s, RelativeClick(dx,dy)=(%f, %f), CurrentMapAngle=%f"), *LogPrefix, *Center.ToString(), dx, dy, CurrentMapAngle);

    // 3. Die "Gegen-Rotation" berechnen, um den Klick zu korrigieren.
    const float AngleRad = FMath::DegreesToRadians(-CurrentMapAngle);
    const float CosAngle = FMath::Cos(AngleRad);
    const float SinAngle = FMath::Sin(AngleRad);

    // 4. Die volle 2D-Rotationsformel anwenden, um die korrigierte Position zu finden.
    const float RotatedX_relative = dx * CosAngle - dy * SinAngle;
    const float RotatedY_relative = dx * SinAngle + dy * CosAngle;

    // 5. Die rotierten Koordinaten zurück in den lokalen Widget-Raum umrechnen.
    const FVector2D CorrectedLocalPosition(RotatedX_relative + Center.X, RotatedY_relative + Center.Y);
    
    // --- LOG POST-ROTATION DATA ---
    UE_LOG(LogTemp, Log, TEXT("%s 3. Post-Rotation: CorrectedLocalPosition=%s"), *LogPrefix, *CorrectedLocalPosition.ToString());
    
    // 6. Die finalen UVs berechnen.
    const float U = FMath::Clamp(CorrectedLocalPosition.X / WidgetSize.X, 0.f, 1.f);
    const float V = FMath::Clamp(CorrectedLocalPosition.Y / WidgetSize.Y, 0.f, 1.f);

    // --- LOG UV AND WORLD BOUNDS ---
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    UE_LOG(LogTemp, Log, TEXT("%s 4. UV Coords: (U,V)=(%f, %f). World Bounds: Min=%s, Max=%s"), *LogPrefix, U, V, *Min.ToString(), *Max.ToString());

    const float WorldExtentX = Max.X - Min.X;
    const float WorldExtentY = Max.Y - Min.Y;

    const float TargetWorldX = Min.X + (U * WorldExtentX);
    const float TargetWorldY = Min.Y + (V * WorldExtentY);

    const FVector CurrentLocation = PlayerPawn->GetActorLocation();
    
    // --- MODIFIED: LineTrace to find Ground Z-Coordinate only if not flying ---
    float TargetWorldZ = CurrentLocation.Z; 
    
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn);
    UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

    if (MovementComponent && MovementComponent->MovementMode != EMovementMode::MOVE_Flying)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            const FVector TraceStart(TargetWorldX, TargetWorldY, 10000.0f);
            const FVector TraceEnd(TargetWorldX, TargetWorldY, -10000.0f);

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(PlayerPawn);

            if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
            {
                // If we hit the ground, use that Z-coordinate.
                TargetWorldZ = HitResult.ImpactPoint.Z + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }
        }
    }
    
    const FVector NewLocation(TargetWorldX, TargetWorldY, TargetWorldZ);
    
    // --- LOG FINAL LOCATION BEFORE SENDING ---
    UE_LOG(LogTemp, Log, TEXT("%s 5. FINAL CLIENT LOCATION: NewLocation=%s. Sending to server..."), *LogPrefix, *NewLocation.ToString());

    // This sets the location locally for immediate feedback.
    PlayerPawn->SetActorLocation(NewLocation); 
    
    // This sends the calculated location to the server for authoritative movement.
    CameraPC->Server_SetCameraLocation(NewLocation);
}