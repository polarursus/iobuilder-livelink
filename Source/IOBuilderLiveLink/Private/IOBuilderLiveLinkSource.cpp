// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOBuilderLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"

#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Json.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

#include "LiveLinkIOBLensRole.h"
#include "LiveLinkIOBLensTypes.h"

//enable logging step 2
DEFINE_LOG_CATEGORY(ModuleLog)

#define LOCTEXT_NAMESPACE "IOBuilderLiveLinkSource"

#define RECV_BUFFER_SIZE 1024 * 1024

FIOBuilderLiveLinkSource::FIOBuilderLiveLinkSource(FIPv4Endpoint InEndpoint)
: Client(nullptr)
, Socket(nullptr)
, SocketSubsystem(nullptr)
, Stopping(false)
, Thread(nullptr)
, WaitTime(FTimespan::FromMilliseconds(100))
{
	// defaults
	DeviceEndpoint = InEndpoint;

	SourceStatus = LOCTEXT("SourceStatus_DeviceNotFound", "Device Not Found");
	SourceType = LOCTEXT("IOBuilderLiveLinkSourceType", "IO Builder LiveLink");
	SourceMachineName = LOCTEXT("IOBuilderLiveLinkSourceMachineName", "localhost");

	if (DeviceEndpoint.Address.IsMulticastAddress())
	{
		Socket = FUdpSocketBuilder(TEXT("IOBLLSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE)

			.BoundToAddress(FIPv4Address::Any)
			.JoinedToGroup(DeviceEndpoint.Address)
			.WithMulticastLoopback()
			.WithMulticastTtl(2);

	}
	else
	{
		Socket = FUdpSocketBuilder(TEXT("IOBLLSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToAddress(DeviceEndpoint.Address)
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE);
	}

	RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);

	if ((Socket != nullptr) && (Socket->GetSocketType() == SOCKTYPE_Datagram))
	{
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		Start();

		SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");
	}
}

FIOBuilderLiveLinkSource::~FIOBuilderLiveLinkSource()
{
	Stop();
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (Socket != nullptr)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
}

void FIOBuilderLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	FScopeLock Lock(&ClientCS);
	Client = InClient;
	SourceGuid = InSourceGuid;
}


bool FIOBuilderLiveLinkSource::IsSourceStillValid() const
{
	bool bIsSourceValid = !Stopping && Thread != nullptr && Socket != nullptr;
	return bIsSourceValid;
}


bool FIOBuilderLiveLinkSource::RequestSourceShutdown()
{
	Stop();
	return true;
}

void FIOBuilderLiveLinkSource::Start()
{
	ThreadName = "IOBLL";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FIOBuilderLiveLinkSource::Stop()
{
	Stopping = true;
}

uint32 FIOBuilderLiveLinkSource::Run()
{
	TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();

	while (!Stopping)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
		{
			continue;
		}

		// Snapshot the client/guid under the lock so we never observe a
		// half-updated pair while ReceiveClient is running on the game thread.
		ILiveLinkClient* LocalClient = nullptr;
		FGuid LocalGuid;
		{
			FScopeLock Lock(&ClientCS);
			LocalClient = Client;
			LocalGuid = SourceGuid;
		}
		if (!LocalClient)
		{
			// Source isn't bound yet; drain pending data so the kernel
			// buffer doesn't grow, but don't push anywhere.
			uint32 DrainSize;
			while (Socket->HasPendingData(DrainSize))
			{
				int32 DrainRead = 0;
				Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), DrainRead, *Sender);
			}
			continue;
		}

		uint32 Size;
		while (Socket->HasPendingData(Size))
		{
			int32 Read = 0;
			if (!Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), Read, *Sender))
			{
				break;
			}
			if (Read <= 0)
			{
				continue;
			}

			// Parse + push on this thread. LiveLink push APIs are AnyThread,
			// so the previous game-thread hop was unnecessary and let the
			// queue grow unboundedly under back-pressure.
			TArray<uint8> Datagram;
			Datagram.SetNumUninitialized(Read);
			FMemory::Memcpy(Datagram.GetData(), RecvBuffer.GetData(), Read);
			HandleReceivedData(LocalClient, LocalGuid, Datagram);
		}
	}
	return 0;
}

void FIOBuilderLiveLinkSource::HandleReceivedData(ILiveLinkClient* InClient, const FGuid& InSourceGuid, const TArray<uint8>& ReceivedData)
{
	FString JsonString;
	JsonString.Empty(ReceivedData.Num());
	for (const uint8 Byte : ReceivedData)
	{
		JsonString += TCHAR(Byte);
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	for (TPair<FString, TSharedPtr<FJsonValue>>& JsonField : JsonObject->Values)
	{
		FName SubjectName(*JsonField.Key);
		const TSharedPtr<FJsonObject> SubjectObject = JsonField.Value->AsObject();
		if (!SubjectObject.IsValid())
		{
			continue;
		}

		FString Role;
		if (!SubjectObject->TryGetStringField(TEXT("Role"), Role))
		{
			UE_LOG(ModuleLog, Error, TEXT("IO Builder LiveLink - 'Role' field is missing on subject '%s'"), *JsonField.Key);
			continue;
		}

		const bool bCreateSubject = !EncounteredSubjects.Contains(SubjectName);

		if (Role == TEXT("Camera"))
		{
			if (bCreateSubject)
			{
				UE_LOG(ModuleLog, Warning, TEXT("Creating Camera Subject '%s'"), *JsonField.Key);
				FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkCameraStaticData::StaticStruct());
				FLiveLinkCameraStaticData& CameraData = *StaticDataStruct.Cast<FLiveLinkCameraStaticData>();

				// Static data is pushed only once (on subject creation).
				// These fields are optional per-frame (e.g. fov/fl/f only
				// arrive once a lens is connected, possibly after the
				// stream started), so advertise full support
				// unconditionally - an absent key just means "no update
				// this tick", the standard LiveLink contract.
				CameraData.bIsAspectRatioSupported = true;
				CameraData.bIsFieldOfViewSupported = true;
				CameraData.bIsFocalLengthSupported = true;
				CameraData.bIsProjectionModeSupported = true;
				CameraData.bIsApertureSupported = true;
				CameraData.bIsFocusDistanceSupported = true;

				InClient->PushSubjectStaticData_AnyThread({ InSourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticDataStruct));
				EncounteredSubjects.Add(SubjectName);
			}

			FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkCameraFrameData::StaticStruct());
			FLiveLinkCameraFrameData& FrameData = *FrameDataStruct.Cast<FLiveLinkCameraFrameData>();

			double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
			SubjectObject->TryGetNumberField(TEXT("tx"), x);
			SubjectObject->TryGetNumberField(TEXT("ty"), y);
			SubjectObject->TryGetNumberField(TEXT("tz"), z);
			const FVector Translation(x, y, z);

			x = 0.0; y = 0.0; z = 0.0; w = 1.0;
			SubjectObject->TryGetNumberField(TEXT("qx"), x);
			SubjectObject->TryGetNumberField(TEXT("qy"), y);
			SubjectObject->TryGetNumberField(TEXT("qz"), z);
			SubjectObject->TryGetNumberField(TEXT("qw"), w);
			const FQuat Quat(x, y, z, w);

			x = 1.0; y = 1.0; z = 1.0;
			SubjectObject->TryGetNumberField(TEXT("sx"), x);
			SubjectObject->TryGetNumberField(TEXT("sy"), y);
			SubjectObject->TryGetNumberField(TEXT("sz"), z);
			const FVector Scale(x, y, z);

			FrameData.Transform = FTransform(Quat, Translation, Scale);

			SubjectObject->TryGetNumberField(TEXT("ar"),  FrameData.AspectRatio);
			SubjectObject->TryGetNumberField(TEXT("fov"), FrameData.FieldOfView);
			SubjectObject->TryGetNumberField(TEXT("fl"),  FrameData.FocalLength);
			SubjectObject->TryGetNumberField(TEXT("ap"),  FrameData.Aperture);
			SubjectObject->TryGetNumberField(TEXT("f"),   FrameData.FocusDistance);

			bool bIsPerspective = true;
			SubjectObject->TryGetBoolField(TEXT("prm"), bIsPerspective);
			FrameData.ProjectionMode = bIsPerspective ? ELiveLinkCameraProjectionMode::Perspective : ELiveLinkCameraProjectionMode::Orthographic;

			InClient->PushSubjectFrameData_AnyThread({ InSourceGuid, SubjectName }, MoveTemp(FrameDataStruct));
		}
		else if (Role == TEXT("Lens"))
		{
			if (bCreateSubject)
			{
				UE_LOG(ModuleLog, Warning, TEXT("Creating IOBuilder Lens Subject '%s'"), *JsonField.Key);
				FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkIOBLensStaticData::StaticStruct());
				FLiveLinkIOBLensStaticData& LensStatic = *StaticDataStruct.Cast<FLiveLinkIOBLensStaticData>();

				double n = 0.0;
				if (SubjectObject->TryGetNumberField(TEXT("rx"), n)) LensStatic.ImageWidth   = (int32)n;
				if (SubjectObject->TryGetNumberField(TEXT("ry"), n)) LensStatic.ImageHeight  = (int32)n;
				if (SubjectObject->TryGetNumberField(TEXT("sw"), n)) LensStatic.SensorWidthMM  = (float)n;
				if (SubjectObject->TryGetNumberField(TEXT("sh"), n)) LensStatic.SensorHeightMM = (float)n;
				FString Model;
				if (SubjectObject->TryGetStringField(TEXT("model"), Model)) LensStatic.Model = Model;

				InClient->PushSubjectStaticData_AnyThread({ InSourceGuid, SubjectName }, ULiveLinkIOBLensRole::StaticClass(), MoveTemp(StaticDataStruct));
				EncounteredSubjects.Add(SubjectName);
			}

			FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkIOBLensFrameData::StaticStruct());
			FLiveLinkIOBLensFrameData& LensFrame = *FrameDataStruct.Cast<FLiveLinkIOBLensFrameData>();

			SubjectObject->TryGetNumberField(TEXT("k1"),  LensFrame.K1);
			SubjectObject->TryGetNumberField(TEXT("k2"),  LensFrame.K2);
			SubjectObject->TryGetNumberField(TEXT("k3"),  LensFrame.K3);
			SubjectObject->TryGetNumberField(TEXT("p1"),  LensFrame.P1);
			SubjectObject->TryGetNumberField(TEXT("p2"),  LensFrame.P2);
			SubjectObject->TryGetNumberField(TEXT("cx"),  LensFrame.Cx);
			SubjectObject->TryGetNumberField(TEXT("cy"),  LensFrame.Cy);
			SubjectObject->TryGetNumberField(TEXT("fl"),  LensFrame.FocalLengthMM);
			SubjectObject->TryGetNumberField(TEXT("fov"), LensFrame.Fov);
			SubjectObject->TryGetNumberField(TEXT("fd"),  LensFrame.FocusDistance);
			SubjectObject->TryGetNumberField(TEXT("ap"),  LensFrame.Aperture);

			InClient->PushSubjectFrameData_AnyThread({ InSourceGuid, SubjectName }, MoveTemp(FrameDataStruct));
		}
	}
}

#undef LOCTEXT_NAMESPACE
