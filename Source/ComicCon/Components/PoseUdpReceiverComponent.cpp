// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PoseUdpReceiverComponent.h"
#include "HAL/RunnableThread.h"
#include "Common/UdpSocketBuilder.h"
#include "DrawDebugHelpers.h"
#include "Utils.h"

// COCO skeleton edges
static const int32 EDGES[][2] = {
    {0,1},{0,2},{1,3},{2,4},
    {5,6},
    {5,7},{7,9},
    {6,8},{8,10},
    {11,12},
    {5,11},{6,12},
    {11,13},{13,15},
    {12,14},{14,16}
};
static const int32 NUM_EDGES = sizeof(EDGES) / sizeof(EDGES[0]);

// 시각화 파라미터(원하면 UPROPERTY로 빼서 에디터에서 조정)
const float LineThickness = 2.f;
const float PointRadius = 5.0f;
const FColor LineColor = FColor::Yellow;
const FColor PointColor = FColor::White;

static const int32 HEADER_SIZE = 18; // 4+1+1+2+2+8

// v2 hand block: hand_id(H=2) + handed(B=1) + score(f=4) + (21*2*4=168) + (21*4=84) = 259
static const int32 EXPECTED_HAND_BLOCK = 259;

// 사람 블록은 동일 (pid 2 + 34 floats + 17 floats = 206)
static const int32 EXPECTED_PERSON_BLOCK = 2 + (34 * 4) + (17 * 4); // 206

// 포맷 버전
enum : uint8 { POSE_VERSION_V1 = 1, POSE_VERSION_V2 = 2 };

UPoseUdpReceiverComponent::UPoseUdpReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPoseUdpReceiverComponent::BeginPlay()
{
    Super::BeginPlay();
    RecvBuffer.SetNumUninitialized(2048);
    InitSocket(7777);
}

void UPoseUdpReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetWorld() || !GetOwner()) return;

    TArray<FPersonPose> Poses;
    TArray<FHandPose>   Hands;
    uint64 Ts = 0;

    // 패킷 여러 개 처리
    while (ReceiveOnce(Poses, Hands, Ts))
    {
        LatestPoses = Poses;
        LatestHands = Hands;
        LatestTimestamp = Ts;

        const FTransform& OwnerXform = GetOwner()->GetActorTransform();

        // ===== Pelvis2DByPid 구성 (디버그/변환용) =====
        TMap<uint16, FVector2f> Pelvis2DByPid;
        Pelvis2DByPid.Reserve(Poses.Num());

        for (const FPersonPose& Pose : Poses)
        {
            if (Pose.XY.Num() < 17) continue;
            const FVector2f& LHip = Pose.XY[11];
            const FVector2f& RHip = Pose.XY[12];
            if (!(IsFinite2D(LHip) && IsFinite2D(RHip))) continue;

            const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;
            Pelvis2DByPid.Add(Pose.PersonId, Pelvis2D);
        }

        // Fallback pelvis (있으면 아무거나 하나)
        FVector2f FallbackPelvis2D = FVector2f::ZeroVector;
        if (Pelvis2DByPid.Num() > 0)
        {
            for (const auto& It : Pelvis2DByPid) { FallbackPelvis2D = It.Value; break; }
        }

        // ===== 바디 디버그 드로잉 (per person) =====
        for (const FPersonPose& Pose : Poses)
        {
            if (Pose.XY.Num() < 17) continue;

            const FVector2f* PelvisPtr = Pelvis2DByPid.Find(Pose.PersonId);
            if (!PelvisPtr) continue;
            const FVector2f Pelvis2D = *PelvisPtr;

            // ★ 삭제: 틱 내 다중 호출 방지 위해 여기서 브로드캐스트 하지 않음
            // OnPoseBufferChanged.Broadcast(Pelvis2D, Poses, PixelToUU, OwnerXform);

            // (디버그 라인/포인트 그리기 부분은 동일)
            for (int32 e = 0; e < NUM_EDGES; ++e)
            {
                const int32 A = EDGES[e][0];
                const int32 B = EDGES[e][1];
                if (!Pose.XY.IsValidIndex(A) || !Pose.XY.IsValidIndex(B)) continue;

                const FVector2f& Pa2D = Pose.XY[A];
                const FVector2f& Pb2D = Pose.XY[B];
                if (!(IsFinite2D(Pa2D) && IsFinite2D(Pb2D))) continue;

                const FVector La = MakeLocalFrom2D(Pa2D, Pelvis2D, PixelToUU, DepthOffsetX);
                const FVector Lb = MakeLocalFrom2D(Pb2D, Pelvis2D, PixelToUU, DepthOffsetX);

                const FVector Wa = OwnerXform.TransformPosition(La);
                const FVector Wb = OwnerXform.TransformPosition(Lb);

                // DrawDebugLine(GetWorld(), Wa, Wb, LineColor, false, 0.f, 0, LineThickness);
            }

            for (int32 k = 0; k < 17; ++k)
            {
                const FVector2f& P2D = Pose.XY[k];
                if (!IsFinite2D(P2D)) continue;

                const FVector Lp = MakeLocalFrom2D(P2D, Pelvis2D, PixelToUU, DepthOffsetX);
                const FVector Wp = OwnerXform.TransformPosition(Lp);

                // DrawDebugSphere(GetWorld(), Wp, PointRadius, 8, PointColor, false, 0.f, 0, 0.5f);
            }
        }

        // ===== 손 디버그 드로잉 =====
        for (const FHandPose& H : Hands)
        {
            FVector2f Pelvis2D = FallbackPelvis2D;
            if (H.PersonId != 0xFFFF)
            {
                if (const FVector2f* Found = Pelvis2DByPid.Find(H.PersonId))
                {
                    Pelvis2D = *Found;
                }
            }

            for (int32 k = 0; k < H.XY.Num(); ++k)
            {
                const FVector2f& P2D = H.XY[k];
                if (!IsFinite2D(P2D)) continue;

                const FVector Lp = MakeLocalFrom2D(P2D, Pelvis2D, PixelToUU, DepthOffsetX);
                const FVector Wp = OwnerXform.TransformPosition(Lp);

                const FColor C =
                    (H.Which == 0) ? FColor(0, 200, 255) :
                    (H.Which == 1) ? FColor(255, 200, 0) :
                    FColor(200, 200, 200);

                DrawDebugSphere(GetWorld(), Wp, PointRadius, 8, C, false, 0.f, 0, 0.5f);
            }

            if (IsFinite2D(H.Center))
            {
                const FVector Lc = MakeLocalFrom2D(H.Center, Pelvis2D, PixelToUU, DepthOffsetX);
                const FVector Wc = OwnerXform.TransformPosition(Lc);
                DrawDebugSphere(GetWorld(), Wc, PointRadius * 1.5f, 10, FColor::Cyan, false, 0.f, 0, 0.8f);
            }
        }
    }

    const FTransform& OwnerXformFinal = GetOwner()->GetActorTransform();

    // ---- Pose(바디) 틱당 한 번 브로드캐스트 (이전 답변 그대로) ----
    if (LatestPoses.Num() > 0)
    {
        FVector2f RepPelvis2D = FVector2f::ZeroVector;
        bool bFoundPelvis = false;

        for (const FPersonPose& Pose : LatestPoses)
        {
            if (Pose.XY.Num() >= 17)
            {
                const FVector2f& LHip = Pose.XY[11];
                const FVector2f& RHip = Pose.XY[12];
                if (IsFinite2D(LHip) && IsFinite2D(RHip))
                {
                    RepPelvis2D = (LHip + RHip) * 0.5f;
                    bFoundPelvis = true;
                    break;
                }
            }
        }

        OnPoseBufferChanged.Broadcast(RepPelvis2D, LatestPoses, PixelToUU, OwnerXformFinal);
    }

    // ---- Hand 틱당 한 번 브로드캐스트 (새 시그니처) ----
    if (LatestHands.Num() > 0)
    {
        // 1) Hands의 PersonId에 매칭되는 Pose에서 Pelvis 우선 추출
        auto ComputePelvisFromPose = [](const FPersonPose& Pose)->TOptional<FVector2f>
            {
                if (Pose.XY.Num() >= 17)
                {
                    const FVector2f& LHip = Pose.XY[11];
                    const FVector2f& RHip = Pose.XY[12];
                    if (IsFinite2D(LHip) && IsFinite2D(RHip))
                        return TOptional<FVector2f>((LHip + RHip) * 0.5f);
                }
                return TOptional<FVector2f>();
            };

        FVector2f RepPelvis2D = FVector2f::ZeroVector;
        bool bFoundPelvis = false;

        // a) Hands 중 유효 pid 손 → 같은 pid의 Pose에서 Pelvis 찾기
        for (const FHandPose& H : LatestHands)
        {
            if (H.PersonId == 0xFFFF) continue;
            const FPersonPose* PosePtr = LatestPoses.FindByPredicate(
                [&](const FPersonPose& P) { return P.PersonId == H.PersonId; });
            if (PosePtr)
            {
                if (TOptional<FVector2f> Pel = ComputePelvisFromPose(*PosePtr))
                {
                    RepPelvis2D = *Pel;
                    bFoundPelvis = true;
                    break;
                }
            }
        }

        // b) 없으면 LatestPoses에서 첫 유효 Pelvis
        if (!bFoundPelvis)
        {
            for (const FPersonPose& Pose : LatestPoses)
            {
                if (TOptional<FVector2f> Pel = ComputePelvisFromPose(Pose))
                {
                    RepPelvis2D = *Pel;
                    bFoundPelvis = true;
                    break;
                }
            }
        }

        // c) 그래도 없으면 ZeroVector 유지
        OnHandBufferChanged.Broadcast(RepPelvis2D, LatestHands, PixelToUU, OwnerXformFinal);
    }
}

void UPoseUdpReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseSocket();
    Super::EndPlay(EndPlayReason);
}

bool UPoseUdpReceiverComponent::InitSocket(int32 Port)
{
    CloseSocket();

    Socket = FUdpSocketBuilder(TEXT("PoseRecv"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToEndpoint(FIPv4Endpoint(FIPv4Address::Any, Port))
        .WithReceiveBufferSize(1 << 20);

    return Socket != nullptr;
}

void UPoseUdpReceiverComponent::CloseSocket()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
}

bool UPoseUdpReceiverComponent::ReceiveOnce(TArray<FPersonPose>& OutPoses, TArray<FHandPose>& OutHands, uint64& OutTsMs)
{
    OutPoses.Reset();
    OutHands.Reset();
    OutTsMs = 0;

    if (!Socket) return false;

    uint32 BytesPending = 0;
    if (!Socket->HasPendingData(BytesPending) || BytesPending < HEADER_SIZE) return false;

    RecvBuffer.SetNumUninitialized(BytesPending);
    int32 BytesRead = 0;
    if (!Socket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, ESocketReceiveFlags::None)) return false;
    if (BytesRead < HEADER_SIZE) return false;

    const uint8* p = RecvBuffer.GetData();
    const uint8* end = RecvBuffer.GetData() + BytesRead;

    auto readU32 = [&p]() { uint32 v = 0; FMemory::Memcpy(&v, p, 4); p += 4; return v; };
    auto readU16 = [&p]() { uint16 v = 0; FMemory::Memcpy(&v, p, 2); p += 2; return v; };
    auto readU8 = [&p]() {  uint8 v = *p; p += 1; return v; };
    auto readU64 = [&p]() { uint64 v = 0; FMemory::Memcpy(&v, p, 8); p += 8; return v; };
    auto readF32 = [&p]() {  float v = 0; FMemory::Memcpy(&v, p, 4); p += 4; return v; };

    // ---- Header (match "<4sBBHHQ") ----
    uint32 magic = readU32();
    if (magic != *((uint32*)"POSE")) return false;
    uint8  ver = readU8();
    (void)readU8();                // flags (현재 사용 안 함)
    uint16 persons = readU16();
    uint16 hands = readU16();    // ★ 기존 코드엔 없었음
    OutTsMs = readU64();

    // ---- Persons ----
    OutPoses.Reserve(persons);
    for (uint16 i = 0; i < persons; ++i)
    {
        if (p + EXPECTED_PERSON_BLOCK > end) return (OutPoses.Num() + OutHands.Num()) > 0;

        FPersonPose pose;
        pose.PersonId = readU16();

        pose.XY.SetNumUninitialized(17);
        for (int k = 0; k < 17; ++k)
        {
            const float x = readF32();
            const float y = readF32();
            pose.XY[k] = FVector2f(x, y);
        }
        pose.Conf.SetNumUninitialized(17);
        for (int k = 0; k < 17; ++k)
        {
            pose.Conf[k] = readF32();
        }
        OutPoses.Add(MoveTemp(pose));
    }

    // ---- Hands (v2 이상) ----
    if (ver >= 2)
    {
        OutHands.Reserve(hands);
        for (uint16 i = 0; i < hands; ++i)
        {
            if (p + EXPECTED_HAND_BLOCK > end) break;

            FHandPose H;
            // 송신은 hand_id(H) 이고, personId는 안 옵니다. 일단 Unknown으로 표기:
            const uint16 hand_id = readU16();
            (void)hand_id;
            const uint8 handed = readU8();   // 0=Right,1=Left,2=Unknown (송신 기준)
            const float score = readF32();  // 현재는 conf 배열에 동일 값 채움

            // 좌표 21개
            H.XY.SetNumUninitialized(21);
            for (int k = 0; k < 21; ++k)
            {
                const float x = readF32();
                const float y = readF32();
                H.XY[k] = FVector2f(x, y);
            }

            // 각 키포인트 conf (21개)
            H.Conf.SetNumUninitialized(21);
            for (int k = 0; k < 21; ++k)
            {
                H.Conf[k] = readF32();
            }

            // 수신 구조 채우기
            H.PersonId = 0xFFFF;               // 매칭 정보 없음
            // 주의: 네 구조가 0=Left,1=Right 로 쓴다면 여기서 스왑 필요
            H.Which = (handed == 0) ? 1 : (handed == 1) ? 0 : 2;

            // Center는 패킷에 없으니 평균으로 계산(원하면)
            FVector2f sum(0, 0);
            int n = 0;
            for (const auto& p2 : H.XY) { if (IsFinite2D(p2)) { sum += p2; ++n; } }
            H.Center = (n > 0) ? (sum / float(n)) : FVector2f(0, 0);

            OutHands.Add(MoveTemp(H));
        }
    }

    return (OutPoses.Num() + OutHands.Num()) > 0;
}
