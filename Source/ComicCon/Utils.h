
#pragma once

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "RenderUtils.h"

/// <param name="ServerBase">e.g. http://127.0.0.1:3000</param>
void UploadFileToServer(const FString& FilePath, const FString& ServerBase, TFunction<void(bool, FString /*qrUrl*/)> Done)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        Done(false, TEXT("")); return;
    }

    FString Boundary = TEXT("----UEForm") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString Url = ServerBase + TEXT("/api/upload");

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));

    // build multipart body
    TArray<uint8> Payload;
    auto AddTextField = [&](const FString& Name, const FString& Value)
        {
            FString Preamble = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"), *Boundary, *Name, *Value);
            FTCHARToUTF8 Conv(*Preamble);
            Payload.Append((uint8*)Conv.Get(), Conv.Length());
        };
    auto AddFileField = [&](const FString& Name, const FString& Filename, const TArray<uint8>& Data, const FString& ContentType)
        {
            FString Header = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"),
                *Boundary, *Name, *Filename, *ContentType);
            FTCHARToUTF8 Hdr(*Header);
            Payload.Append((uint8*)Hdr.Get(), Hdr.Length());
            Payload.Append(Data);
            FTCHARToUTF8 CRLF(TEXT("\r\n"));
            Payload.Append((uint8*)CRLF.Get(), CRLF.Length());
        };

    AddTextField(TEXT("max_downloads"), TEXT("3")); // 필요시 조정
    AddFileField(TEXT("file"), FPaths::GetCleanFilename(FilePath), FileData, TEXT("video/mp4"));

    FString Tail = FString::Printf(TEXT("--%s--\r\n"), *Boundary);
    {
        FTCHARToUTF8 T(*Tail);
        Payload.Append((uint8*)T.Get(), T.Length());
    }

    Req->SetContent(MoveTemp(Payload));
    Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK) {
        if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() != 200)
        {
            Done(false, TEXT("")); return;
        }

        // { ok:true, qr_png_url:"...", download_url:"..." }
        TSharedPtr<FJsonObject> Json;
        auto Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid() && Json->GetBoolField(TEXT("ok")))
        {
            Done(true, Json->GetStringField(TEXT("qr_png_url")));
        }
        else { Done(false, TEXT("")); }
        });
    Req->ProcessRequest();
}

UTexture2D* CreateTextureFromPng(const TArray<uint8>& InPngData)
{
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(InPngData.GetData(), InPngData.Num()))
        return nullptr;

    const int32 Width = Wrapper->GetWidth();
    const int32 Height = Wrapper->GetHeight();

    TArray<uint8> RawBGRA;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
        return nullptr;

    UTexture2D* Tex = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Tex) return nullptr;
    Tex->SRGB = true;

    // 첫 번째 mip에 픽셀 복사
    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, RawBGRA.GetData(), RawBGRA.Num());
    Mip.BulkData.Unlock();

    Tex->UpdateResource();
    return Tex;
}

// QR PNG를 HTTP GET으로 받아 Texture 생성
void FetchQrTexture(const FString& QrUrl, TFunction<void(bool, UTexture2D*)> Done)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(QrUrl);
    Req->SetVerb(TEXT("GET"));
    Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK) {
        if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() != 200)
        {
            Done(false, nullptr); return;
        }

        const TArray<uint8>& PngData = Resp->GetContent();
        if (UTexture2D* Tex = CreateTextureFromPng(PngData))
        {
            Done(true, Tex);
        }
        else
        {
            Done(false, nullptr);
        }
        });
    Req->ProcessRequest();
}

bool DeprojectToXPlane(APlayerController* PC, const FVector2D& ScreenPt, float PlaneX, FVector& OutOnPlane)
{
    FVector O, D;
    if (!PC || !PC->DeprojectScreenPositionToWorld(ScreenPt.X, ScreenPt.Y, O, D))
        return false;

    const float Den = D.X;
    if (FMath::IsNearlyZero(Den)) return false; // 평행
    const float t = (PlaneX - O.X) / Den;
    if (t <= 0.f) return false;                  // 카메라 뒤
    OutOnPlane = O + t * D;
    return true;
}

bool ComputeEdgePointsAtX(APlayerController* PC, float PlaneX, int32 MarginPx, FVector& OutLeft, FVector& OutRight)
{
    if (!PC) return false;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    if (W <= 0 || H <= 0) return false;

    const FVector2D LeftOutside(-MarginPx, H * 0.5f);
    const FVector2D RightOutside(W + MarginPx, H * 0.5f);

    bool b1 = DeprojectToXPlane(PC, LeftOutside, PlaneX, OutLeft);
    bool b2 = DeprojectToXPlane(PC, RightOutside, PlaneX, OutRight);
    return b1 && b2;
}

bool IsFinite2D(const FVector2f& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
}

bool GetPelvis2D(const FPersonPose& P, FVector2f& OutPelvis) {
    if (!(P.XY.IsValidIndex(COCO_LHIP) && P.XY.IsValidIndex(COCO_RHIP))) return false;
    const FVector2f L = P.XY[COCO_LHIP];
    const FVector2f R = P.XY[COCO_RHIP];
    if (!IsFinite2D(L) || !IsFinite2D(R)) return false;
    OutPelvis = (L + R) * 0.5f;
    return true;
}

bool GetShoulderWidth2D(const FPersonPose& P, double& OutSW) {
    if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH))) return false;
    const FVector2f L = P.XY[COCO_LSH];
    const FVector2f R = P.XY[COCO_RSH];
    if (!IsFinite2D(L) || !IsFinite2D(R)) return false;
    OutSW = FVector2D::Distance(FVector2D(L.X, L.Y), FVector2D(R.X, R.Y));
    return true;
}

FVector ToWorldFrom2D(const FVector2f& Pelvis2D, const FVector2f& Joint2D, const FTransform& OwnerXf, float PixelToUU, bool bInvertImageYToUp, float DepthOffsetX)
{
    const FVector2f Rel = Joint2D - Pelvis2D;
    const float YY = Rel.X * PixelToUU;
    const float ZZ = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * PixelToUU;
    const FVector Local(DepthOffsetX, YY, ZZ);
    return OwnerXf.TransformPosition(Local);
}



struct FSample { double T; FVector2D C; float ShoulderW; bool Close; };

void ResampleToFixedRate(const TArray<FSample>& In, double Hz, TArray<FSample>& Out)
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

void SmoothEMA(TArray<FSample>& A, double Alpha /*0~1*/)
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
