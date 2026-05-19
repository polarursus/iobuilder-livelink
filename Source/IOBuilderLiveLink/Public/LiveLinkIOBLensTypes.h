// IO Builder LiveLink - custom "Lens" role data (UE 5.3+)
// Carries the calibrated lens distortion + intrinsics streamed alongside the
// camera tracking subject. Brown-Conrady model == Unreal "Spherical" model.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#include "LiveLinkIOBLensTypes.generated.h"

USTRUCT(BlueprintType)
struct IOBUILDERLIVELINK_API FLiveLinkIOBLensStaticData : public FLiveLinkBaseStaticData
{
	GENERATED_BODY()

	// Pixel resolution the lens was calibrated at (0 if unknown).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	int32 ImageWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	int32 ImageHeight = 0;

	// Physical sensor size in millimetres (0 if unknown -> FOV fallback).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float SensorWidthMM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float SensorHeightMM = 0.0f;

	// Distortion model identifier ("Spherical" = Brown-Conrady).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	FString Model = TEXT("Spherical");
};

USTRUCT(BlueprintType)
struct IOBUILDERLIVELINK_API FLiveLinkIOBLensFrameData : public FLiveLinkBaseFrameData
{
	GENERATED_BODY()

	// Radial distortion coefficients.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float K1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float K2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float K3 = 0.0f;

	// Tangential distortion coefficients.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float P1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float P2 = 0.0f;

	// Principal-point offset, normalized (0 = image centre).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float Cx = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float Cy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float FocalLengthMM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float Fov = 0.0f;          // horizontal field of view, degrees

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float FocusDistance = 0.0f; // metres

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float Aperture = 0.0f;      // T-stop
};

USTRUCT(BlueprintType)
struct IOBUILDERLIVELINK_API FLiveLinkIOBLensBlueprintData : public FLiveLinkBaseBlueprintData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lens")
	FLiveLinkIOBLensStaticData StaticData;

	UPROPERTY(BlueprintReadOnly, Category = "Lens")
	FLiveLinkIOBLensFrameData FrameData;
};
