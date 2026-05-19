// IO Builder LiveLink - custom "Lens" role (UE 5.3+).

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkRole.h"
#include "LiveLinkIOBLensRole.generated.h"

/**
 * Role for streamed lens distortion + intrinsics. Pushed as its own subject
 * ("<Camera> Lens") so it travels with the camera tracking but is fully
 * optional and never affects the standard Camera-role subject.
 */
UCLASS(BlueprintType, meta = (DisplayName = "IOBuilder Lens Role"))
class IOBUILDERLIVELINK_API ULiveLinkIOBLensRole : public ULiveLinkRole
{
	GENERATED_BODY()

public:
	virtual UScriptStruct* GetStaticDataStruct() const override;
	virtual UScriptStruct* GetFrameDataStruct() const override;
	virtual UScriptStruct* GetBlueprintDataStruct() const override;

	virtual bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData,
		FLiveLinkBlueprintDataStruct& OutBlueprintData) const override;

	virtual FText GetDisplayName() const override;
};
