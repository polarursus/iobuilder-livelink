// IO Builder LiveLink - custom "Lens" role implementation.

#include "LiveLinkIOBLensRole.h"
#include "LiveLinkIOBLensTypes.h"
#include "LiveLinkRoleTrait.h"

#define LOCTEXT_NAMESPACE "LiveLinkIOBLensRole"

UScriptStruct* ULiveLinkIOBLensRole::GetStaticDataStruct() const
{
	return FLiveLinkIOBLensStaticData::StaticStruct();
}

UScriptStruct* ULiveLinkIOBLensRole::GetFrameDataStruct() const
{
	return FLiveLinkIOBLensFrameData::StaticStruct();
}

UScriptStruct* ULiveLinkIOBLensRole::GetBlueprintDataStruct() const
{
	return FLiveLinkIOBLensBlueprintData::StaticStruct();
}

bool ULiveLinkIOBLensRole::InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData,
	FLiveLinkBlueprintDataStruct& OutBlueprintData) const
{
	bool bSuccess = false;

	FLiveLinkIOBLensBlueprintData* BlueprintData =
		OutBlueprintData.Cast<FLiveLinkIOBLensBlueprintData>();
	const FLiveLinkIOBLensStaticData* StaticData =
		InSourceData.StaticData.Cast<FLiveLinkIOBLensStaticData>();
	const FLiveLinkIOBLensFrameData* FrameData =
		InSourceData.FrameData.Cast<FLiveLinkIOBLensFrameData>();

	if (BlueprintData && StaticData && FrameData)
	{
		BlueprintData->StaticData = *StaticData;
		BlueprintData->FrameData = *FrameData;
		bSuccess = true;
	}

	return bSuccess;
}

FText ULiveLinkIOBLensRole::GetDisplayName() const
{
	return LOCTEXT("IOBLensRole", "IOBuilder Lens");
}

#undef LOCTEXT_NAMESPACE
