/*
	Copyright (C) 2024 Vizrt NDI AB. All rights reserved.

	This file and its use within a Product is bound by the terms of NDI SDK license that was provided
	as part of the NDI SDK. For more information, please review the license and the NDI SDK documentation.
*/

#include <Objects/Media/NDIMediaTexture2D.h>
#include <Objects/Media/NDIMediaTextureResource.h>
#include <Misc/EngineVersionComparison.h>

UNDIMediaTexture2D::UNDIMediaTexture2D(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	this->SetMyResource(nullptr);
}

void UNDIMediaTexture2D::UpdateTextureReference(FRHICommandList& RHICmdList, FTextureRHIRef Reference)
{
	if (GetMyResource() != nullptr)
	{
		if (Reference.IsValid() && GetMyResource()->TextureRHI != Reference)
		{
			GetMyResource()->TextureRHI = (FTextureRHIRef&)Reference;
			RHIUpdateTextureReference(TextureReference.TextureReferenceRHI, GetMyResource()->TextureRHI);
		}
		else if (!Reference.IsValid())
		{
			if (FNDIMediaTextureResource* TextureResource = static_cast<FNDIMediaTextureResource*>(this->GetMyResource()))
			{
				ENQUEUE_RENDER_COMMAND(FNDIMediaTexture2DUpdateTextureReference)
				([this](FRHICommandListImmediate& RHICmdList) {

					static int32 DefaultWidth = 1280;
					static int32 DefaultHeight = 720;

					// Set the default video texture to reference nothing
					TRefCountPtr<FRHITexture> RenderableTexture;

					const FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("NDIMediaTexture2DUpdateTextureReference"))
						.SetExtent(DefaultWidth, DefaultHeight)
						.SetFormat(EPixelFormat::PF_B8G8R8A8)
						.SetNumMips(1)
						.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::RenderTargetable)
						.SetClearValue(FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)));

					RenderableTexture = RHICreateTexture(CreateDesc);

					GetMyResource()->TextureRHI = (FTextureRHIRef&)RenderableTexture;

					RHIUpdateTextureReference(TextureReference.TextureReferenceRHI, GetMyResource()->TextureRHI);
				});

				// Make sure _RenderThread is executed before continuing
				FlushRenderingCommands();
			}
		}
	}
}

FTextureResource* UNDIMediaTexture2D::CreateResource()
{
	if (this->GetMyResource() != nullptr)
	{
		delete this->GetMyResource();
		this->SetMyResource(nullptr);
	}

	if (FNDIMediaTextureResource* TextureResource = new FNDIMediaTextureResource(this))
	{
		this->SetMyResource(TextureResource);

		ENQUEUE_RENDER_COMMAND(FNDIMediaTexture2DUpdateTextureReference)
		([this](FRHICommandListImmediate& RHICmdList) {

			static int32 DefaultWidth = 1280;
			static int32 DefaultHeight = 720;

			// Set the default video texture to reference nothing
			TRefCountPtr<FRHITexture> RenderableTexture;

			const FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("NDIMediaTexture2DCreateResourceTexture"))
				.SetExtent(DefaultWidth, DefaultHeight)
				.SetFormat(EPixelFormat::PF_B8G8R8A8)
				.SetNumMips(1)
				.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::RenderTargetable)
				.SetClearValue(FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)));

			RenderableTexture = RHICreateTexture(CreateDesc);

			GetMyResource()->TextureRHI = (FTextureRHIRef&)RenderableTexture;

			RHIUpdateTextureReference(TextureReference.TextureReferenceRHI, GetMyResource()->TextureRHI);
		});
	}

	return this->GetMyResource();
}

void UNDIMediaTexture2D::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (FNDIMediaTextureResource* CurrentResource = static_cast<FNDIMediaTextureResource*>(this->GetMyResource()))
	{
		CumulativeResourceSize.AddUnknownMemoryBytes(CurrentResource->GetResourceSize());
	}
}

float UNDIMediaTexture2D::GetSurfaceHeight() const
{
	return GetMyResource() != nullptr ? GetMyResource()->GetSizeY() : 0.0f;
}

float UNDIMediaTexture2D::GetSurfaceWidth() const
{
	return GetMyResource() != nullptr ? GetMyResource()->GetSizeX() : 0.0f;
}

float UNDIMediaTexture2D::GetSurfaceDepth() const
{
	return 0.0f;
}

uint32 UNDIMediaTexture2D::GetSurfaceArraySize() const
{
	return 0;
}

EMaterialValueType UNDIMediaTexture2D::GetMaterialType() const
{
	return MCT_Texture2D;
}


ETextureClass UNDIMediaTexture2D::GetTextureClass() const
{
	return ETextureClass::Other2DNoSource;
}

void UNDIMediaTexture2D::SetMyResource(FTextureResource* ResourceIn)
{
	SetResource(ResourceIn);
}

FTextureResource* UNDIMediaTexture2D::GetMyResource()
{
	return GetResource();
}

const FTextureResource* UNDIMediaTexture2D::GetMyResource() const
{
	return GetResource();
}
