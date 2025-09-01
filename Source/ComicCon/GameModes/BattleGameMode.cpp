// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/BattleGameMode.h"
#include "Subsystems/RecordingSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameStates/BattleGameState.h"
#include "HUDs/ResultHUD.h"
#include "Utils.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

void ABattleGameMode::BeginPlay()
{
    Super::BeginPlay();
    StartMatch();
	Init();
}

void ABattleGameMode::Init()
{
    if (ABattleGameState* BattleGameState = GetGameState<ABattleGameState>())
    {
		BattleGameState->OnCountdownFinished.AddDynamic(this, &ABattleGameMode::EndMatch);
    }
    else
    {
        GetWorldTimerManager().SetTimer(InitRetryTimer, this, &ABattleGameMode::StartMatch, 0.1f, false);
    }
}

void ABattleGameMode::StartMatch()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (auto* Rec = GI->GetSubsystem<URecordingSubsystem>())
        {
            Rec->StartRecording();
        }
    }
}

void ABattleGameMode::EndMatch()
{
    FString VideoPath;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (auto* Rec = GI->GetSubsystem<URecordingSubsystem>())
        {
            Rec->StopRecording();
            VideoPath = Rec->GetLastOutputPath();

            if (!VideoPath.IsEmpty())
            {
                TWeakObjectPtr<UGameInstance> WeakGI = GI;
                TWeakObjectPtr<URecordingSubsystem> WeakRec = Rec;

                // 권장: 서브시스템의 함수를 사용 (수명 안전)
                UploadFileToServer(
                    VideoPath,
                    TEXT("http://ec2-54-180-121-4.ap-northeast-2.compute.amazonaws.com:3000"),
                    [WeakGI, WeakRec](bool bOk, FString QrUrl)
                    {
                        if (!bOk || QrUrl.IsEmpty() || !WeakRec.IsValid()) return;

                        FetchQrTexture(QrUrl, [WeakGI](bool bOk2, UTexture2D* QrTex)
                            {
                                if (!bOk2 || !QrTex || !WeakGI.IsValid()) return;

                                // 게임 스레드에서 UI 접근
                                AsyncTask(ENamedThreads::GameThread, [WeakGI, QrTex]()
                                    {
                                        if (!WeakGI.IsValid()) return;
                                        if (UWorld* World = WeakGI->GetWorld())
                                        {
                                            if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
                                            {
                                                if (AResultHUD* ResultHUD = PC->GetHUD<AResultHUD>())
                                                {
                                                    ResultHUD->SetQrTexture(QrTex);
                                                }
                                            }
                                        }
                                    });
                            });
                    }
                );
            }
        }
    }
}
