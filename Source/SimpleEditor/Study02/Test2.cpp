


#include "Test2.h"

//宏的使用相当于简单的替换

#define M1(NAME) A##NAME##C
//M1(B) ==> ABC
#define M2(NAME) #NAME
//M2(HELLO)==>"HELLO"
#define M3(NAME) \
#NAME
//这样就实现宏的换行了。但是斜杠后面是不能加东西的
int32 ATest2::SValue=1;
ATest2::ATest2()
{
 	
	PrimaryActorTick.bCanEverTick = true;

}

//有的时候想要看调试的信息，但是被编辑器优化掉了，所以关闭这个优化
#pragma optimize("",off)
#pragma optimize("",on)
//这两个语句之间的代买优化都会被关闭


void ATest2::BeginPlay()
{
	Super::BeginPlay();
	{
		//指针的应用
		//原始的指针
	
		{
			FString* s0 = new FString();
			*s0 = TEXT("aaa");
			FString * ss0 =s0;
			delete(s0);
			s0 = nullptr;

			//*ss0 = TEXT("");这里的逻辑是有问题的
		}

		//智能指针
		{
			TSharedPtr<FString> s1 = MakeShareable(new FString());//这里是强引用 
			*s1 = TEXT("bbb");
			//手动删除 两种方案
			s1=nullptr; s1.Reset() ;
		
		}
		//智能引用
		{
			TSharedPtr<FString> s1=MakeShareable(new FString());
			TSharedPtr<FString> s2= s1.ToSharedRef();
			*s2 = TEXT("ddd");
		}
		//弱引用

		{
			TSharedPtr<FString> s1 =MakeShareable(new FString());
			TWeakPtr<FString> s3=s1;
			//如果s1的内存清空了，那么s3也会跟着清空 ，但是s3 是死是活与s1无关

			//原始指针的获取
			FString *s4 = s1.Get();
			FString *s5 = s3.Pin().Get();//这里的弱指针需要这么一步特殊的操作
		}
	}
	

	//指针的强制转化
	{
		const FString * s0 = new FString();
		//不能直接修改s0  *s0=TEXT("");
		FString*s1 =const_cast<FString *>(s0);
		*s1 = TEXT("aaa");//这样就可以修改const里面的内容拉
		delete s0;
		s0 = s1 = nullptr;
		
		//智能指针去掉const的方法
		TSharedPtr<FString> s2 = MakeShareable(new FString());
		TSharedPtr<const FString> s3 = s2;
		TSharedPtr<FString> s4= ConstCastSharedPtr<FString >(s3); // 注意这里的ConstCastSharedPtr和ConstCastSharedRef不要搞混淆了
		*s4 = TEXT("bbbb");  //也可以转化成原始指针之后，使用const_cast来进行转化

		FString *s5 = const_cast<FString *> (s3.Get());
		*s5 = TEXT("cccc");
	}
	//父类指向子类的指针
	{
		class A{};
		class B :public A
		{
		public:
			int32 value =1 ;
		};
		A*a =new B();
		B *b =(B*)a;//dynamic_cast (rtti)  虚幻自己的反射是Cast
		b->value =2;
		delete a;
		a=b=nullptr;
		TSharedPtr<A> a1 =MakeShareable(new B);
		TSharedPtr<B> b1 =StaticCastSharedPtr<B>(a1);
		b1->value =3 ;
	}
// lambda 表达式
	{
		//一个void 参数表为int32 的函数就这样定义了
		//这个方框是用来传入值的,如果说参数是写在类里面的,就是这里的z，那么在传入的时候使用Test2::z是会发生报错的
		//注意方括号里传入的是复制的值，意思就是在执行了这个lambda表达式后，这些值就会被确定下来,但是如果传入的是指针的话，指针地址不变但是可以改变指针的地址对应的数值
		int32 p=0;
		int32 q=1;
		z=0;
		TFunction<void(int32)> myfx = [p,q,this](int32 a)
		{
			int32 o=p+1+z;
		};
		//定义了一个返回bool的lambda表达式
		TFunction<bool(int32 *)>myfx2 = [](int32 *a) ->bool
		{
			return true;
		};

		//调用
		int32 value =2;
		int32 r=myfx2(&value);
		GEngine->AddOnScreenDebugMessage(-1,5,FColor::Green,FString::FromInt(r));
		//通过auto来定义lambda表达式
		auto myfx3 = [p,q,this](int32* a) -> int32
		{
			return 1 ;
		};

	}
	//宏的使用
	{
		int32 M1(B)=123;
		FString s = M2(hello); //等价于"hello"
	}
	
	
}

// Called every frame
void ATest2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

