// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleEditor.h"

#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "MultiWindowMgr/MultiWindowMgr.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FUE5SimpleEditorModuleImpl, SimpleEditor, "SimpleEditor");

#pragma optimize("",off)
void FUE5SimpleEditorModuleImpl::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();
	//这个绑定在编辑器初始化的时候就会绑定上，然后之后在打开PIE的时候就会触发lambda表达式
	FEditorDelegates::PostPIEStarted.AddLambda([](const bool e)
	{
		FMultiWindowMgr::SeparateUI();
	}
	);
	//PIE的开始一共是有三个阶段
	//第一个阶段 PreBeginPIE 预开始阶段 Static FOnPIEEvent PreBeginPIE
	//第二个阶段 BeginPIE 开始的阶段 Static FOnPIEEvent BeginPIE
	//第三个阶段 PostPIEStarted PIE开始结束的时候触发的

	FEditorDelegates::EndPIE.AddLambda([](const bool e)
	{
		FMultiWindowMgr::RecoveryUI();
	}
	);
}

void FUE5SimpleEditorModuleImpl::ShutdownModule()
{
	FDefaultGameModuleImpl::ShutdownModule();
}

#pragma optimize("",on)