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
    // 서브시스템 라이프사이클
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

    // 설정
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

    void* ChildStdOut_Read = nullptr;  // 자식 STDOUT/ERR 읽기 (부모가 ReadPipe)
    void* ChildStdOut_Write = nullptr; // 자식 STDOUT/ERR 쓰기 (CreateProc에 전달)
    void* ChildStdIn_Read = nullptr;  // 자식 STDIN 읽기 (CreateProc에 전달)
    void* ChildStdIn_Write = nullptr;  // 자식 STDIN 쓰기 (부모가 WritePipe)

    double StartTimeSec = 0.0;
    uint8 bUsedGdiGrab : 1 = false;

    void KillIfRunning();
};
