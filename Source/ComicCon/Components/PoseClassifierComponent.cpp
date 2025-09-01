// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PoseClassifierComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Utils.h"
#include "GameFramework/ProjectileMovementComponent.h"

struct FSample { double T; FVector2D C; float ShoulderW; bool Close; };

static void ResampleToFixedRate(const TArray<FSample>& In, double Hz, TArray<FSample>& Out)
{
    Out.Reset();
    if (In.Num() < 2) return;

    const double dt = 1.0 / Hz;
    const double t0 = In[0].T;
    const double t1 = In.Last().T;

    int j = 1;
    for (double t = t0; t <= t1 + 1e-9; t += dt)
    {
        while (j < In.Num() && In[j].T < t) ++j;
        if (j >= In.Num()) break;

        const FSample& A = In[j - 1];
        const FSample& B = In[j];
        const double denom = FMath::Max(B.T - A.T, 1e-6);
        const double a = FMath::Clamp((t - A.T) / denom, 0.0, 1.0);

        FSample S;
        S.T = t;
        S.C = A.C * (1.0 - a) + B.C * a;
        S.ShoulderW = (float)FMath::Lerp((double)A.ShoulderW, (double)B.ShoulderW, a);
        // 보수적: 둘 다 가까워야 true
        S.Close = (A.Close && B.Close);
        Out.Add(S);
    }
}

static void SmoothEMA(TArray<FSample>& A, double Alpha /*0~1*/)
{
    if (A.Num() < 2) return;
    Alpha = FMath::Clamp(Alpha, 0.0, 1.0);
    for (int i = 1; i < A.Num(); ++i)
    {
        A[i].C = (1.0 - Alpha) * A[i - 1].C + Alpha * A[i].C;
        A[i].ShoulderW = (float)FMath::Lerp((double)A[i - 1].ShoulderW, (double)A[i].ShoulderW, Alpha);
        // Close는 보수적으로 유지
        A[i].Close = (A[i].Close && A[i - 1].Close);
    }
}

UPoseClassifierComponent::UPoseClassifierComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPoseClassifierComponent::BeginPlay()
{
    Super::BeginPlay();

    // Receiver 자동 탐색(같은 Actor에 있으면)
    if (!Receiver)
    {
        if (AActor* Owner = GetOwner())
        {
            Receiver = Owner->FindComponentByClass<UPoseUdpReceiverComponent>();
        }
    }
}

void UPoseClassifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    WindowBuffer.Reset();
    Super::EndPlay(EndPlayReason);
}

void UPoseClassifierComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    IngestLatestFromReceiver();

    // 리시버 타임스탬프 기준으로 오래된 스냅샷 제거
    PruneOld(NowMsFromReceiver());

    DetectSwing();
    DetectArrow();
    DetectSingleSwing(EMainHand::Right);
}

void UPoseClassifierComponent::IngestLatestFromReceiver()
{
    if (!Receiver) return;

    const uint64 Ts = Receiver->GetLatestTimestamp();
    if (Ts == 0 || Ts == LastIngestedTs) return; // 중복 회피

    const TArray<FPersonPose>& Latest = Receiver->GetLatestPoses();

    FTimedPoseSnapshot Snap;
    Snap.TimestampMs = Ts;
    Snap.Poses = Latest; // 얕은 복사(내부 TArray는 값 복사됨)

    WindowBuffer.Add(MoveTemp(Snap));
    LastIngestedTs = Ts;

    OnPoseWindowUpdated.Broadcast();
}

void UPoseClassifierComponent::PruneOld(uint64 NowMs)
{
    if (WindowSeconds <= 0.f) { WindowBuffer.Reset(); return; }

    const uint64 SpanMs = static_cast<uint64>(WindowSeconds * 1000.0f);
    const uint64 Cutoff = (NowMs > SpanMs) ? (NowMs - SpanMs) : 0;

    int32 FirstValidIdx = 0;
    while (FirstValidIdx < WindowBuffer.Num() && WindowBuffer[FirstValidIdx].TimestampMs < Cutoff)
    {
        ++FirstValidIdx;
    }
    if (FirstValidIdx > 0)
    {
        WindowBuffer.RemoveAt(0, FirstValidIdx, /*bAllowShrinking*/ false);
    }
}

uint64 UPoseClassifierComponent::NowMsFromReceiver() const
{
    // 수신 타임스탬프(송신 기준 ms)를 시간 기준으로 사용
    if (Receiver)
    {
        return Receiver->GetLatestTimestamp();
    }
    return 0;
}

int32 UPoseClassifierComponent::GetWindowTotalPersons() const
{
    int32 Sum = 0;
    for (const auto& Snap : WindowBuffer)
    {
        Sum += Snap.Poses.Num();
    }
    return Sum;
}

bool UPoseClassifierComponent::GetLatestJoint(int32 CocoIndex, FVector2D& OutXY) const
{
    if (WindowBuffer.Num() == 0) return false;

    const FTimedPoseSnapshot& Last = WindowBuffer.Last();
    if (Last.Poses.Num() == 0) return false;

    const FPersonPose& P = Last.Poses[0]; // 최신 스냅샷의 첫 사람
    if (!P.XY.IsValidIndex(CocoIndex)) return false;

    const FVector2f& V = P.XY[CocoIndex];
    if (!FMath::IsFinite(V.X) || !FMath::IsFinite(V.Y)) return false;

    OutXY = FVector2D(V.X, V.Y);
    return true;
}

int32 UPoseClassifierComponent::GetDominantPersonId() const
{
    TMap<int32, int32> Count;
    for (const auto& Snap : WindowBuffer)
    {
        for (const auto& PP : Snap.Poses)
        {
            Count.FindOrAdd((int32)PP.PersonId)++;
        }
    }
    int32 BestId = -1;
    int32 BestCnt = -1;
    for (const auto& It : Count)
    {
        if (It.Value > BestCnt)
        {
            BestCnt = It.Value;
            BestId = It.Key;
        }
    }
    return BestId;
}

bool UPoseClassifierComponent::PickBestPerson(const TArray<FPersonPose>& Poses, int32& OutIdx) const
{
    // "가장 손이 가까운 사람"을 선택 (프레임마다 주 대상 선택)
    float BestDist = TNumericLimits<float>::Max();
    int32 Best = INDEX_NONE;
    for (int32 i = 0; i < Poses.Num(); ++i)
    {
        const auto& P = Poses[i];
        if (P.XY.Num() < 17) continue;

        const FVector2f& LWR = P.XY[9];
        const FVector2f& RWR = P.XY[10];
        const FVector2f& LSH = P.XY[5];
        const FVector2f& RSH = P.XY[6];

        if (!IsFinite2D(LWR) || !IsFinite2D(RWR) || !IsFinite2D(LSH) || !IsFinite2D(RSH)) continue;

        const float ShoulderW = FMath::Abs(RSH.X - LSH.X);
        if (ShoulderW <= KINDA_SMALL_NUMBER) continue;

        const float Dist = FVector2f::Distance(LWR, RWR);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Best = i;
        }
    }
    OutIdx = Best;
    return Best != INDEX_NONE;
}

bool UPoseClassifierComponent::IsHandsClose(const FPersonPose& P, float& OutShoulderW, float& OutWristDist) const
{
    const FVector2f& LWR = P.XY[9];
    const FVector2f& RWR = P.XY[10];
    const FVector2f& LSH = P.XY[5];
    const FVector2f& RSH = P.XY[6];

    if (!IsFinite2D(LWR) || !IsFinite2D(RWR) || !IsFinite2D(LSH) || !IsFinite2D(RSH))
        return false;

    const float ShoulderW = FMath::Abs(RSH.X - LSH.X);
    if (ShoulderW <= KINDA_SMALL_NUMBER)
        return false;

    const float WristDist = FVector2f::Distance(LWR, RWR);

    OutShoulderW = ShoulderW;
    OutWristDist = WristDist;
    return (WristDist <= HandsCloseRatio * ShoulderW);
}

FVector UPoseClassifierComponent::Map2DToLocal(const FVector2f& P, const FVector2f& Pelvis2D) const
{
    const FVector2f Rel = P - Pelvis2D;          // 골반 기준 상대(픽셀)
    const float Y = Rel.X * PixelToUU;          // 화면 X → 로컬 Y
    const float Z = -Rel.Y * PixelToUU;          // 화면 Y(아래+) → 로컬 Z(위+) 부호 반전
    return FVector(DepthOffsetX, Y, Z);          // 로컬 X는 고정 오프셋(액터 앞)
}

bool UPoseClassifierComponent::GetExtendedArmWorld(const FPersonPose& Pose, FVector& OutStart, FVector& OutDir, FVector& OutUpperArmDir) const
{
    if (Pose.XY.Num() < 17 || !GetOwner()) return false;

    // 2D 포인트
    const FVector2f& LHip = Pose.XY[11];
    const FVector2f& RHip = Pose.XY[12];
    const FVector2f& LSh = Pose.XY[5];
    const FVector2f& RSh = Pose.XY[6];
    const FVector2f& LWr = Pose.XY[9];
    const FVector2f& RWr = Pose.XY[10];

    auto Finite2D = [](const FVector2f& V)
        {
            return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
        };
    if (!Finite2D(LHip) || !Finite2D(RHip) || !Finite2D(LSh) || !Finite2D(RSh) || !Finite2D(LWr) || !Finite2D(RWr))
        return false;

    const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

    // 2D → 로컬(Y=오른쪽, Z=위), X=깊이(고정 오프셋)
    auto Map2DToLocal = [&](const FVector2f& P)->FVector
        {
            const FVector2f Rel = P - Pelvis2D;
            const float Y = Rel.X * PixelToUU;                 // 화면 X → 로컬 Y
            const float Z = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * PixelToUU; // 화면 Y(아래+) → 로컬 Z(위+)
            return FVector(DepthOffsetX, Y, Z);
        };

    // 로컬 좌표
    const FVector LShL = Map2DToLocal(LSh);
    const FVector RShL = Map2DToLocal(RSh);
    const FVector LWrL = Map2DToLocal(LWr);
    const FVector RWrL = Map2DToLocal(RWr);

    // 손 선택: Auto(더 뻗은 팔) / Left / Right
    const float LenL = (LWrL - LShL).Size();
    const float LenR = (RWrL - RShL).Size();

    bool bUseLeft = (LenL >= LenR);
    switch (HandSource)
    {
    case EMainHand::Left:  bUseLeft = true;  break;
    case EMainHand::Right: bUseLeft = false; break;
    case EMainHand::Auto:  /* keep bUseLeft as decided by reach */ break;
    }

    const FVector ShL = bUseLeft ? LShL : RShL;
    const FVector WrL = bUseLeft ? LWrL : RWrL;

    const FTransform& Xf = GetOwner()->GetActorTransform();

    // 월드 변환
    const FVector LShW = Xf.TransformPosition(LShL);
    const FVector RShW = Xf.TransformPosition(RShL);
    const FVector ShW = Xf.TransformPosition(ShL);
    const FVector WrW = Xf.TransformPosition(WrL);

    // 어깨→손목 벡터 (월드)
    const FVector vSW = (WrW - ShW);
    const float   len = vSW.Size();
    if (len <= KINDA_SMALL_NUMBER) return false;

    const FVector Dir = vSW / len; // 진행 방향(어깨→손목)

    // 시작점 결정:
    //  - bStartAtShoulderMid == true  → 어깨 중점
    //  - else StartAlongArmRatio      → 어깨에서 손목 방향으로 비율만큼 이동 (1=손목, >1=손목 바깥)
    FVector StartW = ShW;
    if (bStartAtShoulderMid)
    {
        const FVector ShoulderMidW = 0.5f * (LShW + RShW);
        StartW = ShoulderMidW;
    }
    else
    {
        StartW = ShW + vSW * StartAlongArmRatio;
    }

    // UpperArmDir: 평면 구성용 축. 기본은 Dir 과 동일(안정적).
    FVector Upper = Dir;

    OutStart = StartW;
    OutDir = Dir;
    OutUpperArmDir = Upper;
    return true;
}

bool UPoseClassifierComponent::GetCameraView(FVector& OutCamLoc, FVector& OutCamForward) const
{
    OutCamLoc = FVector::ZeroVector;
    OutCamForward = FVector::ForwardVector;

    if (!GetWorld()) return false;

    // 우선: 오너가 플레이어가 조종 중이면 그 컨트롤러의 뷰포인트
    if (AActor* Owner = GetOwner())
    {
        if (APawn* Pawn = Cast<APawn>(Owner))
        {
            if (AController* Ctrl = Pawn->GetController())
            {
                FVector Loc; FRotator Rot;
                Ctrl->GetPlayerViewPoint(Loc, Rot);
                OutCamLoc = Loc;
                OutCamForward = Rot.Vector().GetSafeNormal();
                return true;
            }
        }
    }

    // 폴백: 첫 번째 플레이어 카메라
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector Loc; FRotator Rot;
        PC->GetPlayerViewPoint(Loc, Rot);
        OutCamLoc = Loc;
        OutCamForward = Rot.Vector().GetSafeNormal();
        return true;
    }
    return false;
}

void UPoseClassifierComponent::OverlapPlaneOnce(const FVector& Center, const FQuat& Rot, TSet<AActor*>& UniqueActors, float DebugLifeTime)
{
    const FVector HalfExtent(SwingPlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SwingPlaneSweep), /*bTraceComplex*/ false, GetOwner());
    const bool bAny = GetWorld()->OverlapMultiByChannel(Overlaps, Center, Rot, PlaneChannel, Shape, Params);

    if (bDrawDebugPlane)
    {
        DrawDebugBox(GetWorld(), Center, HalfExtent, Rot, FColor::Cyan, false, DebugLifeTime, 0, 2.0f);
    }

    if (!bAny) return;

    for (const FOverlapResult& R : Overlaps)
    {
        if (AActor* A = R.GetActor())
        {
            if (A != GetOwner())
            {
                UniqueActors.Add(A);
            }
        }
    }
}

bool UPoseClassifierComponent::GetShoulderMidWorld(FVector& OutMidWorld) const
{
    OutMidWorld = FVector::ZeroVector;
    if (!Receiver || !GetOwner()) return false;

    const TArray<FPersonPose>& Poses = Receiver->GetLatestPoses();
    if (Poses.Num() == 0) return false;

    const int32 LSH = 5, RSH = 6, LHIP = 11, RHIP = 12;

    // 가장 신뢰도 높은 양어깨/양힙 유효 사람 선택
    int32 BestIdx = INDEX_NONE;
    float BestScore = -FLT_MAX;

    for (int32 i = 0; i < Poses.Num(); ++i)
    {
        const FPersonPose& P = Poses[i];
        if (!(P.XY.IsValidIndex(LSH) && P.XY.IsValidIndex(RSH) &&
            P.XY.IsValidIndex(LHIP) && P.XY.IsValidIndex(RHIP))) continue;

        const FVector2f& LSh = P.XY[LSH], & RSh = P.XY[RSH], & LHip = P.XY[LHIP], & RHip = P.XY[RHIP];
        if (!(IsFinite2D(LSh) && IsFinite2D(RSh) && IsFinite2D(LHip) && IsFinite2D(RHip))) continue;

        const float cL = (P.Conf.IsValidIndex(LSH) ? P.Conf[LSH] : 1.f);
        const float cR = (P.Conf.IsValidIndex(RSH) ? P.Conf[RSH] : 1.f);
        const float Score = cL + cR;

        if (Score > BestScore) { BestScore = Score; BestIdx = i; }
    }
    if (BestIdx == INDEX_NONE) return false;

    const FPersonPose& Best = Poses[BestIdx];
    const FVector2f Pelvis2D = (Best.XY[LHIP] + Best.XY[RHIP]) * 0.5f;

    // 이미지→로컬(Y=오른쪽, Z=위, X=깊이)
    auto Map2DToLocal = [&](const FVector2f& P)->FVector
        {
            const FVector2f Rel = P - Pelvis2D;
            const float Y = Rel.X * PixelToUU;                                   // imgX -> localY
            const float Z = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * PixelToUU;    // imgY(↓) -> localZ(↑)
            return FVector(DepthOffsetX, Y, Z);
        };

    const FVector LShL = Map2DToLocal(Best.XY[LSH]);
    const FVector RShL = Map2DToLocal(Best.XY[RSH]);

    const FTransform& OwnerXf = GetOwner()->GetActorTransform();
    const FVector LShW = OwnerXf.TransformPosition(LShL);
    const FVector RShW = OwnerXf.TransformPosition(RShL);

    OutMidWorld = 0.5f * (LShW + RShW);
    return true;
}

void UPoseClassifierComponent::DetectSwing()
{
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

    // PASS → 실제 스윙 처리
    ApplySwingPlaneSweepDamage(Snaps);
    LastSwingLoggedMs = Now;
    WindowBuffer.Reset();
}

void UPoseClassifierComponent::ApplySwingPlaneSweepDamage( const TArray<const FTimedPoseSnapshot*>& Snaps, TSubclassOf<AActor> ProjectileClass, float ProjectileSpeed, float ProjectileLifeSeconds, float SpawnForwardOffset, bool bSpawnAtEachPlane)
{
    if (!GetWorld() || Snaps.Num() < 2) return;

    // 1) 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 2) 첫/끝 스냅샷으로 평면 축 + "손목 방향" 추출
    auto MakePlaneFromSnap = [&](const FTimedPoseSnapshot* S,
        FVector& OutStartW,
        FQuat& OutRot,
        FVector& OutU,
        FVector& OutDirToWrist) -> bool
        {
            int32 PersonIdx = INDEX_NONE;
            if (!PickBestPerson(S->Poses, PersonIdx)) return false;
            const FPersonPose& P = S->Poses[PersonIdx];

            FVector StartW, DirW, UpperW;
            if (!GetExtendedArmWorld(P, StartW, DirW, UpperW)) return false; // DirW = 어깨→손목 (단위)

            // 법선 N = CamFwd × UpperW
            FVector N = FVector::CrossProduct(CamFwd, UpperW).GetSafeNormal();
            if (N.IsNearlyZero())
            {
                N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
                if (N.IsNearlyZero()) N = FVector::RightVector;
            }

            // U = UpperW의 평면 정사영
            FVector U = (UpperW - FVector::DotProduct(UpperW, N) * N).GetSafeNormal();
            if (U.IsNearlyZero())
                U = (CamFwd - FVector::DotProduct(CamFwd, N) * N).GetSafeNormal();

            FVector Z = FVector::CrossProduct(N, U).GetSafeNormal();
            if (Z.IsNearlyZero()) Z = FVector::UpVector;

            const FMatrix M(
                FPlane(N.X, N.Y, N.Z, 0.f),   // X축 = 평면 법선
                FPlane(U.X, U.Y, U.Z, 0.f),   // Y축 = 스윙 진행(평면 내 팔축)
                FPlane(Z.X, Z.Y, Z.Z, 0.f),   // Z축
                FPlane(0, 0, 0, 1)
            );

            OutStartW = StartW;            // 어깨(또는 옵션으로 정한 시작점)
            OutRot = FQuat(M);
            OutU = U;                 // 평면 진행축(스윕용)
            OutDirToWrist = DirW.GetSafeNormal(); // ★ 발사용: 어깨→손목
            if (OutDirToWrist.IsNearlyZero())   // 안전망: 못 구하면 U 쓰기
                OutDirToWrist = U;
            return true;
        };

    FVector Start0, Start1;  FQuat Rot0, Rot1;  FVector U0, U1;
    FVector DirW0, DirW1; // ★ 어깨→손목 방향(월드)
    const FTimedPoseSnapshot* SnapFirst = Snaps[0];
    const FTimedPoseSnapshot* SnapLast = Snaps.Last();

    if (!MakePlaneFromSnap(SnapFirst, Start0, Rot0, U0, DirW0)) return;
    if (!MakePlaneFromSnap(SnapLast, Start1, Rot1, U1, DirW1)) return;

    // 3) 보간 개수 산정
    const float AngleRad = Rot0.AngularDistance(Rot1);
    const float AngleDeg = FMath::RadiansToDegrees(AngleRad);
    const float StepDeg = FMath::Max(1e-3f, DegreesPerInterp);
    int32 TotalPlanes = FMath::CeilToInt(AngleDeg / StepDeg) + 1;
    TotalPlanes = FMath::Clamp(TotalPlanes, FMath::Max(2, MinInterpPlanes), MaxInterpPlanes);

    const FVector HalfExtent(SwingPlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    TSet<AActor*> UniqueActors;

    // 발사 유틸
    auto SpawnProjectile = [&](const FVector& SpawnLoc, const FVector& DirWorld)
        {
            if (!ProjectileClass) return;

            const FRotator Rot = DirWorld.Rotation();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            Params.Owner = GetOwner();
            Params.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;

            AActor* Proj = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, Rot, Params);
            if (!Proj) return;

            if (UProjectileMovementComponent* PMC = Proj->FindComponentByClass<UProjectileMovementComponent>())
            {
                PMC->Velocity = DirWorld * ProjectileSpeed;
            }
            else
            {
                Proj->SetActorLocation(SpawnLoc + DirWorld * (ProjectileSpeed * GetWorld()->GetDeltaSeconds()));
            }

            if (ProjectileLifeSeconds > 0.f)
            {
                Proj->SetLifeSpan(ProjectileLifeSeconds);
            }
        };

    // 메인 루프
    for (int32 k = 0; k < TotalPlanes; ++k)
    {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);

        // 회전 보간(스윕 평면용)
        const FQuat RotK = FQuat::Slerp(Rot0, Rot1, t).GetNormalized();

        // 축(스윕용)
        const FVector N = RotK.GetAxisX();
        const FVector U = RotK.GetAxisY();

        // 시작점(어깨) 보간
        const FVector StartK = FMath::Lerp(Start0, Start1, t);

        // ★발사 방향: 어깨→손목 방향 보간
        // (간단 선형 보간 후 정규화; 더 부드럽게 하려면 쿼터니언 기반 slerp 구현해도 됨)
        const FVector DirK = ((1.f - t) * DirW0 + t * DirW1).GetSafeNormal();
        const FVector FireDir = DirK.IsNearlyZero() ? U : DirK;

        // 평면 중심(Overlap용) — 기존 유지
        const FVector CenterK = StartK + CamFwd * PlaneDistance + U * HalfExtent.Y;

        // 3-1) 평면 스윕 (근접 판정)
        OverlapPlaneOnce(CenterK, RotK, UniqueActors, DebugSweepDrawTime);

        // 3-2) (옵션) 각 보간 평면에서 발사 — ★어깨에서 손목 방향으로 발사★
        if (bSpawnAtEachPlane && ProjectileClass)
        {
            const FVector SpawnLoc = StartK + FireDir * SpawnForwardOffset;
            SpawnProjectile(SpawnLoc, FireDir);
        }

        // 디버그: 어깨 기준 화살표
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + CamFwd * 80.f, 15.f, FColor::Blue, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + FireDir * 80.f, 15.f, FColor::Green, false, DebugSweepDrawTime, 0, 1.5f); // ★발사 방향
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + U * 80.f, 15.f, FColor::Yellow, false, DebugSweepDrawTime, 0, 1.5f); // 평면 진행축
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + N * 80.f, 15.f, FColor::Magenta, false, DebugSweepDrawTime, 0, 1.5f);
    }

    // 3-3) 마지막 한 발만 쏘는 경우 — ★어깨에서 DirW1 방향★
    if (!bSpawnAtEachPlane && ProjectileClass)
    {
        const FVector FireDirLast = DirW1.IsNearlyZero() ? U1 : DirW1;
        const FVector SpawnLoc = Start1 + FireDirLast * SpawnForwardOffset;
        SpawnProjectile(SpawnLoc, FireDirLast);
    }

    if (UniqueActors.Num() == 0) return;

    // 4) 근접 데미지 일괄 적용
    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    TSubclassOf<UDamageType> DmgCls = DamageTypeClass;

    for (AActor* A : UniqueActors)
    {
        UGameplayStatics::ApplyDamage(A, SwingDamageAmount, Inst, GetOwner(), DmgCls);
    }
}

void UPoseClassifierComponent::DetectArrow()
{
    if (WindowBuffer.Num() == 0 || !Receiver) return;

    const uint64 Now = NowMsFromReceiver();
    if (Now == 0) return;

    // 쿨다운
    if (LastArrowLoggedMs != 0)
    {
        const uint64 CoolMs = static_cast<uint64>(ArrowCooldownSeconds * 1000.0);
        if (Now - LastArrowLoggedMs < CoolMs) return;
    }

    // 윈도우에서 유효 스냅샷 모으기
    const uint64 SpanMs = static_cast<uint64>(WindowSeconds * 1000.0);
    const uint64 Oldest = (Now > SpanMs) ? (Now - SpanMs) : 0;

    struct FArrowSample { double T; float ShoulderW; float WristDist; bool Close; };
    TArray<FArrowSample> Smp;
    Smp.Reserve(WindowBuffer.Num());

    for (const auto& Snap : WindowBuffer)
    {
        if (Snap.TimestampMs < Oldest || Snap.Poses.Num() == 0) continue;

        int32 Idx = INDEX_NONE;
        if (!PickBestPerson(Snap.Poses, Idx)) continue;
        const auto& P = Snap.Poses[Idx];
        if (P.XY.Num() < 17) continue;

        float SW = 0.f, WD = 0.f;
        const bool bClose = IsHandsClose(P, SW, WD);
        if (SW <= KINDA_SMALL_NUMBER) continue;

        FArrowSample a;
        a.T = (double)Snap.TimestampMs * 0.001;
        a.ShoulderW = SW;
        a.WristDist = WD;
        a.Close = bClose;
        Smp.Add(MoveTemp(a));
    }
    if (Smp.Num() < 3) return;

    // 1) 시작: "가까운 상태"가 윈도우에 반드시 존재해야 함
    float MinWD = FLT_MAX;   // 최소 손목거리
    float MaxWD = 0.f;       // 최대 손목거리
    double TMin = 0.0, TMax = 0.0;

    bool bSawClose = false;
    for (const auto& a : Smp)
    {
        if (a.Close) bSawClose = true;

        if (a.WristDist < MinWD) { MinWD = a.WristDist; TMin = a.T; }
        if (a.WristDist > MaxWD) { MaxWD = a.WristDist; TMax = a.T; }
    }
    if (!bSawClose) return;

    // 평균 어깨폭으로 정규화 기준 잡기
    double MeanSW = 0.0;
    for (const auto& a : Smp) MeanSW += (double)a.ShoulderW;
    MeanSW /= (double)Smp.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    const double MinNorm = (double)MinWD / MeanSW;
    const double MaxNorm = (double)MaxWD / MeanSW;
    const double DeltaNorm = MaxNorm - MinNorm;

    // 2) 종료: 충분히 멀어졌는지 + 증가량 조건
    const bool bFarEnough = (MaxNorm >= (double)ArrowFarRatio);
    const bool bDeltaEnough = (DeltaNorm >= (double)ArrowMinDeltaNorm);

    // 3) 멀어지는 속도(최소 속도) — 최대점과 최소점 사이로 계산
    double SpeedNorm = 0.0;
    if (TMax > TMin)
    {
        const double dt = TMax - TMin; // sec
        SpeedNorm = DeltaNorm / dt;    // 어깨폭/초
    }
    const bool bFastEnough = (SpeedNorm >= (double)ArrowMinSpeedNorm);

    if (bFarEnough && bDeltaEnough && bFastEnough)
    {
        const FTimedPoseSnapshot& LastSnap = WindowBuffer.Last();
        int32 BestIdx = INDEX_NONE;
        if (PickBestPerson(LastSnap.Poses, BestIdx))
        {
            FVector StartW, DirW, UpperW;
            if (GetExtendedArmWorld(LastSnap.Poses[BestIdx], StartW, DirW, UpperW))
            {
                // 카메라 시선과 상완 모두에 수직인 법선의 평면으로 데미지
                ApplyArrowPlaneDamage(StartW, UpperW);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[PoseClassifier] Arrow"));
        LastArrowLoggedMs = Now;
        WindowBuffer.Reset();
    }
}

void UPoseClassifierComponent::ApplyArrowPlaneDamage(const FVector& Start, const FVector& UpperArmDir)
{
    if (!GetWorld()) return;

    // 1) 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 2) 법선 = CamFwd × UpperArmDir  (둘 다에 수직)
    FVector N = FVector::CrossProduct(CamFwd, UpperArmDir).GetSafeNormal();
    if (N.IsNearlyZero())
    {
        N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::RightVector;
    }

    // 3) 평면 내 팔 축 U: UpperArmDir을 평면에 정사영 후 정규화
    //    (U가 박스의 '가로'(Y) 축이 됨)
    FVector U = (UpperArmDir - FVector::DotProduct(UpperArmDir, N) * N).GetSafeNormal();
    if (U.IsNearlyZero())
    {
        // 팔 축이 불안정하면 카메라 전방의 평면 정사영으로 대체
        U = (CamFwd - FVector::DotProduct(CamFwd, N) * N).GetSafeNormal();
    }

    // 4) Z축 = N × U  (평면 내에서 U에 수직)
    FVector Z = FVector::CrossProduct(N, U).GetSafeNormal();
    if (Z.IsNearlyZero())
    {
        // 드문 수치불안정 폴백
        Z = FVector::UpVector;
    }

    // 5) 회전행렬 구성: X=N(법선), Y=U(팔 방향), Z=N×U
    const FMatrix M(
        FPlane(N.X, N.Y, N.Z, 0.f),   // X
        FPlane(U.X, U.Y, U.Z, 0.f),   // Y
        FPlane(Z.X, Z.Y, Z.Z, 0.f),   // Z
        FPlane(0, 0, 0, 1)
    );
    const FQuat Rot(M);

    // 6) 박스 형상
    const FVector HalfExtent(ArrowPlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);

    // 7) 평면 중심:
    //    기본은 어깨에서 카메라 전방으로 PlaneDistance,
    //    여기에 상완 방향(U)으로 '반폭'만큼 더 이동시켜 반쪽만 커버
    //    (반대편을 때리고 싶다면 -HalfExtent.Y로 바꾸면 됨)
    const FVector Center =
        Start
        + CamFwd * PlaneDistance
        + U * HalfExtent.Y;      // ← 한쪽으로 밀기 (반폭)

    // 8) Overlap
    const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ArrowPlane), false, GetOwner());
    const bool bAny = GetWorld()->OverlapMultiByChannel(Overlaps, Center, Rot, PlaneChannel, Shape, Params);

    // 9) 디버그
    if (bDrawDebugPlane)
    {
        DrawDebugBox(GetWorld(), Center, HalfExtent, Rot, FColor::Cyan, false, 1.0f, 0, 2.0f);
        
    }

    DrawDebugDirectionalArrow(GetWorld(), Center, Center + CamFwd * 120.f, 20.f, FColor::Blue, false, 1.0f, 0, 5.0f); // 카메라 시선
    DrawDebugDirectionalArrow(GetWorld(), Center, Center + U * 120.f, 20.f, FColor::Yellow, false, 1.0f, 0, 5.0f); // 상완(가로 축)
    DrawDebugDirectionalArrow(GetWorld(), Center, Center + N * 120.f, 20.f, FColor::Magenta, false, 1.0f, 0, 5.0f); // 법선

    if (!bAny) return;

    // 10) 유니크 액터에 데미지
    TSet<AActor*> Unique;
    for (const auto& R : Overlaps)
        if (AActor* A = R.GetActor())
            if (A != GetOwner()) Unique.Add(A);

    if (Unique.Num() == 0) return;

    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    TSubclassOf<UDamageType> DmgCls = DamageTypeClass;

    for (AActor* A : Unique)
    {
        UGameplayStatics::ApplyDamage(A, ArrowDamageAmount, Inst, GetOwner(), DmgCls);
    }
}

void UPoseClassifierComponent::DetectSingleSwing(EMainHand WhichHand)
{
    if (WindowBuffer.Num() == 0) return;

    const uint64 Now = NowMsFromReceiver();
    if (Now == 0) return;

    // 공용 쿨다운
    if (LastSwingLoggedMs != 0) {
        const uint64 CoolMs = (uint64)(SwingCooldownSeconds * 1000.0);
        const uint64 Elapsed = Now - LastSwingLoggedMs;
        if (Elapsed < CoolMs) return;
    }

    // 최근 구간만 사용
    const double UseSec = FMath::Clamp((double)SwingRecentSeconds, 0.2, (double)WindowSeconds);
    const uint64 RecentOldest = (Now > (uint64)(UseSec * 1000.0)) ? (Now - (uint64)(UseSec * 1000.0)) : 0;

    // 스냅샷 모으기
    TArray<const FTimedPoseSnapshot*> Snaps;
    Snaps.Reserve(WindowBuffer.Num());
    for (const auto& S : WindowBuffer) {
        if (S.TimestampMs >= RecentOldest && S.Poses.Num() > 0)
            Snaps.Add(&S);
    }
    if (Snaps.Num() < 3) return;

    // ----- 단일 스윙용 로컬 샘플(몸 중심 C 추가) -----
    struct FHandSample
    {
        double    T = 0.0;
        FVector2D L = FVector2D::ZeroVector; // 왼손목 (픽셀)
        FVector2D R = FVector2D::ZeroVector; // 오른손목 (픽셀)
        FVector2D C = FVector2D::ZeroVector; // 몸 중심(어깨 중점) (픽셀)  ← NEW
        float     ShoulderW = 0.f;           // 어깨폭 (픽셀)
        bool      Open = false;              // "손 가까움"의 반대 (멀리 떨어짐)
    };

    // 샘플 추출
    TArray<FHandSample> Samples;
    Samples.Reserve(Snaps.Num());

    auto IsFinite2DLoc = [](const FVector2f& p)->bool {
        return FMath::IsFinite(p.X) && FMath::IsFinite(p.Y);
        };

    for (const FTimedPoseSnapshot* S : Snaps) {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(S->Poses, idx)) continue;
        const FPersonPose& P = S->Poses[idx];
        if (P.XY.Num() < 17) continue;

        // 손목
        const FVector2f& LWR = P.XY[9];
        const FVector2f& RWR = P.XY[10];
        if (!IsFinite2DLoc(LWR) || !IsFinite2DLoc(RWR)) continue;

        // 어깨 중점(몸 중심 C)
        const int32 LSH = 5, RSH = 6;
        if (!(P.XY.IsValidIndex(LSH) && P.XY.IsValidIndex(RSH))) continue;
        const FVector2f& LSH2 = P.XY[LSH];
        const FVector2f& RSH2 = P.XY[RSH];
        if (!IsFinite2DLoc(LSH2) || !IsFinite2DLoc(RSH2)) continue;

        const FVector2D C = 0.5 * FVector2D(LSH2.X + RSH2.X, LSH2.Y + RSH2.Y);

        float ShoulderW = 0.f, WristDist = 0.f;
        const bool bClose = IsHandsClose(P, ShoulderW, WristDist);
        if (ShoulderW <= KINDA_SMALL_NUMBER) continue;

        FHandSample smp;
        smp.T = (double)S->TimestampMs * 0.001;
        smp.L = FVector2D(LWR.X, LWR.Y);
        smp.R = FVector2D(RWR.X, RWR.Y);
        smp.C = C; // 몸 중심 저장
        smp.ShoulderW = ShoulderW;
        smp.Open = !bClose; // "가깝지 않음"
        Samples.Add(MoveTemp(smp));
    }
    if (Samples.Num() < 3) return;

    // 고정 Hz 리샘플
    const double FixedHz = 30.0;
    const double FixedDt = 1.0 / FixedHz;

    auto ResampleHands = [&](const TArray<FHandSample>& In, double Hz, TArray<FHandSample>& Out)
        {
            Out.Reset();
            if (In.Num() < 2) return;

            const double t0 = In[0].T;
            const double t1 = In.Last().T;
            const int N = FMath::Max(2, (int)FMath::RoundToInt((t1 - t0) * Hz) + 1);
            Out.Reserve(N);

            int j = 0;
            for (int i = 0; i < N; ++i) {
                const double t = t0 + i * (1.0 / Hz);
                while (j + 1 < In.Num() && In[j + 1].T < t) ++j;
                const int k = FMath::Min(j + 1, In.Num() - 1);
                const double tA = In[j].T, tB = In[k].T;
                const double alpha = (tB > tA) ? FMath::Clamp((t - tA) / (tB - tA), 0.0, 1.0) : 0.0;

                FHandSample o;
                o.T = t;
                o.L = FMath::Lerp(In[j].L, In[k].L, alpha);
                o.R = FMath::Lerp(In[j].R, In[k].R, alpha);
                o.C = FMath::Lerp(In[j].C, In[k].C, alpha); // 몸 중심도 보간
                o.ShoulderW = FMath::Lerp(In[j].ShoulderW, In[k].ShoulderW, alpha);
                o.Open = (alpha < 0.5) ? In[j].Open : In[k].Open;
                Out.Add(o);
            }
        };

    TArray<FHandSample> U;
    ResampleHands(Samples, FixedHz, U);
    if (U.Num() < 3) return;

    // EMA 스무딩 (손목/몸중심)
    auto SmoothEMA2D = [&](TArray<FHandSample>& A, double Alpha)
        {
            if (A.Num() == 0) return;
            FVector2D l = A[0].L, r = A[0].R, c = A[0].C;
            for (int i = 0; i < A.Num(); ++i) {
                l = (1.0 - Alpha) * l + Alpha * A[i].L;
                r = (1.0 - Alpha) * r + Alpha * A[i].R;
                c = (1.0 - Alpha) * c + Alpha * A[i].C;
                A[i].L = l; A[i].R = r; A[i].C = c;
            }
        };
    SmoothEMA2D(U, 0.35);

    // Open(손 떨어짐) 커버리지
    int32 OpenCount = 0;
    for (const auto& s : U) if (s.Open) ++OpenCount;
    const double OpenCoverage = (double)OpenCount / FMath::Max(1, U.Num());

    if (OpenCoverage < MinOpenCoverage) return;

    // 평균 어깨폭
    double MeanSW = 0.0;
    for (const auto& s : U) MeanSW += (double)FMath::Max(s.ShoulderW, KINDA_SMALL_NUMBER);
    MeanSW /= (double)U.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    // 손별 메트릭 (반경 기반)
    struct FHandMetrics {
        double PeakSpeed = 0.0; // 전체 속도 norm (참고용)
        double PathLen = 0.0; // 전체 경로 길이(어깨폭 단위)
        double AvgSpeed = 0.0;
        double DeltaRad = 0.0; // 순반경증가 r_end - r_start (어깨폭)
        double OutwardPath = 0.0; // 바깥(away)만 누적한 경로 합 (어깨폭)
        int32  Reversals = 0;   // 반경 속도 방향 반전 수
    };

    auto ComputeMetricsFor = [&](bool bLeft) -> FHandMetrics
        {
            FHandMetrics M{};
            if (U.Num() < 2) return M;

            const FVector2D startP = bLeft ? U[0].L : U[0].R;
            const FVector2D startC = U[0].C;
            const FVector2D endP = bLeft ? U.Last().L : U.Last().R;
            const FVector2D endC = U.Last().C;

            // 디바운스용
            int lastSign = 0;
            double hold = 0.0;

            double usedTime = 0.0;
            for (int i = 1; i < U.Num(); ++i) {
                if (!U[i].Open) continue;

                const FVector2D p0 = bLeft ? U[i - 1].L : U[i - 1].R;
                const FVector2D p1 = bLeft ? U[i].L : U[i].R;
                const FVector2D c0 = U[i - 1].C;
                const FVector2D c1 = U[i].C;

                const FVector2D d = p1 - p0;

                // 전체 속도 정규화
                const double vNorm = (d.Size() / FixedDt) / MeanSW;
                M.PathLen += (d.Size() / MeanSW);
                if (vNorm > M.PeakSpeed) M.PeakSpeed = vNorm;

                // 반경 변화(away>0, toward<0)
                const double r0 = (p0 - c0).Size();
                const double r1 = (p1 - c1).Size();
                const double dr = (r1 - r0);                 // 픽셀
                const double drNorm = dr / MeanSW;           // 어깨폭 단위

                // 바깥(away) 구간만 누적
                if (drNorm > 0.0) M.OutwardPath += drNorm;

                // 반경 속도(정규화)
                const double radialVelNorm = (dr / FixedDt) / MeanSW;
                const int sgn = (radialVelNorm > SignDeadband) ? +1 :
                    (radialVelNorm < -SignDeadband) ? -1 : 0;
                if (sgn == 0) {
                    hold = 0.0;
                }
                else {
                    if (lastSign == 0 || sgn == lastSign) {
                        hold += FixedDt;
                        lastSign = sgn;
                    }
                    else {
                        if (hold >= AxisHoldSeconds) {
                            ++M.Reversals;
                            lastSign = sgn;
                            hold = 0.0;
                        }
                    }
                }

                usedTime += FixedDt;
            }

            if (usedTime > 0.0) M.AvgSpeed = M.PathLen / usedTime;

            // 순반경증가(시작/끝 기준)
            const double rStart = (startP - startC).Size();
            const double rEnd = (endP - endC).Size();
            M.DeltaRad = (rEnd - rStart) / MeanSW;

            return M;
        };

    const FHandMetrics Lm = ComputeMetricsFor(true);
    const FHandMetrics Rm = ComputeMetricsFor(false);

    // 활성 손: 피크 속도 우선, 동률이면 평균 속도
    bool bUseLeft = true;
    switch (WhichHand)
    {
    case EMainHand::Left:
        bUseLeft = true;
        break;
    case EMainHand::Right:
        bUseLeft = false;
        break;
    case EMainHand::Auto:
        break;
    default:
        // 기존 Auto 로직: 피크속도 우선, 동률이면 평균속도
        const bool bLeftActive =
            (Lm.PeakSpeed > Rm.PeakSpeed) ||
            (FMath::IsNearlyEqual(Lm.PeakSpeed, Rm.PeakSpeed, 1e-3) && (Lm.AvgSpeed >= Rm.AvgSpeed));
        bUseLeft = bLeftActive;
        break;
    }

    const FHandMetrics& M = bUseLeft ? Lm : Rm;

    // ===== 최종 판정: "몸에서 바깥쪽(away) 스윙" =====
    const bool bPass =
        (OpenCoverage >= MinOpenCoverage) &&
        ((M.AvgSpeed >= MinAvgSpeedNorm) || (M.PeakSpeed >= MinPeakSpeedNorm)) &&
        (M.DeltaRad >= MinDeltaRadNorm) &&          // 순반경증가
        (M.OutwardPath >= MinOutwardPathNorm) &&    // 바깥 누적 경로
        (M.Reversals <= MaxRadialReversals);

    if (!bPass) return;

    ApplyMainHandMovementPlaneAndFire(Snaps, HandSource, AmuletProjectileClass, 1000.f, 3.0f, 12.f, true);
    LastSwingLoggedMs = Now;
    WindowBuffer.Reset();
}

void UPoseClassifierComponent::ApplyMainHandMovementPlaneAndFire(
    const TArray<const FTimedPoseSnapshot*>& Snaps,
    EMainHand WhichHand,
    TSubclassOf<AActor> ProjectileClass,
    float ProjectileSpeed,
    float ProjectileLifeSeconds,
    float SpawnForwardOffset,
    bool bSpawnAtEachPlane)
{
    if (!GetWorld() || Snaps.Num() < 3) return;

    // 1) 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 2) 유틸: 손목 2D/어깨폭, 손목 월드 변환
    auto PickWrist2D = [](const FPersonPose& P, bool bRight, FVector2D& OutWr) -> bool
        {
            const int32 idx = bRight ? COCO_RWR : COCO_LWR;
            if (!P.XY.IsValidIndex(idx)) return false;
            const FVector2f W = P.XY[idx];
            if (!FMath::IsFinite(W.X) || !FMath::IsFinite(W.Y)) return false;
            OutWr = FVector2D(W.X, W.Y);
            return true;
        };
    auto PickShoulderWidth2D = [](const FPersonPose& P, double& OutSW)->bool
        {
            if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH))) return false;
            const FVector2f L = P.XY[COCO_LSH], R = P.XY[COCO_RSH];
            if (!FMath::IsFinite(L.X) || !FMath::IsFinite(L.Y) || !FMath::IsFinite(R.X) || !FMath::IsFinite(R.Y)) return false;
            OutSW = FVector2D::Distance(FVector2D(L.X, L.Y), FVector2D(R.X, R.Y));
            return true;
        };
    auto GetWristWorldOfSnap = [&](const FTimedPoseSnapshot* S, bool bRight, FVector& OutWristW) -> bool
        {
            int32 PersonIdx = INDEX_NONE;
            if (!PickBestPerson(S->Poses, PersonIdx)) return false;
            const FPersonPose& P = S->Poses[PersonIdx];
            if (P.XY.Num() < 17) return false;

            // Pelvis 계산
            const FVector2f& LHip = P.XY[11];
            const FVector2f& RHip = P.XY[12];
            if (!FMath::IsFinite(LHip.X) || !FMath::IsFinite(LHip.Y) ||
                !FMath::IsFinite(RHip.X) || !FMath::IsFinite(RHip.Y)) return false;
            const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

            // 손목 2D
            const int32 idxWr = bRight ? COCO_RWR : COCO_LWR;
            if (!P.XY.IsValidIndex(idxWr)) return false;
            const FVector2f W2 = P.XY[idxWr];
            if (!FMath::IsFinite(W2.X) || !FMath::IsFinite(W2.Y)) return false;

            // 2D→로컬(Y=오른쪽, Z=위), X=깊이 오프셋
            const FVector2f Rel = W2 - Pelvis2D;
            const float YY = Rel.X * PixelToUU;
            const float ZZ = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * PixelToUU;
            const FVector Local(DepthOffsetX, YY, ZZ);

            // 로컬→월드
            const FTransform& Xf = GetOwner()->GetActorTransform();
            OutWristW = Xf.TransformPosition(Local);
            return true;
        };

    // 3) “최우선 사람”을 시간순으로 뽑아 시퀀스 구성
    struct FSample { double T; const FTimedPoseSnapshot* Snap; FVector2D Wr; double SW; };
    TArray<FSample> Smp; Smp.Reserve(Snaps.Num());
    for (const FTimedPoseSnapshot* S : Snaps)
    {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(S->Poses, idx)) continue;
        const FPersonPose& P = S->Poses[idx];
        Smp.Add({ (double)S->TimestampMs * 0.001, S, FVector2D::ZeroVector, 0.0 });
    }
    if (Smp.Num() < 3) return;

    // 4) 메인 손 결정 (Auto → 첫/끝 이동량 큰 손)
    auto DecideHandRight = [&](EMainHand H)->bool
        {
            if (H == EMainHand::Left)  return false;
            if (H == EMainHand::Right) return true;

            int32 p0 = INDEX_NONE, p1 = INDEX_NONE;
            PickBestPerson(Snaps[0]->Poses, p0);
            PickBestPerson(Snaps.Last()->Poses, p1);
            if (p0 == INDEX_NONE || p1 == INDEX_NONE) return true;

            const FPersonPose& P0 = Snaps[0]->Poses[p0];
            const FPersonPose& P1 = Snaps.Last()->Poses[p1];
            FVector2D L0, L1, R0, R1;
            const bool bL = PickWrist2D(P0, false, L0) && PickWrist2D(P1, false, L1);
            const bool bR = PickWrist2D(P0, true, R0) && PickWrist2D(P1, true, R1);
            if (bL && bR) return ((R1 - R0).Size() >= (L1 - L0).Size());
            if (bR) return true;
            if (bL) return false;
            return true;
        };
    const bool bRight = DecideHandRight(WhichHand);

    // 5) 각 샘플에 손목/어깨폭 채우기
    for (int32 i = 0; i < Smp.Num(); ++i)
    {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(Smp[i].Snap->Poses, idx)) { Smp.RemoveAt(i--); continue; }
        const FPersonPose& P = Smp[i].Snap->Poses[idx];
        if (!PickWrist2D(P, bRight, Smp[i].Wr) || !PickShoulderWidth2D(P, Smp[i].SW)) { Smp.RemoveAt(i--); continue; }
    }
    if (Smp.Num() < 3) return;

    // 6) 속도(어깨폭 정규화)로 “가속 시작/멈춤” 시점 찾기
    double MeanSW = 0.0; for (auto& s : Smp) MeanSW += FMath::Max(1e-6, s.SW); MeanSW /= (double)Smp.Num();

    TArray<double> SpeedNorm; SpeedNorm.SetNum(Smp.Num());
    SpeedNorm[0] = 0.0;
    for (int i = 1; i < Smp.Num(); ++i)
    {
        const double dt = FMath::Max(1e-6, Smp[i].T - Smp[i - 1].T);
        const double v = (Smp[i].Wr - Smp[i - 1].Wr).Size() / dt;
        SpeedNorm[i] = v / FMath::Max(1e-6, MeanSW);
    }

    // 느슨한 히스테리시스
    const double EnterSpeedThresh = 0.80;
    const double ExitSpeedThresh = 0.40;
    const double HoldFastSec = 0.06;
    const double HoldStillSec = 0.06;

    auto FindRun = [&](int startIdx, double thresh, double holdSec, bool bAbove, int& outIdx)->bool
        {
            double acc = 0.0;
            for (int i = startIdx; i < Smp.Num(); ++i)
            {
                const bool pass = bAbove ? (SpeedNorm[i] >= thresh) : (SpeedNorm[i] <= thresh);
                const double dt = (i > 0) ? (Smp[i].T - Smp[i - 1].T) : 0.0;
                if (pass) { acc += dt; if (acc >= holdSec) { outIdx = i; return true; } }
                else acc = 0.0;
            }
            return false;
        };

    int iEnter = -1; if (!FindRun(1, EnterSpeedThresh, HoldFastSec, true, iEnter)) iEnter = 0;
    int iExit = -1; if (!FindRun(FMath::Max(1, iEnter + 1), ExitSpeedThresh, HoldStillSec, false, iExit)) iExit = Smp.Num() - 1;
    if (iExit <= iEnter) iExit = Smp.Num() - 1;

    // 7) 발사 방향: Enter→Exit (이미지 2D → 월드 YZ)
    const FVector2D P_enter = Smp[iEnter].Wr;
    const FVector2D P_exit = Smp[iExit].Wr;
    FVector2D DirImg = (P_exit - P_enter); if (!DirImg.Normalize()) return;

    const FTransform OwnerXf = GetOwner()->GetActorTransform();
    const FVector AxisY = OwnerXf.GetUnitAxis(EAxis::Y);
    const FVector AxisZ = OwnerXf.GetUnitAxis(EAxis::Z);
    const float y = DirImg.X;
    const float z = bInvertImageYToUp ? -DirImg.Y : DirImg.Y;
    const FVector Udir = (y * AxisY + z * AxisZ).GetSafeNormal();
    if (Udir.IsNearlyZero()) return;

    // 8) 손목 월드 위치(Enter/Exit) — ★Spawn은 Exit 손목에서!★
    FVector WristEnterW, WristExitW;
    if (!GetWristWorldOfSnap(Smp[iEnter].Snap, bRight, WristEnterW)) return;
    if (!GetWristWorldOfSnap(Smp[iExit].Snap, bRight, WristExitW)) return;

    // 9) 평면(스윕) 축: N = CamFwd × Udir, Z = N × Udir (회전 고정)
    FVector N = FVector::CrossProduct(CamFwd, Udir).GetSafeNormal();
    if (N.IsNearlyZero())
    {
        N = FVector::CrossProduct(FVector::UpVector, Udir).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::RightVector;
    }
    const FVector Z = FVector::CrossProduct(N, Udir).GetSafeNormal();
    const FMatrix M(
        FPlane(N.X, N.Y, N.Z, 0.f),      // X = 법선
        FPlane(Udir.X, Udir.Y, Udir.Z, 0.f), // Y = 진행
        FPlane(Z.X, Z.Y, Z.Z, 0.f),      // Z
        FPlane(0, 0, 0, 1)
    );
    const FQuat RotFixed(M);

    // 10) 스윕/발사
    const int32 TotalPlanes = FMath::Clamp(FMath::Max(3, MinInterpPlanes), MinInterpPlanes, MaxInterpPlanes);
    const FVector HalfExtent(SwingPlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    TSet<AActor*> UniqueActors;

    auto SpawnProjectile = [&](const FVector& SpawnLoc, const FVector& DirWorld)
        {
            if (!ProjectileClass) return;
            const FRotator Rot = DirWorld.Rotation();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            Params.Owner = GetOwner();
            Params.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;

            AActor* Proj = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, Rot, Params);
            if (!Proj) return;

            if (UProjectileMovementComponent* PMC = Proj->FindComponentByClass<UProjectileMovementComponent>())
            {
                PMC->Velocity = DirWorld * ProjectileSpeed;
            }
            else
            {
                Proj->SetActorLocation(SpawnLoc + DirWorld * (ProjectileSpeed * GetWorld()->GetDeltaSeconds()));
            }

            if (ProjectileLifeSeconds > 0.f) Proj->SetLifeSpan(ProjectileLifeSeconds);
        };

    // 손목 기반 보간(Enter→Exit)
    for (int32 k = 0; k < TotalPlanes; ++k)
    {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);
        const FVector WristK = FMath::Lerp(WristEnterW, WristExitW, t);

        // Overlap 평면 중심: 손목에서 카메라 전방 + 진행방향 반폭
        const FVector CenterK = WristK + CamFwd * PlaneDistance + Udir * HalfExtent.Y;
        OverlapPlaneOnce(CenterK, RotFixed, UniqueActors, DebugSweepDrawTime);

        if (bSpawnAtEachPlane && ProjectileClass)
        {
            // ★항상 Exit 손목에서 발사★ (요구사항)
            const FVector SpawnLoc = WristExitW + Udir * SpawnForwardOffset;
            SpawnProjectile(SpawnLoc, Udir);
        }

        // 디버그
        DrawDebugDirectionalArrow(GetWorld(), WristK, WristK + Udir * 80.f, 15.f, FColor::Green, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(GetWorld(), WristK, WristK + N * 80.f, 15.f, FColor::Magenta, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugSphere(GetWorld(), WristExitW, 4.f, 8, FColor::Yellow, false, DebugSweepDrawTime);
    }

    // 마지막 한 발만 쏘는 경우 — ★Exit 손목★
    if (!bSpawnAtEachPlane && ProjectileClass)
    {
        const FVector SpawnLoc = WristExitW + Udir * SpawnForwardOffset;
        SpawnProjectile(SpawnLoc, Udir);
    }

    if (UniqueActors.Num() == 0) return;

    // 11) 근접 데미지 일괄 적용
    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    TSubclassOf<UDamageType> DmgCls = DamageTypeClass;
    for (AActor* A : UniqueActors)
    {
        UGameplayStatics::ApplyDamage(A, SwingDamageAmount, Inst, GetOwner(), DmgCls);
    }
}
