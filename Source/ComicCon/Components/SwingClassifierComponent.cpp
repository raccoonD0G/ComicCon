// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/SwingClassifierComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Utils.h"

void USwingClassifierComponent::Detect(EMainHand WhichHand)
{
	Super::Detect(WhichHand);

    if (WindowBuffer.Num() == 0) return;

    const uint64 Now = NowMsFromReceiver();
    if (Now == 0) return;

    // --- 쿨다운 ---
    if (LastSwingLoggedMs != 0) {
        const uint64 CoolMs = (uint64)(SwingCooldownSeconds * 1000.0);
        const uint64 Elapsed = Now - LastSwingLoggedMs;
        if (Elapsed < CoolMs) return;
    }

    // --- 최근 구간만 ---
    const double UseSec = FMath::Clamp((double)SwingRecentSeconds, 0.2, (double)WindowSeconds);
    const uint64 RecentOldest = (Now > (uint64)(UseSec * 1000.0)) ? (Now - (uint64)(UseSec * 1000.0)) : 0;

    TArray<const FTimedPoseSnapshot*> Snaps;
    Snaps.Reserve(WindowBuffer.Num());
    for (const auto& S : WindowBuffer) {
        if (S.TimestampMs >= RecentOldest && S.Poses.Num() > 0)
            Snaps.Add(&S);
    }
    if (Snaps.Num() < 3) return;

    // --- 샘플 추출 ---
    TArray<FSample> Samples;
    Samples.Reserve(Snaps.Num());

    for (const FTimedPoseSnapshot* S : Snaps) {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(S->Poses, idx)) continue;
        const FPersonPose& P = S->Poses[idx];
        if (P.XY.Num() < 17) continue;

        float ShoulderW = 0, WristDist = 0;
        const bool bClose = IsHandsClose(P, ShoulderW, WristDist);

        const FVector2f& LWR = P.XY[9];
        const FVector2f& RWR = P.XY[10];
        if (!IsFinite2D(LWR) || !IsFinite2D(RWR) || ShoulderW <= KINDA_SMALL_NUMBER) continue;

        const FVector2f Mid((LWR.X + RWR.X) * 0.5f, (LWR.Y + RWR.Y) * 0.5f);

        FSample smp;
        smp.T = (double)S->TimestampMs * 0.001;
        smp.C = FVector2D(Mid.X, Mid.Y);
        smp.ShoulderW = ShoulderW;
        smp.Close = bClose;
        Samples.Add(MoveTemp(smp));
    }
    if (Samples.Num() < 3) return;

    // --- 고정 Hz 리샘플 ---
    const double FixedHz = 30.0;
    TArray<FSample> U;
    ResampleToFixedRate(Samples, FixedHz, U);
    if (U.Num() < 3) return;

    // --- EMA 스무딩 ---
    SmoothEMA(U, 0.35);

    // --- 커버리지 (손 가까움) ---
    int32 CloseCount = 0;
    for (const auto& S : U) if (S.Close) ++CloseCount;
    const double Coverage = (double)CloseCount / FMath::Max(1, U.Num());
    if (Coverage < (double)MinCloseCoverage) return;

    // --- 평균 어깨폭 ---
    double MeanSW = 0.0;
    for (const auto& S : U) MeanSW += (double)FMath::Max(S.ShoulderW, KINDA_SMALL_NUMBER);
    MeanSW /= (double)U.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;


    // --- 속도/거리/방향전환 계산 ---
    double downPathNorm = 0.0;
    double maxDownSpeedNorm = 0.0;
    int32  vertReversals = 0;
    double downCoverage = 0.0;

    int lastVertSign = 0;
    double vertHold = 0.0;

    for (int i = 1; i < U.Num(); ++i) {
        const FVector2D d = U[i].C - U[i - 1].C;
        const double vy_norm = (d.Y / (1.0 / FixedHz)) / MeanSW; // ↓속도

        if (vy_norm > 0.0) {
            downPathNorm += (d.Y / MeanSW);
            if (vy_norm > maxDownSpeedNorm) maxDownSpeedNorm = vy_norm;
        }

        if (vy_norm > DownVyThresholdNorm) downCoverage += 1.0;

        const int sgn = (vy_norm > 0.20) ? +1 :
            (vy_norm < -0.20) ? -1 : 0;
        if (sgn == 0) {
            vertHold = 0.0;
        }
        else {
            if (lastVertSign == 0 || sgn == lastVertSign) {
                vertHold += (1.0 / FixedHz);
                lastVertSign = sgn;
            }
            else {
                if (vertHold >= VertHoldSeconds) {
                    ++vertReversals;
                    lastVertSign = sgn;
                    vertHold = 0.0;
                }
            }
        }
    }

    const double totalTime = (U.Last().T - U[0].T);
    if (totalTime <= 0.0) return;

    downCoverage /= double(FMath::Max(1, U.Num()));
    const double avgDownSpeedNorm = downPathNorm / totalTime;
    const double downDeltaNorm = (U.Last().C.Y - U[0].C.Y) / MeanSW;

    // --- 임계 판정 ---
    const bool bPass =
        (Coverage >= (double)MinCloseCoverage) &&
        (downCoverage >= MinDownCoverage) &&
        (downDeltaNorm >= MinDownDeltaNorm) &&
        ((avgDownSpeedNorm >= MinDownAvgSpeedNorm) || (maxDownSpeedNorm >= MinDownPeakSpeedNorm)) &&
        (vertReversals <= MaxVertReversals);

    if (!bPass) return;

    TArray<FTimedPoseSnapshot> SnapsValue;
    SnapsValue.Reserve(Snaps.Num());
    for (const FTimedPoseSnapshot* S : Snaps)
    {
        if (S) { SnapsValue.Add(*S); }   // 값 복사
    }

    OnSwingDetected.Broadcast(SnapsValue);

    LastSwingLoggedMs = Now;
    WindowBuffer.Reset();
}