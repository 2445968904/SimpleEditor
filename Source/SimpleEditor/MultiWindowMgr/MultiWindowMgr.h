// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <Blueprint/UserWidget.h>

/**
 * 
 */
class SIMPLEEDITOR_API FMultiWindowMgr
{
public:

	//分离UI
	static void SeparateUI();

	//恢复UI
	static void RecoveryUI();
	
	
private:
	static TSharedPtr<SWidget>BuildTabManagerWidget(TSharedPtr<SWidget> InsertWidget); //静态的创建TabManager的方法

	struct FViewportWidgetData
	{
		bool Type;
		TSharedPtr<SWidget> P4;
		TSharedPtr<class SOverlay> P5_Overlay;
		TArray<TSharedPtr<SWidget>> P5_Childs;
	};
	static TArray<FViewportWidgetData> ModifyViewport;

	friend class USinpleEditorGameEngine;

	

public:

	static TSharedPtr<class SOverlay> AssetViewUI;//容纳新UI的
	static TSharedPtr<SWidget> AssetViewUIHandle;//容纳待会会创建的资源UI,一直把这个UI抓住不让其释放
	static void RegisterAssetViewUI(class UUserWidget* UI);
	static void RegisterAssetViewUI(TSharedPtr<SWidget> UI);
};
