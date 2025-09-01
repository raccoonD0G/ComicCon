/*
	Copyright (C) 2024 Vizrt NDI AB. All rights reserved.

	This file and its use within a Product is bound by the terms of NDI SDK license that was provided
	as part of the NDI SDK. For more information, please review the license and the NDI SDK documentation.
*/

#pragma once

#include <CoreMinimal.h>
#include <Engine/Texture.h>
#include <Misc/EngineVersionComparison.h>
#include <RHI.h>
#include <RHICommandList.h>

#include "NDIMediaTexture2D.generated.h"

/**
	A Texture Object used by an NDI Media Receiver object for capturing video from
	a network source
*/
UCLASS(NotBlueprintType, NotBlueprintable, HideDropdown,
	   HideCategories = (ImportSettings, Compression, Texture, Adjustments, Compositing, LevelOfDetail, Object),
	   META = (DisplayName = "NDI Media Texture 2D"))
class NDIIO_API UNDIMediaTexture2D : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	virtual float GetSurfaceHeight() const override;
	virtual float GetSurfaceWidth() const override;

	virtual float GetSurfaceDepth() const;
	virtual uint32 GetSurfaceArraySize() const;

	virtual ETextureClass GetTextureClass() const;

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual EMaterialValueType GetMaterialType() const override;

	virtual void UpdateTextureReference(FRHICommandList& RHICmdList, FTextureRHIRef Reference) final;

private:
	virtual class FTextureResource* CreateResource() override;

	void SetMyResource(FTextureResource* ResourceIn);
	FTextureResource* GetMyResource();
	const FTextureResource* GetMyResource() const;
};