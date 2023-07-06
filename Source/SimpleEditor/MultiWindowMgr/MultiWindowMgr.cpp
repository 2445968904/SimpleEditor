// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiWindowMgr.h"
#include <ILevelEditor.h>
#include <LevelEditor.h>

#include "SLevelViewport.h"

void FMultiWindowMgr::SeparateUI()
{
	FLevelEditorModule & LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if(LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (TSharedPtr<SLevelViewport> ViewportWindow : Viewports)
		{
			TSharedPtr<SLevelViewport> Viewport = StaticCastSharedPtr<SLevelViewport>(ViewportWindow);
			if(!Viewport->IsPlayInEditorViewportActive())continue; //检查viewport是否处于活动的状态
			TSharedPtr<SWidget> p1 = Viewport->GetParentWidget();//p1.SCanvas
			TSharedPtr<SWidget> p2 = p1->GetParentWidget();//p2.Soverlay
			TSharedPtr<SWidget> p3 = p2->GetParentWidget();//p3.SAssetEditorViewportsOverlay
			TSharedPtr<SWidget> p4 = p3->GetParentWidget();//p4: SBorder
			TSharedPtr<SWidget> p5 = p4->GetParentWidget();//p5:SOverlay
			//转换类型
			TSharedPtr<SOverlay> p5_Overlay = StaticCastSharedPtr<SOverlay>(p5);
			//获取p5的子UI
			TPanelChildren<SOverlay::FOverlaySlot>* childptr = static_cast<TPanelChildren<SOverlay::FOverlaySlot>*>( p5_Overlay->GetChildren() ) ;
			for(int32 i=0;i<childptr->Num();++i)
			{
				TSharedRef<SWidget> r= childptr->GetChildAt(i);
				if(r != p4) //这里判断这个子类是否是我们的SBorder这个子类
				{
					//临时的进行保存,因为把父组件和子组件无差别断开了，所以需要保存，在关闭的时候可以绑定回去
					
				}
			}
			//清除P5的子UI
			p5_Overlay->ClearChildren();
			//创建新UI (tableManager)
			TSharedPtr<SWidget> ContainerWidget = FMultiWindowMgr::BuildTabManagerWidget();

			//插入到p5
			p5_Overlay->AddSlot()
			        [
						ContainerWidget.ToSharedRef()
					];

			//通知编辑器 我们的tableManager有变化
		}
	}
}

TSharedPtr<SWidget> FMultiWindowMgr::BuildTabManagerWidget()
{
	//首先定义一个布局,或者说是布局
	TSharedRef<FTabManager::FLayout> LoadedLayout=
		FTabManager::NewLayout("MySimpleEditorLayout")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Horizontal)//第一套split是从左往右的进行分隔
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.15f)//所占的比例
			->SetHideTabWell(true)//是否隐藏Tab的关闭按钮
			
		)
		
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient(0.6f)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.4f)
				->Split
				(
					FTabManager::NewStack()
						->SetSizeCoefficient(0.05f)
						->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
				)
			
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Vertical)
			->SetSizeCoefficient(0.25f)
			->Split
			(
				
			)
			->Split
			(
					
			)
		)
	)；
	
	
}
