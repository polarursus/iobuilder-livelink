// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "IOBuilderLiveLinkSourceFactory.h"
#include "IOBuilderLiveLinkSource.h"
#include "SIOBuilderLiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "IOBuilderLiveLinkSourceFactory"

FText UIOBuilderLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "IO Builder LiveLink");
}

FText UIOBuilderLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "IO Builder LiveLink");
}

TSharedPtr<SWidget> UIOBuilderLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(SIOBuilderLiveLinkSourceFactory)
		.OnOkClicked(SIOBuilderLiveLinkSourceFactory::FOnOkClicked::CreateUObject(this, &UIOBuilderLiveLinkSourceFactory::OnOkClicked, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> UIOBuilderLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
	FIPv4Endpoint DeviceEndPoint;
	if (!FIPv4Endpoint::Parse(InConnectionString, DeviceEndPoint))
	{
		return TSharedPtr<ILiveLinkSource>();
	}

	return MakeShared<FIOBuilderLiveLinkSource>(DeviceEndPoint);
}

void UIOBuilderLiveLinkSourceFactory::OnOkClicked(FIPv4Endpoint InEndpoint, FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	InOnLiveLinkSourceCreated.ExecuteIfBound(MakeShared<FIOBuilderLiveLinkSource>(InEndpoint), InEndpoint.ToString());
}

#undef LOCTEXT_NAMESPACE