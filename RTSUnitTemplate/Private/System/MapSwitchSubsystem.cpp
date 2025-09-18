#include "System/MapSwitchSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

FString UMapSwitchSubsystem::NormalizeMapKey(const FString& MapIdentifier) const
{
    if (MapIdentifier.IsEmpty())
    {
        return FString();
    }

    // 1) Versuche, aus einem Long-Package-Namen den Asset-Namen zu gewinnen
    FString AssetName = FPackageName::GetLongPackageAssetName(MapIdentifier);

    // 2) Fallback: nutze Basename (funktioniert auch für Pfade und einfache Namen)
    if (AssetName.IsEmpty())
    {
        AssetName = FPaths::GetBaseFilename(MapIdentifier);
    }

    // 3) Entferne PIE-Präfix "UEDPIE_<n>_" falls vorhanden
    if (AssetName.StartsWith(TEXT("UEDPIE_")))
    {
        // "UEDPIE_<n>_LevelName" => finde das zweite '_' und schneide davor ab
        const int32 FirstUnderscore = AssetName.Find(TEXT("_"));
        const int32 SecondUnderscore = AssetName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstUnderscore + 1);
        if (SecondUnderscore != INDEX_NONE)
        {
            AssetName = AssetName.Mid(SecondUnderscore + 1);
        }
    }

    return AssetName;
}

void UMapSwitchSubsystem::MarkSwitchEnabledForMap(const FString& MapLongPackageName, FName SwitchTag)
{
    if (MapLongPackageName.IsEmpty() || SwitchTag.IsNone())
    {
        return;
    }

    const FString Key = NormalizeMapKey(MapLongPackageName);
    TSet<FName>& EnabledSet = EnabledSwitchTagsByMap.FindOrAdd(Key);
    EnabledSet.Add(SwitchTag);
}

bool UMapSwitchSubsystem::IsSwitchEnabledForMap(const FString& MapLongPackageName, FName SwitchTag) const
{
    if (MapLongPackageName.IsEmpty() || SwitchTag.IsNone())
    {
        return false;
    }

    const FString Key = NormalizeMapKey(MapLongPackageName);
    if (const TSet<FName>* FoundSet = EnabledSwitchTagsByMap.Find(Key))
    {
        return FoundSet->Contains(SwitchTag);
    }

    return false;
}
