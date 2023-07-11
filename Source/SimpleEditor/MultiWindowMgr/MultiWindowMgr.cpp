// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiWindowMgr.h"
#include <ILevelEditor.h>
#include <LevelEditor.h>

#include "SLevelViewport.h"
#pragma optimize("",off)

TArray<FMultiWindowMgr::FViewportWidgetData> FMultiWindowMgr::ModifyViewport;

void FMultiWindowMgr::SeparateUI()
{
	FLevelEditorModule & LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if(LevelEditor.IsValid())
	{
		bool HasActiveViewport =false;
		
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (TSharedPtr<SLevelViewport> ViewportWindow : Viewports)
		{
			TSharedPtr<SLevelViewport> Viewport = StaticCastSharedPtr<SLevelViewport>(ViewportWindow);
			if(!Viewport->IsPlayInEditorViewportActive())continue; //检查viewport是否处于活动的状态

			HasActiveViewport =true ;
			
			TSharedPtr<SWidget> p1 = Viewport->GetParentWidget();//p1.SCanvas
			TSharedPtr<SWidget> p2 = p1->GetParentWidget();//p2.Soverlay
			TSharedPtr<SWidget> p3 = p2->GetParentWidget();//p3.SAssetEditorViewportsOverlay
			TSharedPtr<SWidget> p4 = p3->GetParentWidget();//p4: SBorder
			TSharedPtr<SWidget> p5 = p4->GetParentWidget();//p5:SOverlay
			//转换类型
			TSharedPtr<SOverlay> p5_Overlay = StaticCastSharedPtr<SOverlay>(p5);

			//创建保存的数据
			ModifyViewport.Add(FViewportWidgetData());
			FViewportWidgetData& ViewportWidgetData = ModifyViewport[ModifyViewport.Num()-1];
			ViewportWidgetData.Type = false ;
			ViewportWidgetData.P4=p4;
			ViewportWidgetData.P5_Overlay= p5_Overlay;
			
			//获取p5的子UI
			TPanelChildren<SOverlay::FOverlaySlot>* childptr = static_cast<TPanelChildren<SOverlay::FOverlaySlot>*>( p5_Overlay->GetChildren() ) ;
			for(int32 i=0;i<childptr->Num();++i)
			{
				TSharedRef<SWidget> r= childptr->GetChildAt(i);
				if(r != p4) //这里判断这个子类是否是我们的SBorder这个子类
				{
					//临时的进行保存,因为把父组件和子组件无差别断开了，所以需要保存，在关闭的时候可以绑定回去
					ViewportWidgetData.P5_Childs.Add(r);
				}
			}
			//清除P5的子UI
			p5_Overlay->ClearChildren();
			//创建新UI (tableManager)
			TSharedPtr<SWidget> ContainerWidget = FMultiWindowMgr::BuildTabManagerWidget(p4);

			//插入到p5
			p5_Overlay->AddSlot()
			        [
						ContainerWidget.ToSharedRef()
					];

			//通知编辑器 我们的tableManager有变化
			LevelEditorModule.OnTabManagerChanged().Broadcast();
		}
		//这个if代表着播放的是新的窗口
		//1.New Editor Window 2.没有活动的视口
		if(!HasActiveViewport)
		{
			//获取的SWindow是新弹出来的窗口
			TSharedPtr<SWindow> RootWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
			TSharedPtr<const SWidget> ContentWidget = RootWindow->GetContent();

			ModifyViewport.Add(FViewportWidgetData());
			FViewportWidgetData & ViewportWidgetData = ModifyViewport[ModifyViewport.Num()-1];
			ViewportWidgetData.Type = true ;

			ViewportWidgetData.P4 = const_cast<SWidget*>(ContentWidget.Get())->AsShared();
			//将p4和RootWindow之间插入UI
			//注意P4 之前是SBorder 现在是SOverlay 因为P4的类型是SWidget 所以可以之间进行赋值的操作

			ViewportWidgetData.P5_Overlay = StaticCastSharedRef<SOverlay>(((SWidget*)RootWindow.Get())->AsShared());
				//p5是SOverlay ,现在是SWindow ，因为P5_Overlay 是指针 ，这里只是简单储存一下地址，不直接使用

			//生成UI
			TSharedPtr<SWidget> ContainerWidget = FMultiWindowMgr::BuildTabManagerWidget(ViewportWidgetData.P4);

			//把新的UI插入到Window下面
			RootWindow->SetContent(ContainerWidget.ToSharedRef());
		}
	}
}

void FMultiWindowMgr::RecoveryUI()
{
	//还原UI
	for(const FViewportWidgetData & ViewportWindow : ModifyViewport)
	{
		if(ViewportWindow.Type)
		{
			//首先是要还原SWindow
			TSharedPtr<SWindow> RootWindow = StaticCastSharedRef<SWindow>(((SWidget*)ViewportWindow.P5_Overlay.Get())->AsShared());//指针强转的手段
			if(RootWindow.IsValid())
			{
				RootWindow->SetContent(ViewportWindow.P4.ToSharedRef());

				
			}
		}
		else
		{
			ViewportWindow.P5_Overlay->ClearChildren();
			ViewportWindow.P5_Overlay->AddSlot()
			[
				ViewportWindow.P4.ToSharedRef()
			];
			for(int32 i=0;i<ViewportWindow.P5_Childs.Num();++i)
			{
				ViewportWindow.P5_Overlay->AddSlot()
				[
					ViewportWindow.P5_Childs[i].ToSharedRef()
				];
			}
		}
		ModifyViewport.Empty();
		//通知引擎重新恢复UI了
		FLevelEditorModule & LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.OnTabContentChanged().Broadcast();
		}
		
}

TSharedPtr<SWidget> FMultiWindowMgr::BuildTabManagerWidget(TSharedPtr<SWidget> InsertWidget)
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
			->AddTab("SceneTreeTab",ETabState::OpenedTab)
			
		)
		
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient(0.6f)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.4f)
				->Split
				(
					FTabManager::NewStack()
						->SetSizeCoefficient(0.05f)
						->SetHideTabWell(true)
						->AddTab("ToolBarTab",ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab("SceneViewTab",ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab("AssetViewTab",ETabState::OpenedTab)
				)
			
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Vertical)
			->SetSizeCoefficient(0.25f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.6f)
				->SetHideTabWell(true)
				->AddTab("EntityDetailTab",ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.4f)
				->SetHideTabWell(true)
				->AddTab("RedoUndoListTab",ETabState::OpenedTab)
			)
		)
	)
	
	);

	//注册Tab 生成器
	//args是不定参数的意思
	//lambda表达式[&] 使用了就可以获得这个函数的变量 比如说这里的InsertWidget
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("SceneViewTab",FOnSpawnTab::CreateLambda([&](const FSpawnTabArgs &SpawnTabArgs)
		{
			TSharedRef<SDockTab> s = SNew(SDockTab)
			.TabRole(ETabRole::PanelTab)
		[
			InsertWidget.ToSharedRef()
		];
		return s;
		}
	)
	);
		
	

	
	//上面是样式 ，下面是生成UI的函数 loadedlayout是刚刚定义的布局
	TSharedPtr<SWidget> MainFrameContent = FGlobalTabmanager::Get()->RestoreFrom(LoadedLayout,nullptr,false);

	return MainFrameContent;
}
#pragma optimize("",on)
