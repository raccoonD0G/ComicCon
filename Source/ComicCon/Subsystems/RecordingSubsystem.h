// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RecordingSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API URecordingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ����ý��� ����������Ŭ
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Recording")
    void StartRecording();

    UFUNCTION(BlueprintCallable, Category = "Recording")
    void StopRecording();

    UFUNCTION(BlueprintPure, Category = "Recording")
    const FString& GetLastOutputPath() const { return OutputPath; }

private:
    UFUNCTION(BlueprintPure, Category = "Recording")
    bool IsRecording() const { return bRecording; }

    // ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (AllowPrivateAccess = "true"))
    FString FfmpegExe = TEXT("ffmpeg/bin/ffmpeg.exe");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (AllowPrivateAccess = "true"))
    FString OutputDir;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (AllowPrivateAccess = "true"))
    int32 Framerate = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (AllowPrivateAccess = "true"))
    int32 CRF = 23;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (AllowPrivateAccess = "true"))
    bool bHideWindow = true;

private:
    bool bRecording = false;

    FString OutputPath;
    FProcHandle FfmpegHandle;

    void* ChildStdOut_Read = nullptr;  // �ڽ� STDOUT/ERR �б� (�θ� ReadPipe)
    void* ChildStdOut_Write = nullptr; // �ڽ� STDOUT/ERR ���� (CreateProc�� ����)
    void* ChildStdIn_Read = nullptr;  // �ڽ� STDIN �б� (CreateProc�� ����)
    void* ChildStdIn_Write = nullptr;  // �ڽ� STDIN ���� (�θ� WritePipe)

    double StartTimeSec = 0.0;
    uint8 bUsedGdiGrab : 1 = false;

    void KillIfRunning();
};
