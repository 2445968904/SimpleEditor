// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "SinpleEditorGameEngine.generated.h"

/**
 * 
 */
UCLASS()
class SIMPLEEDITOR_API USinpleEditorGameEngine : public UGameEngine
{
	GENERATED_BODY()


public:

	virtual void Init (class IEngineLoop* InEngineLoop) override;

	virtual void Tick(float DeltaSeconds , bool bIdleMode) override;

	//因为CreateGameViewport不是虚函数，不能用一般的手段来覆写，所以用一个函数来进行一次代替
	void CreateGameViewport_New(UGameViewportClient* GameViewportClient);
	void CreateGameViewportWidget_New(UGameViewportClient* GameViewportClient);
	void SwitchGameWindowToUseGameViewport_New();
	

	FSceneViewport * GetGameSceneViewport(UGameViewportClient * ViewportClient) const ;

	void OnViewportResized_New(FViewport* Viewport,uint32 Unused);

	TSharedPtr<SViewport> New_GameViewportWidget;
	
	double LastTimeLogsFlushed2=0.0f;
	
};
