// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SIMPLEEDITOR_API FUE5SimpleEditorModuleImpl : public FDefaultGameModuleImpl
{
	//FUE5SimpleEditorModuleImpl是模块的名字,下面是重写两个函数

public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
};