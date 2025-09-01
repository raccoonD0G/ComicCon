// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PoseClassifierComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SplineComponent.h"

#include "Utils.h"

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
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);

    // 로컬 축(OBB)
    const FVector X = Rot.RotateVector(FVector::XAxisVector); // 법선(두께) 축
    // Center가 시작 면(-X face)이 되도록, 센터를 +X * 반두께 만큼 이동
    const FVector CenterFromStartFace = Center + X * HalfExtent.X;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SwingPlaneSweep), /*bTraceComplex*/ false, GetOwner());
    const bool bAny = GetWorld()->OverlapMultiByChannel(Overlaps, CenterFromStartFace, Rot, PlaneChannel, Shape, Params);

    if (bDrawDebugPlane)
    {
        DrawDebugBox(GetWorld(), CenterFromStartFace, HalfExtent, Rot, FColor::Cyan, false, DebugLifeTime, 0, 2.0f);

        // (선택) 시작 면 시각화: Center 위치와 -X면의 중심점 표시
        const FVector StartFaceCenter = Center; // 요청한 '시작 면' 위치
        DrawDebugPoint(GetWorld(), StartFaceCenter, 10.f, FColor::Yellow, false, DebugLifeTime);
        DrawDebugLine(GetWorld(), StartFaceCenter, StartFaceCenter + X * 50.f, FColor::Magenta, false, DebugLifeTime, 0, 2.f);
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

void UPoseClassifierComponent::ApplySwingPlaneSweepDamage(const TArray<const FTimedPoseSnapshot*>& Snaps, TSubclassOf<AActor> ProjectileClass, float ProjectileSpeed, float ProjectileLifeSeconds, float SpawnForwardOffset, bool bSpawnAtEachPlane)
{
    if (!GetWorld() || Snaps.Num() < 2) return;

    // 1) 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 2) 첫/끝 스냅샷으로 평면 축 + "손목 방향" + "연장 앵커" 추출
    auto MakePlaneFromSnap = [&](const FTimedPoseSnapshot* S,
        FVector& OutStartW,     // ★ Wrist+(Wrist-Elbow)*ratio
        FQuat& OutRot,
        FVector& OutU,
        FVector& OutDirToWrist) -> bool
        {
            int32 PersonIdx = INDEX_NONE;
            if (!PickBestPerson(S->Poses, PersonIdx)) return false;
            const FPersonPose& P = S->Poses[PersonIdx];
            if (P.XY.Num() < 17 || !GetOwner()) return false;

            // 기존 어깨→손목 방향/팔축 얻기 (어느 팔을 쓸지 방향 힌트)
            FVector StartW_tmp, DirW, UpperW;
            if (!GetExtendedArmWorld(P, StartW_tmp, DirW, UpperW)) return false; // DirW = Shoulder->Wrist (unit)

            // === 2D → 로컬 → 월드: 팔꿈치/손목 ===
            auto Finite2D = [](const FVector2f& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y); };

            const FVector2f& LHip = P.XY[11];
            const FVector2f& RHip = P.XY[12];
            if (!Finite2D(LHip) || !Finite2D(RHip)) return false;
            const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

            auto Map2DToLocal = [&](const FVector2f& Q)->FVector
                {
                    const FVector2f d = Q - Pelvis2D;
                    const float YY = d.X * PixelToUU;
                    const float ZZ = (bInvertImageYToUp ? -d.Y : d.Y) * PixelToUU;
                    return FVector(DepthOffsetX, YY, ZZ);
                };

            auto ToWorld = [&](const FVector& L)->FVector
                {
                    return GetOwner()->GetActorTransform().TransformPosition(L);
                };

            // 필요한 조인트 인덱스
            const int32 LEL = COCO_LEL, REL = COCO_REL, LWR = COCO_LWR, RWR = COCO_RWR;

            if (!(P.XY.IsValidIndex(LEL) && P.XY.IsValidIndex(REL) &&
                P.XY.IsValidIndex(LWR) && P.XY.IsValidIndex(RWR))) return false;

            const FVector2f LEl2 = P.XY[LEL], REl2 = P.XY[REL], LWr2 = P.XY[LWR], RWr2 = P.XY[RWR];
            if (!(Finite2D(LEl2) && Finite2D(REl2) && Finite2D(LWr2) && Finite2D(RWr2))) return false;

            // 로컬/월드
            const FVector LElW = ToWorld(Map2DToLocal(LEl2));
            const FVector RElW = ToWorld(Map2DToLocal(REl2));
            const FVector LWw = ToWorld(Map2DToLocal(LWr2));
            const FVector RWw = ToWorld(Map2DToLocal(RWr2));

            // 어느 손을 사용할지: DirW와 더 잘 정렬된 손목 선택
            const FVector vL = (LWw - StartW_tmp).GetSafeNormal();
            const FVector vR = (RWw - StartW_tmp).GetSafeNormal();
            const float dL = FVector::DotProduct(DirW, vL);
            const float dR = FVector::DotProduct(DirW, vR);
            const bool bUseLeft = (dL >= dR);

            const FVector WristW = bUseLeft ? LWw : RWw;
            const FVector ElbowW = bUseLeft ? LElW : RElW;

            // ★ 팔꿈치→손목 25% 연장 지점
            const float ExtendRatio = 0.25f;
            const FVector Forearm = (WristW - ElbowW);
            const FVector Anchor = WristW + Forearm * ExtendRatio;

            // 평면 축 구성 (기존과 동일)
            FVector N = FVector::CrossProduct(CamFwd, UpperW).GetSafeNormal();
            if (N.IsNearlyZero())
            {
                N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
                if (N.IsNearlyZero()) N = FVector::RightVector;
            }
            FVector U = (UpperW - FVector::DotProduct(UpperW, N) * N).GetSafeNormal();
            if (U.IsNearlyZero())
                U = (CamFwd - FVector::DotProduct(CamFwd, N) * N).GetSafeNormal();

            FVector Z = FVector::CrossProduct(N, U).GetSafeNormal();
            if (Z.IsNearlyZero()) Z = FVector::UpVector;

            const FMatrix M(
                FPlane(N.X, N.Y, N.Z, 0.f),   // X = 평면 법선
                FPlane(U.X, U.Y, U.Z, 0.f),   // Y = 평면 내 진행축(팔축)
                FPlane(Z.X, Z.Y, Z.Z, 0.f),   // Z
                FPlane(0, 0, 0, 1)
            );

            OutStartW = Anchor;                // ★ 이제 어깨가 아니라 연장 손목 지점
            OutRot = FQuat(M);
            OutU = U;
            OutDirToWrist = DirW.GetSafeNormal();  // (발사 기본 방향 유지: 어깨→손목)
            if (OutDirToWrist.IsNearlyZero()) OutDirToWrist = U;

            return true;
        };

    FVector Start0, Start1;  FQuat Rot0, Rot1;  FVector U0, U1;
    FVector DirW0, DirW1; // 어깨→손목 방향(월드)
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

    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    TSet<AActor*> UniqueActors;

    // ★★★ CHANGED: 발사 유틸 - 클래스 인자를 받도록 일반화
    auto SpawnProjectile = [&](TSubclassOf<AActor> InClass, const FVector& SpawnLoc, const FVector& DirWorld)
        {
            if (!InClass) return;

            const FRotator Rot = DirWorld.Rotation();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.Owner = GetOwner();
            Params.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;

            AActor* Proj = GetWorld()->SpawnActor<AActor>(InClass, SpawnLoc, Rot, Params);
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

    TArray<FVector> ArcPoints;
    ArcPoints.Reserve(TotalPlanes);

    // ★★★ NEW: U(노란 화살표)와 시작점 누적값
    FVector SumU = FVector::ZeroVector;
    FVector SumStart = FVector::ZeroVector;
    int32   SumCount = 0;

    // 메인 루프
    for (int32 k = 0; k < TotalPlanes; ++k)
    {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);
        const FQuat RotK = FQuat::Slerp(Rot0, Rot1, t).GetNormalized();

        const FVector N = RotK.GetAxisX();
        const FVector U = RotK.GetAxisY();          // ← 노란 화살표
        const FVector StartK = FMath::Lerp(Start0, Start1, t);

        const FVector DirK = ((1.f - t) * DirW0 + t * DirW1).GetSafeNormal();
        const FVector FireDir = DirK.IsNearlyZero() ? U : DirK;

        const FVector CenterK = StartK + U * HalfExtent.Y;

        OverlapPlaneOnce(CenterK, RotK, UniqueActors, DebugSweepDrawTime);

        // per-plane 발사(기존 유지): ProjectileClass 사용
        if (bSpawnAtEachPlane && ProjectileClass)
        {
            const FVector SpawnLoc = StartK + FireDir * SpawnForwardOffset;
            SpawnProjectile(ProjectileClass, SpawnLoc, FireDir);
        }

        // 디버그 화살표(기존)
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + FireDir * 80.f, 15.f, FColor::Green, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + U * 80.f, 15.f, FColor::Yellow, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(GetWorld(), StartK, StartK + N * 80.f, 15.f, FColor::Magenta, false, DebugSweepDrawTime, 0, 1.5f);

        ArcPoints.Add(StartK);

        // ★★★ NEW: 평균 계산용 누적
        SumU += U;
        SumStart += StartK;
        ++SumCount;
    }

    // ★★★ CHANGED: 마지막 한 발은 "노란 화살표(U)의 평균"으로 SwingProjectileClass 발사
    if (!bSpawnAtEachPlane && SwingProjectileClass && SumCount > 0)
    {
        FVector AvgU = SumU / float(SumCount);
        if (AvgU.IsNearlyZero())
        {
            AvgU = U1; // 안전망: 마지막 U로 대체
        }
        AvgU = AvgU.GetSafeNormal();

        const FVector AvgStart = SumStart / float(SumCount);
        const FVector SpawnLoc = AvgStart + AvgU * SpawnForwardOffset;

        SpawnProjectile(SwingProjectileClass, SpawnLoc, AvgU);
    }

    if (UniqueActors.Num() == 0) return;

    // 4) 근접 데미지 일괄 적용 (기존 동일)
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
    TArray<FArrowSample> Smp; Smp.Reserve(WindowBuffer.Num());

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

    // 1) 시작: "가까운 상태"가 윈도우에 반드시 존재
    float MinWD = FLT_MAX;
    float MaxWD = 0.f;
    double TMin = 0.0, TMax = 0.0;
    bool bSawClose = false;

    for (const auto& a : Smp)
    {
        if (a.Close) bSawClose = true;
        if (a.WristDist < MinWD) { MinWD = a.WristDist; TMin = a.T; }
        if (a.WristDist > MaxWD) { MaxWD = a.WristDist; TMax = a.T; }
    }
    if (!bSawClose) return;

    // 2) 어깨폭 정규화
    double MeanSW = 0.0;
    for (const auto& a : Smp) MeanSW += (double)a.ShoulderW;
    MeanSW /= (double)Smp.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    const double MinNorm = (double)MinWD / MeanSW;
    const double MaxNorm = (double)MaxWD / MeanSW;
    const double DeltaNorm = MaxNorm - MinNorm;

    // 3) 멀어짐 크기/증가량/속도
    const bool bFarEnough = (MaxNorm >= (double)ArrowFarRatio);
    const bool bDeltaEnough = (DeltaNorm >= (double)ArrowMinDeltaNorm);

    double SpeedNorm = 0.0;
    if (TMax > TMin)
    {
        const double dt = TMax - TMin;
        SpeedNorm = DeltaNorm / dt; // (어깨폭/초)
    }
    const bool bFastEnough = (SpeedNorm >= (double)ArrowMinSpeedNorm);

    if (!(bFarEnough && bDeltaEnough && bFastEnough)) return;

    // -------- 여기서부터 "어깨/손목 월드 포지션" 산출 --------
    const FTimedPoseSnapshot& LastSnap = WindowBuffer.Last();
    int32 BestIdx = INDEX_NONE;
    if (!PickBestPerson(LastSnap.Poses, BestIdx)) return;
    const FPersonPose& P = LastSnap.Poses[BestIdx];
    if (P.XY.Num() < 17) return;

    // 양쪽 어깨/손목 2D
    if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH) &&
        P.XY.IsValidIndex(COCO_LWR) && P.XY.IsValidIndex(COCO_RWR))) return;

    const FVector2f LSH = P.XY[COCO_LSH];
    const FVector2f RSH = P.XY[COCO_RSH];
    const FVector2f LWR = P.XY[COCO_LWR];
    const FVector2f RWR = P.XY[COCO_RWR];
    if (!IsFinite2D(LSH) || !IsFinite2D(RSH) || !IsFinite2D(LWR) || !IsFinite2D(RWR)) return;

    // 어느 팔이 더 “쭉 뻗었는지” (픽셀 기준: 어깨-손목 거리)
    const double LArmLenPx = FVector2D::Distance(FVector2D(LSH.X, LSH.Y), FVector2D(LWR.X, LWR.Y));
    const double RArmLenPx = FVector2D::Distance(FVector2D(RSH.X, RSH.Y), FVector2D(RWR.X, RWR.Y));
    const bool bUseRight = (RArmLenPx >= LArmLenPx);

    // 월드 변환
    FVector2f Pelvis2D;
    if (!GetPelvis2D(P, Pelvis2D)) return;

    const FVector ShoulderW = ToWorldFrom2D(Pelvis2D, bUseRight ? RSH : LSH, GetOwner()->GetActorTransform(), PixelToUU, bInvertImageYToUp, DepthOffsetX);
    const FVector WristW = ToWorldFrom2D(Pelvis2D, bUseRight ? RWR : LWR, GetOwner()->GetActorTransform(), PixelToUU, bInvertImageYToUp, DepthOffsetX);

    // 평면 데미지: 어깨가 중심, Y축은 어깨→손목 (ApplyArrowPlaneDamage의 어깨/손목 버전 사용)
    ApplyArrowPlaneDamage(ShoulderW, WristW);

    LastArrowLoggedMs = Now;
    WindowBuffer.Reset();
}

void UPoseClassifierComponent::ApplyArrowPlaneDamage(const FVector& ShoulderW, const FVector& WristW)
{
    if (!GetWorld()) return;

    // 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 어깨→손목 방향 (쭉 뻗은 팔의 실제 방향)
    FVector ArmDir = (WristW - ShoulderW).GetSafeNormal();
    if (ArmDir.IsNearlyZero())
    {
        // 폴백: 카메라 전방
        ArmDir = CamFwd.GetSafeNormal();
        if (ArmDir.IsNearlyZero()) ArmDir = FVector::ForwardVector;
    }

    // 법선 = CamFwd × ArmDir  (둘 다에 수직)
    FVector N = FVector::CrossProduct(CamFwd, ArmDir).GetSafeNormal();
    if (N.IsNearlyZero())
    {
        // 폴백: 카메라 전방과 월드업
        N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::RightVector;
    }

    // 평면 내 Z축 = N × ArmDir
    FVector Z = FVector::CrossProduct(N, ArmDir).GetSafeNormal();
    if (Z.IsNearlyZero()) Z = FVector::UpVector;

    // 회전행렬 → 쿼터니언 (X=N, Y=ArmDir, Z=N×Y)
    const FQuat Rot(FMatrix(
        FPlane(N.X, N.Y, N.Z, 0.f),
        FPlane(ArmDir.X, ArmDir.Y, ArmDir.Z, 0.f),
        FPlane(Z.X, Z.Y, Z.Z, 0.f),
        FPlane(0, 0, 0, 1)
    ));

    // 박스 형상
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);

    // 어깨가 중심
    const FVector Center = ShoulderW;

    // 오버랩 1회 (공유 헬퍼)
    TSet<AActor*> UniqueActors;
    OverlapPlaneOnce(Center, Rot, UniqueActors, /*DebugLifeTime=*/1.0f);

    // 디버그(선택)
    if (bDrawDebugPlane)
    {
        DrawDebugDirectionalArrow(GetWorld(), Center, Center + CamFwd * 120.f, 20.f, FColor::Blue, false, 1.0f, 0, 5.0f); // 카메라
        DrawDebugDirectionalArrow(GetWorld(), Center, Center + ArmDir * 120.f, 20.f, FColor::Yellow, false, 1.0f, 0, 5.0f); // 어깨→손목(Y)
        DrawDebugDirectionalArrow(GetWorld(), Center, Center + N * 120.f, 20.f, FColor::Magenta, false, 1.0f, 0, 5.0f); // 법선(X)

        DrawDebugSphere(GetWorld(), ShoulderW, 6.f, 12, FColor::Green, false, 1.0f); // 실제 어깨
        DrawDebugSphere(GetWorld(), Center, 6.f, 12, FColor::Cyan, false, 1.0f); // 보정된 센터
    }

    if (UniqueActors.Num() == 0) return;

    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    for (AActor* A : UniqueActors)
    {
        if (A && A != GetOwner())
        {
            UGameplayStatics::ApplyDamage(A, ArrowDamageAmount, Inst, GetOwner(), DamageTypeClass);
        }
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
        if ((Now - LastSwingLoggedMs) < CoolMs) return;
    }

    // 최근 구간만
    const double UseSec = FMath::Clamp((double)SwingRecentSeconds, 0.2, (double)WindowSeconds);
    const uint64 RecentOldest = (Now > (uint64)(UseSec * 1000.0)) ? (Now - (uint64)(UseSec * 1000.0)) : 0;

    // 스냅샷 수집
    TArray<const FTimedPoseSnapshot*> Snaps;
    Snaps.Reserve(WindowBuffer.Num());
    for (const auto& S : WindowBuffer) {
        if (S.TimestampMs >= RecentOldest && S.Poses.Num() > 0)
            Snaps.Add(&S);
    }
    if (Snaps.Num() < 3) return;

    // ---- 샘플 구조 ----
    struct FHandSample {
        double    T = 0.0;
        FVector2D L = FVector2D::ZeroVector; // 왼손목(px)
        FVector2D R = FVector2D::ZeroVector; // 오른손목(px)
        FVector2D C = FVector2D::ZeroVector; // 어깨중점(px)
        float     ShoulderW = 0.f;
        bool      Open = false;              // 손이 충분히 떨어짐
    };
    auto IsFinite2D = [](const FVector2f& p)->bool { return FMath::IsFinite(p.X) && FMath::IsFinite(p.Y); };

    // 샘플 추출
    TArray<FHandSample> Samples; Samples.Reserve(Snaps.Num());
    for (const FTimedPoseSnapshot* S : Snaps) {
        int32 idx = INDEX_NONE;
        if (!PickBestPerson(S->Poses, idx)) continue;
        const FPersonPose& P = S->Poses[idx];
        if (P.XY.Num() < 17) continue;

        const FVector2f& LWR = P.XY[COCO_LWR];
        const FVector2f& RWR = P.XY[COCO_RWR];
        if (!IsFinite2D(LWR) || !IsFinite2D(RWR)) continue;

        if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH))) continue;
        const FVector2f& LSH = P.XY[COCO_LSH];
        const FVector2f& RSH = P.XY[COCO_RSH];
        if (!IsFinite2D(LSH) || !IsFinite2D(RSH)) continue;

        const FVector2D C = 0.5 * FVector2D(LSH.X + RSH.X, LSH.Y + RSH.Y);

        float ShoulderW = 0.f, WristDist = 0.f;
        const bool bClose = IsHandsClose(P, ShoulderW, WristDist);
        if (ShoulderW <= KINDA_SMALL_NUMBER) continue;

        FHandSample smp;
        smp.T = (double)S->TimestampMs * 0.001;
        smp.L = FVector2D(LWR.X, LWR.Y);
        smp.R = FVector2D(RWR.X, RWR.Y);
        smp.C = C;
        smp.ShoulderW = ShoulderW;
        smp.Open = !bClose; // “충분히 떨어짐”
        Samples.Add(MoveTemp(smp));
    }
    if (Samples.Num() < 3) return;

    // 고정 Hz 리샘플 + EMA 스무딩
    const double FixedHz = 30.0, FixedDt = 1.0 / FixedHz;
    auto Resample = [&](const TArray<FHandSample>& In, double Hz, TArray<FHandSample>& Out) {
        Out.Reset(); if (In.Num() < 2) return;
        const double t0 = In[0].T, t1 = In.Last().T;
        const int N = FMath::Max(2, (int)FMath::RoundToInt((t1 - t0) * Hz) + 1);
        int j = 0; Out.Reserve(N);
        for (int i = 0; i < N; ++i) {
            const double t = t0 + i * (1.0 / Hz);
            while (j + 1 < In.Num() && In[j + 1].T < t) ++j;
            const int k = FMath::Min(j + 1, In.Num() - 1);
            const double tA = In[j].T, tB = In[k].T;
            const double a = (tB > tA) ? FMath::Clamp((t - tA) / (tB - tA), 0.0, 1.0) : 0.0;
            FHandSample o;
            o.T = t;
            o.L = FMath::Lerp(In[j].L, In[k].L, a);
            o.R = FMath::Lerp(In[j].R, In[k].R, a);
            o.C = FMath::Lerp(In[j].C, In[k].C, a);
            o.ShoulderW = FMath::Lerp(In[j].ShoulderW, In[k].ShoulderW, a);
            o.Open = (a < 0.5) ? In[j].Open : In[k].Open;
            Out.Add(o);
        }
        };
    TArray<FHandSample> U; Resample(Samples, FixedHz, U);
    if (U.Num() < 3) return;

    auto SmoothEMA = [&](TArray<FHandSample>& A, double Alpha) {
        if (A.Num() == 0) return;
        FVector2D l = A[0].L, r = A[0].R, c = A[0].C;
        for (int i = 0; i < A.Num(); ++i) { l = (1 - Alpha) * l + Alpha * A[i].L; r = (1 - Alpha) * r + Alpha * A[i].R; c = (1 - Alpha) * c + Alpha * A[i].C; A[i].L = l; A[i].R = r; A[i].C = c; }
        };
    SmoothEMA(U, 0.35);

    // Open coverage (두 손이 멀리 떨어진 상태 비율)
    int32 OpenCount = 0; for (auto& s : U) if (s.Open) ++OpenCount;
    const double OpenCoverage = (double)OpenCount / FMath::Max(1, U.Num());
    if (OpenCoverage < MinOpenCoverage) return;

    // 평균 어깨폭
    double MeanSW = 0.0; for (auto& s : U) MeanSW += (double)FMath::Max(s.ShoulderW, KINDA_SMALL_NUMBER);
    MeanSW /= (double)U.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    // === 속도 기반 메트릭만 사용(반경/바깥 경로/반전 제거) ===
    struct FHandMetrics { double Peak = 0.0; double Avg = 0.0; };
    auto Metrics = [&](bool bLeft)->FHandMetrics {
        FHandMetrics M{};
        double used = 0.0, path = 0.0;
        for (int i = 1; i < U.Num(); ++i) {
            if (!U[i].Open) continue;                    // “멀리 떨어진 상태”에서만 측정
            const FVector2D p0 = bLeft ? U[i - 1].L : U[i - 1].R;
            const FVector2D p1 = bLeft ? U[i].L : U[i].R;
            const double v = (p1 - p0).Size() / FixedDt / MeanSW; // 속도(어깨폭 정규화)
            if (v > M.Peak) M.Peak = v;
            path += (p1 - p0).Size() / MeanSW;           // 경로 길이(정규화)
            used += FixedDt;
        }
        if (used > 0.0) M.Avg = path / used;
        return M;
        };
    const FHandMetrics ML = Metrics(true);
    const FHandMetrics MR = Metrics(false);

    // 최종 손 확정 (Auto일 때는 빠른 손)
    bool bRightHandFinal = false;
    switch (WhichHand) {
    case EMainHand::Left:  bRightHandFinal = false; break;
    case EMainHand::Right: bRightHandFinal = true;  break;
    default: {
        const bool bLeftActive = (ML.Peak > MR.Peak) || (FMath::IsNearlyEqual(ML.Peak, MR.Peak, 1e-3) && ML.Avg >= MR.Avg);
        bRightHandFinal = !bLeftActive;
    } break;
    }

    // === 최종 판정: “손이 떨어진 상태(Open) + 한 손이 빠르게 움직임(피크/평균)” ===
    const FHandMetrics& M = bRightHandFinal ? MR : ML;
    const bool bPass =
        (OpenCoverage >= MinOpenCoverage) &&
        ((M.Avg >= MinAvgSpeedNorm) || (M.Peak >= MinPeakSpeedNorm));
    if (!bPass) return;

    // ---- 스냅별 대표 PersonIdx 고정(한 번) ----
    TArray<int32> PersonIdxOf; PersonIdxOf.SetNum(Snaps.Num());
    for (int32 i = 0; i < Snaps.Num(); ++i) { int32 idx = INDEX_NONE; PickBestPerson(Snaps[i]->Poses, idx); PersonIdxOf[i] = idx; }

    // ---- 가속 시작/종료 인덱스 (선택 손 기준, 히스테리시스) ----
    TArray<double> SpeedNorm; SpeedNorm.Init(0.0, U.Num());
    auto WristAt = [&](int i) { return bRightHandFinal ? U[i].R : U[i].L; };
    for (int i = 1; i < U.Num(); ++i) {
        const FVector2D d = WristAt(i) - WristAt(i - 1);
        SpeedNorm[i] = (d.Size() / FixedDt) / MeanSW;
    }
    const double EnterSpeed = 0.80, ExitSpeed = 0.40, HoldFast = 0.06, HoldStill = 0.06;
    auto FindRun = [&](int s, double thr, double hold, bool above, int& out)->bool {
        double acc = 0; for (int i = s; i < U.Num(); ++i) {
            const bool pass = above ? (SpeedNorm[i] >= thr) : (SpeedNorm[i] <= thr);
            const double dt = (i > 0) ? FixedDt : 0.0; if (pass) { acc += dt; if (acc >= hold) { out = i; return true; } }
            else acc = 0;
        }
        return false;
        };
    int iEnter = -1; if (!FindRun(1, EnterSpeed, HoldFast, true, iEnter)) iEnter = 0;
    int iExit = -1; if (!FindRun(FMath::Max(1, iEnter + 1), ExitSpeed, HoldStill, false, iExit)) iExit = U.Num() - 1;
    const int iEnterAfter = FMath::Clamp(iEnter + 1, 0, U.Num() - 1);
    const int iExitAfter = FMath::Clamp(iExit + 1, 0, U.Num() - 1);

    // ---- Apply ----
    ApplySingleSwingDamage(
        Snaps,
        PersonIdxOf,
        bRightHandFinal,
        iEnterAfter,
        iExitAfter,
        AmuletProjectileClass,
        1000.f,
        3.0f,
        true
    );

    LastSwingLoggedMs = Now;
    WindowBuffer.Reset();
}

void UPoseClassifierComponent::ApplySingleSwingDamage(const TArray<const FTimedPoseSnapshot*>& Snaps,const TArray<int32>& PersonIdxOf, bool bRightHand, int32 EnterIdx, int32 ExitIdx, TSubclassOf<AActor> ProjectileClass, float ProjectileSpeed, float ProjectileLifeSeconds, bool bSpawnAtEachPlane)
{
    if (!GetWorld() || Snaps.Num() < 3) return;
    if (PersonIdxOf.Num() != Snaps.Num()) return;

    FVector CamLoc, CamFwd; if (!GetCameraView(CamLoc, CamFwd)) return;
    const FTransform OwnerXf = GetOwner()->GetActorTransform();
    const FVector AxisY = OwnerXf.GetUnitAxis(EAxis::Y);
    const FVector AxisZ = OwnerXf.GetUnitAxis(EAxis::Z);

    const int32 IdxWr = bRightHand ? COCO_RWR : COCO_LWR;
    const int32 IdxEl = bRightHand ? COCO_REL : COCO_LEL;

    // Enter/Exit 안전 클램프
    EnterIdx = FMath::Clamp(EnterIdx, 0, Snaps.Num() - 1);
    ExitIdx = FMath::Clamp(ExitIdx, 0, Snaps.Num() - 1);

    // Enter/Exit 손목 2D → 이동 방향
    auto PickWr2D = [&](int i, FVector2D& Out)->bool {
        const int32 p = PersonIdxOf[i]; if (p == INDEX_NONE) return false;
        const FPersonPose& P = Snaps[i]->Poses[p];
        if (!P.XY.IsValidIndex(IdxWr) || !IsFinite2D(P.XY[IdxWr])) return false;
        Out = FVector2D(P.XY[IdxWr].X, P.XY[IdxWr].Y); return true;
        };
    FVector2D WrEnter2D, WrExit2D; if (!PickWr2D(EnterIdx, WrEnter2D) || !PickWr2D(ExitIdx, WrExit2D)) return;
    FVector2D DirImg = (WrExit2D - WrEnter2D);
    if (!DirImg.Normalize()) return;

    const float y = DirImg.X;
    const float z = bInvertImageYToUp ? -DirImg.Y : DirImg.Y;
    const FVector DirMove = (y * AxisY + z * AxisZ).GetSafeNormal();
    if (DirMove.IsNearlyZero()) return;

    // Exit에서 손목/팔꿈치 월드
    const int32 pExit = PersonIdxOf[ExitIdx]; if (pExit == INDEX_NONE) return;
    const FPersonPose& PExit = Snaps[ExitIdx]->Poses[pExit];

    FVector2f Pelvis2D_Exit; if (!GetPelvis2D(PExit, Pelvis2D_Exit)) return;
    if (!PExit.XY.IsValidIndex(IdxWr) || !IsFinite2D(PExit.XY[IdxWr])) return;

    const FVector WristExitW = ToWorldFrom2D(Pelvis2D_Exit, PExit.XY[IdxWr], OwnerXf, PixelToUU, bInvertImageYToUp, DepthOffsetX);
    if (!PExit.XY.IsValidIndex(IdxEl) || !IsFinite2D(PExit.XY[IdxEl])) return;
    const FVector ElbowExitW = ToWorldFrom2D(Pelvis2D_Exit, PExit.XY[IdxEl], OwnerXf, PixelToUU, bInvertImageYToUp, DepthOffsetX);

    const FVector ForearmExit = (WristExitW - ElbowExitW);
    const float   ForearmLen = ForearmExit.Size(); if (ForearmLen <= KINDA_SMALL_NUMBER) return;
    const float ForearmExtendRatio = 0.25f;
    const FVector SpawnLoc = WristExitW + ForearmExit.GetSafeNormal() * (ForearmLen * ForearmExtendRatio);

    // 고정 평면 회전 (N=CamFwd×Dir, Z=N×Dir)
    FVector N = FVector::CrossProduct(CamFwd, DirMove).GetSafeNormal();
    if (N.IsNearlyZero()) { N = FVector::CrossProduct(FVector::UpVector, DirMove).GetSafeNormal(); if (N.IsNearlyZero()) N = FVector::RightVector; }
    const FVector Zaxis = FVector::CrossProduct(N, DirMove).GetSafeNormal();
    const FQuat RotFixed = FQuat(FMatrix(
        FPlane(N.X, N.Y, N.Z, 0.f),
        FPlane(DirMove.X, DirMove.Y, DirMove.Z, 0.f),
        FPlane(Zaxis.X, Zaxis.Y, Zaxis.Z, 0.f),
        FPlane(0, 0, 0, 1)
    ));

    // Enter 손목 월드
    FVector WristEnterW = WristExitW;
    {
        const int32 pEnter = PersonIdxOf[EnterIdx];
        if (pEnter != INDEX_NONE) {
            const FPersonPose& PEnter = Snaps[EnterIdx]->Poses[pEnter];
            FVector2f Pelvis2D_Enter;
            if (GetPelvis2D(PEnter, Pelvis2D_Enter) && PEnter.XY.IsValidIndex(IdxWr) && IsFinite2D(PEnter.XY[IdxWr])) {
                WristEnterW = ToWorldFrom2D(Pelvis2D_Enter, PEnter.XY[IdxWr], OwnerXf, PixelToUU, bInvertImageYToUp, DepthOffsetX);
            }
        }
    }

    // 스윕 & 스폰
    const int32 TotalPlanes = FMath::Clamp(FMath::Max(3, MinInterpPlanes), MinInterpPlanes, MaxInterpPlanes);
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    TSet<AActor*> UniqueActors;

    auto SpawnProjectileOnce = [&](const FVector& L, const FVector& D) {
        if (!ProjectileClass) return;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = GetOwner();
        Params.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;
        if (AActor* Proj = GetWorld()->SpawnActor<AActor>(ProjectileClass, L, D.Rotation(), Params)) {
            if (UProjectileMovementComponent* PMC = Proj->FindComponentByClass<UProjectileMovementComponent>()) {
                PMC->Velocity = D * ProjectileSpeed;
            }
            else {
                Proj->SetActorLocation(L + D * (ProjectileSpeed * GetWorld()->GetDeltaSeconds()));
            }
            if (ProjectileLifeSeconds > 0.f) Proj->SetLifeSpan(ProjectileLifeSeconds);
        }
        };

    if (!bSpawnAtEachPlane && ProjectileClass) {
        SpawnProjectileOnce(SpawnLoc, DirMove);
    }

    for (int32 k = 0; k < TotalPlanes; ++k) {
        const float t = (TotalPlanes == 1) ? 0.f : (float)k / (float)(TotalPlanes - 1);
        const FVector WristK = FMath::Lerp(WristEnterW, WristExitW, t);
        const FVector SpawnOffset = ForearmExit.GetSafeNormal() * (ForearmLen * ForearmExtendRatio);
        const FVector CenterK = WristK + DirMove * HalfExtent.Y + SpawnOffset;

        OverlapPlaneOnce(CenterK, RotFixed, UniqueActors, DebugSweepDrawTime);

        DrawDebugDirectionalArrow(GetWorld(), WristK, WristK + DirMove * 80.f, 15.f, FColor::Green, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugDirectionalArrow(GetWorld(), WristK, WristK + N * 80.f, 15.f, FColor::Magenta, false, DebugSweepDrawTime, 0, 1.5f);
        DrawDebugSphere(GetWorld(), SpawnLoc, 4.f, 8, FColor::Yellow, false, DebugSweepDrawTime);

        if (bSpawnAtEachPlane && ProjectileClass) {
            SpawnProjectileOnce(SpawnLoc, DirMove);
        }
    }

    if (UniqueActors.Num() == 0) return;

    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    for (AActor* A : UniqueActors) {
        UGameplayStatics::ApplyDamage(A, SwingDamageAmount, Inst, GetOwner(), DamageTypeClass);
    }
}