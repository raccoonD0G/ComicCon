// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/SingleSwingClassifierComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Utils.h"

void USingleSwingClassifierComponent::Detect(EMainHand WhichHand)
{
	Super::Detect(WhichHand);

    if (WindowBuffer.Num() == 0) return;

    const uint64 SenderNow = NowMsFromReceiver();    // 윈도우 슬라이싱용
    if (SenderNow == 0) return;

    const uint64 LocalNow = LocalNowMs();            // 쿨다운 비교용(로컬 시계)

    // 쿨다운
    if (LastSingleSwingLoggedMs != 0)
    {
        const uint64 CoolMs = (uint64)(SingleSwingCooldownSeconds * 1000.0);
        const uint64 Delta = (LocalNow >= LastSingleSwingLoggedMs) ? (LocalNow - LastSingleSwingLoggedMs) : 0;
        if (Delta < CoolMs) return;
    }

    // 스냅샷 수집(최근 구간)
    const double UseSec = FMath::Clamp((double)SingleSwingRecentSeconds, 0.2, (double)WindowSeconds);
    const uint64 RecentOldest = (SenderNow > (uint64)(UseSec * 1000.0)) ? (SenderNow - (uint64)(UseSec * 1000.0)) : 0;

    TArray<const FTimedPoseSnapshot*> Snaps;
    Snaps.Reserve(WindowBuffer.Num());
    for (const auto& S : WindowBuffer)
        if (S.TimestampMs >= RecentOldest && S.Poses.Num() > 0) Snaps.Add(&S);
    if (Snaps.Num() < 3) return;

    auto IsFinite2D_Local = [](const FVector2f& p)->bool { return FMath::IsFinite(p.X) && FMath::IsFinite(p.Y); };

    auto FindHandFor = [&](const TArray<FHandPose>& Hands, uint16 PersonId, uint8 Which, const FVector2f& FallbackNearTo, FVector2D& OutCenter)->bool
        {
            int bestIdx = INDEX_NONE;

            // 1) pid & which 정확히 일치
            for (int i = 0; i < Hands.Num(); ++i)
            {
                const FHandPose& H = Hands[i];
                if (H.PersonId == PersonId && H.Which == Which && IsFinite2D_Local(H.Center)) { bestIdx = i; break; }
            }
            // 2) pid 일치 & which unknown(2): 손목 근처
            if (bestIdx == INDEX_NONE)
            {
                double bestD = DBL_MAX;
                for (int i = 0; i < Hands.Num(); ++i)
                {
                    const FHandPose& H = Hands[i];
                    if (H.PersonId == PersonId && IsFinite2D_Local(H.Center))
                    {
                        const double d = FVector2D::Distance((FVector2D)H.Center, (FVector2D)FallbackNearTo);
                        if (d < bestD) { bestD = d; bestIdx = i; }
                    }
                }
            }
            // 3) pid 불명: 전체 중 손목 근처
            if (bestIdx == INDEX_NONE)
            {
                double bestD = DBL_MAX;
                for (int i = 0; i < Hands.Num(); ++i)
                {
                    const FHandPose& H = Hands[i];
                    if (!IsFinite2D_Local(H.Center)) continue;
                    const double d = FVector2D::Distance((FVector2D)H.Center, (FVector2D)FallbackNearTo);
                    if (d < bestD) { bestD = d; bestIdx = i; }
                }
            }
            if (bestIdx != INDEX_NONE) { OutCenter = (FVector2D)Hands[bestIdx].Center; return true; }
            return false;
        };

    struct FHandSample
    {
        double    T = 0.0;
        FVector2D L = FVector2D::ZeroVector;
        FVector2D R = FVector2D::ZeroVector;
        FVector2D C = FVector2D::ZeroVector;
        float     ShoulderW = 0.f;
        bool      Open = false;
    };

    // 샘플 추출(손 감지 우선)
    TArray<FHandSample> Samples; Samples.Reserve(Snaps.Num());
    for (const FTimedPoseSnapshot* S : Snaps)
    {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(S->Poses, idx)) continue;
        const FPersonPose& P = S->Poses[idx];
        if (P.XY.Num() < 17) continue;

        if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH))) continue;
        const FVector2f& LSH = P.XY[COCO_LSH];
        const FVector2f& RSH = P.XY[COCO_RSH];
        if (!IsFinite2D_Local(LSH) || !IsFinite2D_Local(RSH)) continue;
        const FVector2D C = 0.5 * FVector2D(LSH.X + RSH.X, LSH.Y + RSH.Y);

        const FVector2f& LWR = P.XY[COCO_LWR];
        const FVector2f& RWR = P.XY[COCO_RWR];
        if (!IsFinite2D_Local(LWR) || !IsFinite2D_Local(RWR)) continue;

        float ShoulderW = 0.f, WristDist = 0.f;
        const bool bCloseLegacy = IsHandsClose(P, ShoulderW, WristDist);
        if (ShoulderW <= KINDA_SMALL_NUMBER) continue;

        FVector2D LCenter = FVector2D(LWR.X, LWR.Y);
        FVector2D RCenter = FVector2D(RWR.X, RWR.Y);

        if (S->Hands.Num() > 0)
        {
            const uint16 PersonId = (uint16)P.PersonId;
            FVector2D candL; if (FindHandFor(S->Hands, PersonId, 0, LWR, candL)) LCenter = candL;
            FVector2D candR; if (FindHandFor(S->Hands, PersonId, 1, RWR, candR)) RCenter = candR;
        }

        const float HandCenterDist = FVector2D::Distance(LCenter, RCenter);
        const bool  bCloseNow = (ShoulderW > 0.f) ? ((HandCenterDist / ShoulderW) <= HandsCloseRatio) : true;

        FHandSample smp;
        smp.T = (double)S->TimestampMs * 0.001;
        smp.L = LCenter;
        smp.R = RCenter;
        smp.C = C;
        smp.ShoulderW = ShoulderW;
        smp.Open = !bCloseNow;
        Samples.Add(MoveTemp(smp));
    }
    if (Samples.Num() < 3) return;

    // 리샘플 + EMA
    const double FixedHz = 30.0;
    const double FixedDt = 1.0 / FixedHz;

    TArray<FHandSample> U;
    {
        if (Samples.Num() < 2) return;
        const double t0 = Samples[0].T, t1 = Samples.Last().T;
        const int N = FMath::Max(2, (int)FMath::RoundToInt((t1 - t0) * FixedHz) + 1);
        U.Reset(); U.Reserve(N);
        int j = 0;
        for (int i = 0; i < N; ++i)
        {
            const double t = t0 + i * FixedDt;
            while (j + 1 < Samples.Num() && Samples[j + 1].T < t) ++j;
            const int k = FMath::Min(j + 1, Samples.Num() - 1);
            const double tA = Samples[j].T, tB = Samples[k].T;
            const double a = (tB > tA) ? FMath::Clamp((t - tA) / (tB - tA), 0.0, 1.0) : 0.0;

            FHandSample o;
            o.T = t;
            o.L = FMath::Lerp(Samples[j].L, Samples[k].L, a);
            o.R = FMath::Lerp(Samples[j].R, Samples[k].R, a);
            o.C = FMath::Lerp(Samples[j].C, Samples[k].C, a);
            o.ShoulderW = FMath::Lerp(Samples[j].ShoulderW, Samples[k].ShoulderW, a);
            o.Open = (a < 0.5) ? Samples[j].Open : Samples[k].Open;
            U.Add(o);
        }
    }
    if (U.Num() < 3) return;

    {
        FVector2D l = U[0].L, r = U[0].R, c = U[0].C;
        const double Alpha = 0.35;
        for (int i = 0; i < U.Num(); ++i)
        {
            l = (1 - Alpha) * l + Alpha * U[i].L;
            r = (1 - Alpha) * r + Alpha * U[i].R;
            c = (1 - Alpha) * c + Alpha * U[i].C;
            U[i].L = l; U[i].R = r; U[i].C = c;
        }
    }

    // Open coverage
    int32 OpenCount = 0; for (auto& s : U) if (s.Open) ++OpenCount;
    const double OpenCoverage = (double)OpenCount / FMath::Max(1, U.Num());
    if (OpenCoverage < MinOpenCoverage) return;

    // 어깨 폭 평균
    double MeanSW = 0.0; for (auto& s : U) MeanSW += (double)FMath::Max(s.ShoulderW, KINDA_SMALL_NUMBER);
    MeanSW /= (double)U.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    // 속도 메트릭
    struct FHandMetrics { double Peak = 0.0; double Avg = 0.0; };
    auto Metrics = [&](bool bLeft)->FHandMetrics
        {
            FHandMetrics M{};
            double used = 0.0, path = 0.0;
            for (int i = 1; i < U.Num(); ++i)
            {
                if (!U[i].Open) continue;
                const FVector2D p0 = bLeft ? U[i - 1].L : U[i - 1].R;
                const FVector2D p1 = bLeft ? U[i].L : U[i].R;
                const double v = (p1 - p0).Size() / FixedDt / MeanSW;
                if (v > M.Peak) M.Peak = v;
                path += (p1 - p0).Size() / MeanSW;
                used += FixedDt;
            }
            if (used > 0.0) M.Avg = path / used;
            return M;
        };

    const FHandMetrics ML = Metrics(true);
    const FHandMetrics MR = Metrics(false);

    // 최종 손 선택
    bool bRightHandFinal = false;
    switch (WhichHand)
    {
    case EMainHand::Left:  bRightHandFinal = false; break;
    case EMainHand::Right: bRightHandFinal = true;  break;
    default:
        bRightHandFinal = !((ML.Peak > MR.Peak) || (FMath::IsNearlyEqual(ML.Peak, MR.Peak, 1e-3) && ML.Avg >= MR.Avg));
        break;
    }

    // 최소 이동 거리(정규화)
    const FVector2D FirstPos = bRightHandFinal ? U[0].R : U[0].L;
    const FVector2D LastPos = bRightHandFinal ? U.Last().R : U.Last().L;
    const double Displacement = (LastPos - FirstPos).Size() / MeanSW;
    if (Displacement < MinDisplacementNorm) return;

    // 속도 통과 조건
    const FHandMetrics& M = bRightHandFinal ? MR : ML;
    const bool bPass = (OpenCoverage >= MinOpenCoverage) &&
        ((M.Avg >= MinAvgSpeedNorm) || (M.Peak >= MinPeakSpeedNorm));
    if (!bPass) return;

    // 대표 PersonIdx 고정
    TArray<int32> PersonIdxOf; PersonIdxOf.SetNum(Snaps.Num());
    for (int32 i = 0; i < Snaps.Num(); ++i) { int32 idx = INDEX_NONE; PickBestPerson(Snaps[i]->Poses, idx); PersonIdxOf[i] = idx; }

    // 스윙 구간 탐색(속도 기반)
    TArray<double> SpeedNorm; SpeedNorm.Init(0.0, U.Num());
    auto WristAt = [&](int i) { return bRightHandFinal ? U[i].R : U[i].L; };
    for (int i = 1; i < U.Num(); ++i)
    {
        const FVector2D d = WristAt(i) - WristAt(i - 1);
        SpeedNorm[i] = (d.Size() / FixedDt) / MeanSW;
    }

    auto FindRun = [&](int s, double thr, double hold, bool above, int& out)->bool
        {
            double acc = 0;
            for (int i = s; i < U.Num(); ++i)
            {
                const bool pass = above ? (SpeedNorm[i] >= thr) : (SpeedNorm[i] <= thr);
                const double dt = (i > 0) ? FixedDt : 0.0;
                if (pass) { acc += dt; if (acc >= hold) { out = i; return true; } }
                else acc = 0;
            }
            return false;
        };

    int iEnter = -1; if (!FindRun(1, EnterSpeed, HoldFast, true, iEnter)) iEnter = 0;
    int iExit = -1; if (!FindRun(FMath::Max(1, iEnter + 1), ExitSpeed, HoldStill, false, iExit)) iExit = U.Num() - 1;
    const int iEnterAfter = FMath::Clamp(iEnter + 1, 0, U.Num() - 1);
    const int iExitAfter = FMath::Clamp(iExit + 1, 0, U.Num() - 1);

    {
        const FVector2D WEnter = WristAt(iEnterAfter);
        const FVector2D WExit = WristAt(iExitAfter);
        const FVector2D CEnter = U[iEnterAfter].C;
        const FVector2D CExit = U[iExitAfter].C;

        // (1) 몸 중심으로부터의 반경이 증가해야 함
        const double rEnter = (WEnter - CEnter).Size() / MeanSW;
        const double rExit = (WExit - CExit).Size() / MeanSW;

        // (2) 이동 방향이 '바깥쪽' 벡터와 일치해야 함 (dot > 0)
        FVector2D outwardAvg = ((WEnter - CEnter) + (WExit - CExit)) * 0.5;
        const FVector2D move = (WExit - WEnter);

        double outwardDot = 0.0;
        if (!outwardAvg.IsNearlyZero())
            outwardDot = FVector2D::DotProduct(move, outwardAvg.GetSafeNormal());
        else if (!(WExit - CExit).IsNearlyZero())
            outwardDot = FVector2D::DotProduct(move, (WExit - CExit).GetSafeNormal());
        else
            outwardDot = 0.0;

        // 임계값: 반경 증가가 충분히 양수이고, 바깥쪽 성분이 있어야 함
        const double OutwardDeltaMinNorm = 0.05; // 어깨폭의 5% 이상 바깥으로
        const bool bRadiallyOut = (rExit >= rEnter + OutwardDeltaMinNorm);
        const bool bDirOut = (outwardDot > 0.0);

        if (!(bRadiallyOut && bDirOut))
        {
            return; // 바깥으로 나가는 스윙이 아니면 무시
        }
    }

    // U 인덱스를 Snaps 인덱스로 매핑
    if (Snaps.Num() == 0) return;

    TArray<double> SnapTimes; SnapTimes.Reserve(Snaps.Num());
    for (int s = 0; s < Snaps.Num(); ++s) SnapTimes.Add((double)Snaps[s]->TimestampMs * 0.001);

    auto MapUToSnapIndex = [&](int Ui, int& Cursor)->int
        {
            const double t = U[Ui].T;
            while (Cursor + 1 < SnapTimes.Num() && SnapTimes[Cursor + 1] <= t) ++Cursor;
            int a = Cursor;
            int b = FMath::Min(Cursor + 1, SnapTimes.Num() - 1);
            if (a < 0) a = 0; if (b < 0) b = 0;
            const double da = FMath::Abs(SnapTimes[a] - t);
            const double db = FMath::Abs(SnapTimes[b] - t);
            return (db < da) ? b : a;
        };

    int cur = 0;
    int EnterSnapIdx = MapUToSnapIndex(iEnterAfter, cur);
    int ExitSnapIdx = MapUToSnapIndex(iExitAfter, cur);
    EnterSnapIdx = FMath::Clamp(EnterSnapIdx, 0, Snaps.Num() - 1);
    ExitSnapIdx = FMath::Clamp(ExitSnapIdx, 0, Snaps.Num() - 1);
    if (ExitSnapIdx < EnterSnapIdx) ExitSnapIdx = EnterSnapIdx;

    TArray<FTimedPoseSnapshot> SnapsValue;
    SnapsValue.Reserve(Snaps.Num());
    for (const FTimedPoseSnapshot* S : Snaps)
    {
        if (S) { SnapsValue.Add(*S); }   // 값 복사
    }

    OnSingleSwingDetected.Broadcast(SnapsValue, PersonIdxOf, bRightHandFinal, EnterSnapIdx, ExitSnapIdx);

    LastSingleSwingLoggedMs = LocalNow;  // 로컬 시계로 쿨다운 시작
    WindowBuffer.Reset();
}
