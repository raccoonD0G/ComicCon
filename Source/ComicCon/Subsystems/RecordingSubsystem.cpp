// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/RecordingSubsystem.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

void URecordingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 출력 폴더: <ProjectDir>/Saved/Captures
    if (OutputDir.IsEmpty())
    {
        OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Captures"));
    }
    OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);

#if PLATFORM_WINDOWS
    // ffmpeg 실행 파일: <ProjectDir>/ffmpeg/bin/ffmpeg.exe
    FfmpegExe = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectContentDir(), TEXT("ffmpeg/bin/ffmpeg.exe"))
    );
#endif

    // 폴더 보장
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*OutputDir))
    {
        PF.CreateDirectoryTree(*OutputDir);
    }
}

void URecordingSubsystem::Deinitialize()
{
    KillIfRunning();
}

void URecordingSubsystem::StartRecording()
{
    if (bRecording) return;

#if PLATFORM_WINDOWS
    // 출력 경로: <Project>/Saved/Captures/match_<ts>.mp4
    const FString Ts = FString::Printf(TEXT("%lld"), FDateTime::UtcNow().ToUnixTimestamp());
    OutputPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(OutputDir, FString::Printf(TEXT("match_%s.mp4"), *Ts))
    );

    // STDIN 파이프(자식이 읽는 쪽/부모가 쓰는 쪽)만 생성
    FPlatformProcess::CreatePipe(ChildStdIn_Read, ChildStdIn_Write, /*bWritePipeLocal=*/true);

    auto LaunchFF = [&](bool bUseGdi) -> bool
        {
            const TCHAR* Grab = bUseGdi ? TEXT("gdigrab") : TEXT("ddagrab");

            const FString Args = FString::Printf(
                TEXT("-y -f %s -i desktop -framerate %d ")
                TEXT("-vf pad=ceil(iw/2)*2:ceil(ih/2)*2 ")
                TEXT("-pix_fmt yuv420p -vcodec libx264 -preset veryfast -crf %d -movflags +faststart \"%s\""),
                Grab, Framerate, CRF, *OutputPath
            );

            const FString WorkDir = FPaths::GetPath(FfmpegExe);
            FfmpegHandle = FPlatformProcess::CreateProc(
                *FfmpegExe, *Args,
                /*Detached*/ true,
                /*Hidden*/   bHideWindow,
                /*ReallyHidden*/ false,
                /*PID*/      nullptr,
                /*Priority*/ 0,
                /*WorkDir*/  WorkDir.IsEmpty() ? nullptr : *WorkDir,
                /*PipeWrite (child stdout/err)*/ nullptr,
                /*PipeRead  (child stdin)     */ ChildStdIn_Read
            );

            // 살짝 대기 후 살아있는지 확인
            FPlatformProcess::Sleep(0.1f);
            return FPlatformProcess::IsProcRunning(FfmpegHandle);
        };

    // ddagrab 우선 → 실패시 gdigrab
    bool bOK = LaunchFF(false);
    if (!bOK) bOK = LaunchFF(true);

    bRecording = bOK;
    StartTimeSec = bOK ? FPlatformTime::Seconds() : 0.0;

    if (!bRecording)
    {
        // 시작 실패 시 파이프 정리
        if (ChildStdIn_Read || ChildStdIn_Write)
            FPlatformProcess::ClosePipe(ChildStdIn_Read, ChildStdIn_Write);
        ChildStdIn_Read = ChildStdIn_Write = nullptr;
    }
#endif
}

void URecordingSubsystem::StopRecording()
{
    KillIfRunning();
}

void URecordingSubsystem::KillIfRunning()
{
#if PLATFORM_WINDOWS
    // 너무 빨리 멈추면 0프레임 → 최소 0.5s 보장
    if (bRecording)
    {
        const double MinDur = 0.5;
        const double Now = FPlatformTime::Seconds();
        if (StartTimeSec > 0 && Now - StartTimeSec < MinDur)
            FPlatformProcess::Sleep(static_cast<float>(MinDur - (Now - StartTimeSec)));
    }

    if (FPlatformProcess::IsProcRunning(FfmpegHandle))
    {
        // 그레이스풀 종료: stdin으로 'q\n'
        if (ChildStdIn_Write)
        {
            FPlatformProcess::WritePipe(ChildStdIn_Write, TEXT("q\n"));
            // 원하면 즉시 EOF 신호: 파이프 닫기
            // FPlatformProcess::ClosePipe(ChildStdIn_Read, ChildStdIn_Write);
            // ChildStdIn_Read = ChildStdIn_Write = nullptr;
        }

        // 최대 10초 대기
        const double Deadline = FPlatformTime::Seconds() + 10.0;
        while (FPlatformTime::Seconds() < Deadline && FPlatformProcess::IsProcRunning(FfmpegHandle))
            FPlatformProcess::Sleep(0.1f);

        // 그래도 살아있으면 강제 종료
        if (FPlatformProcess::IsProcRunning(FfmpegHandle))
            FPlatformProcess::TerminateProc(FfmpegHandle, true);

        FPlatformProcess::CloseProc(FfmpegHandle);
    }

    // 남은 파이프 정리
    if (ChildStdIn_Read || ChildStdIn_Write)
        FPlatformProcess::ClosePipe(ChildStdIn_Read, ChildStdIn_Write);
    ChildStdIn_Read = ChildStdIn_Write = nullptr;

    bRecording = false;
#endif
}