// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "IMessageContext.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

//enable logging step 1
DECLARE_LOG_CATEGORY_EXTERN(ModuleLog, Log, All)

class FRunnableThread;
class FSocket;
class ILiveLinkClient;
class ISocketSubsystem;

class FIOBuilderLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:

	FIOBuilderLiveLinkSource(FIPv4Endpoint Endpoint);

	virtual ~FIOBuilderLiveLinkSource();

	// Begin ILiveLinkSource Interface

	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override { return SourceType; };
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override { return SourceStatus; }

	// End ILiveLinkSource Interface

	// Begin FRunnable Interface

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	void Start();
	virtual void Stop() override;
	virtual void Exit() override { }

	// End FRunnable Interface

private:

	// Parses a JSON datagram and pushes static/frame data for each subject.
	// Runs on the receiver thread - LiveLink push APIs are AnyThread-safe.
	void HandleReceivedData(ILiveLinkClient* InClient, const FGuid& InSourceGuid, const TArray<uint8>& ReceivedData);

	// Protects Client / SourceGuid which are written on the game thread
	// (ReceiveClient) and read on the receiver thread (Run).
	mutable FCriticalSection ClientCS;
	ILiveLinkClient* Client;

	// Our identifier in LiveLink
	FGuid SourceGuid;

	FMessageAddress ConnectionAddress;

	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;

	FIPv4Endpoint DeviceEndpoint;

	// Socket to receive data on
	FSocket* Socket;

	// Subsystem associated to Socket
	ISocketSubsystem* SocketSubsystem;

	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool Stopping;

	// Thread to run socket operations on
	FRunnableThread* Thread;

	// Name of the sockets thread
	FString ThreadName;

	// Time to wait between attempted receives
	FTimespan WaitTime;

	// List of subjects we've already encountered (receiver thread only)
	TSet<FName> EncounteredSubjects;

	// Buffer to receive socket data into
	TArray<uint8> RecvBuffer;
};
