#pragma once

#include"common.h"

//template<size_t  size> //C语言的写法，模板参数是一个具体的大小，只管申请内存。缺点是不能初始化内存。
//class ObjectPool
//{
//
//};

template<class T> //实现一个对象池模板，传对象的类型。C++写法，传类型的好处是，知道类型之后，在申请完空间之后，可以用定位new显式调用该类型的构造函数初始化一下。
class ObjectPool
{
public:
	~ObjectPool()
	{
		// ...
	}

	void*& NextObj(void* obj) //返回自由链表中的obj指针域的引用
	{
		//平常我们维护一个链表结构都是用一个结点的结构体去维护的，这里还需要定义一个结点的结构体去维护吗？
		//结点结构体的核心是想保存一个val，而这里的自由链表没有必要存放val
		//这个释放回来的空间本身就可以充当这个链表的结构，其前4（32位）或者8（64位）个字节可以存放指向下一个位置的地址
		//也就是说这块空间我拿给你用，你还回来之后我还要用这块空间本身去存地址，维持链表结构，不用借助其它空间开链表
		//由于不同平台下指针的大小是不相同的，所以不能确定是取前4个字节还是前8个字节来充当指针域
		return *((void**)obj); //这里所采取的做法是，通过先强转成void**，再解引用，从这块被释放的空间里拿出一个void*大小的空间。
		//因为指针的意义就是，解引用之后取多少字节的空间。比如*（（int*）obj）拿到的就是一个int大小的空间，所以 *((void**)obj)拿到就是一个void*大小的空间
		//这样32位平台拿出来的就是4字节，64位平台下拿出来的就是8字节
		//这里的主要目的是拿出一个指针的空间，所以这里用char**或者int**都是可以的
	}

	T* New() //申请内存
	{
		T* obj = nullptr; //所申请内存的返回值，相当于malloc函数的返回值
		if (_freeList) //如果自由链表不为空，就去自由链表里面去申请内存
		{
			//此处相当于头删
			obj = (T*)_freeList; //_freeList的类型是void*，给值的时候记得强转
			//_freeList = *((void**)_freeList);
			_freeList = NextObj(_freeList);
		}
		else //自由链表为空，去_memory指向的空间里面去申请
		{
			if (_leftSize < sizeof(T)) //如果_memory指向的空间内存不够，这时需要扩展空间
			{
				_leftSize = 1024 * 100;
				_memory = (char*)malloc(_leftSize); //新开辟出来一块空间
				if (_memory == nullptr) //空间开辟失败
				{
					//exit(-1);
					//cout << "malloc fail" << endl;
					throw std::bad_alloc(); //抛异常，bad_alloc是库里面针对异常内存申请失败的类型
				}
			}
			//到这里，，一定可以确保_memory指向的位置，有足够的空间去给

			obj = (T*)_memory; //通过类型强转，分割出类型T所需的空间
			_memory += sizeof(T); //空间分割出去之后，_memory指向的位置越过分割出去的空间
			_leftSize -= sizeof(T); //调整剩余空间大小
			//通过此处的逻辑可以判断出，分割出去的这块空间，从物理结构上看还在那个位置，只是从逻辑结构的角度去分析，这块内存已经不属于_memory指向的空间了
		}

		new(obj)T; //通过定位new对申请到的内存，调用T类型的默认构造函数初始化
		return obj;
	}

	void Delete(T* obj) //释放内存函数
	{
		obj->~T(); //先调用类型T的析构函数

		// 头查到freeList
		//*((int*)obj) = (int)_freeList;
		//*((void**)obj) = _freeList;

		//将删除过的空间头插到自由链表中
		NextObj(obj) = _freeList; //被删空间的下一个结点指向此时自由链表的起始位置，NextObj函数的作用是返回obj的指针域（实际上是被删空间的前4（32位）或者8（64位）个字节）

		_freeList = obj; //自由链表的起始位置变为刚删除的空间
	}

private:
	//这里的成员变量都给上缺省值，目的是为了让默认构造函数去生成
	char* _memory = nullptr; //指向内存池用作分配的空间，用char*指针指向它的原因是，在分配空间切割时，_memory直接加上所申请空间的大小，即可将该空间分割出去（此处画图比较容易理解）
	int   _leftSize = 0; //_memory剩余空间的大小，用来控制分配内存的边界
	void* _freeList = nullptr; //指向自由链表，使用过的空间挂在该自由链表上，下次申请空间时，如果自由链表不为空，就直接到自由链表上取空间，而不是去_memory上分割，实现空间的重复利用
};

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	/*ObjectPool<TreeNode> tnPool;
	std::vector<TreeNode*> v;
	for (size_t i = 0; i < 100; ++i)
	{
		TreeNode* node = tnPool.New();
		cout << node << endl;
		v.push_back(node);
	}

	for (auto e : v)
	{
		tnPool.Delete(e);
	}*/

	/*ObjectPool<TreeNode> tnPool;
	TreeNode* node1 = tnPool.New();
	TreeNode* node2 = tnPool.New();
	TreeNode* node3 = tnPool.New();
	TreeNode* node4 = tnPool.New();
	cout << node1 << endl;
	cout << node2 << endl;
	cout << node3 << endl;
	cout << node4 << endl;
	tnPool.Delete(node1);
	tnPool.Delete(node4);


	TreeNode* node10 = tnPool.New();
	cout << node10 << endl;

	TreeNode* node11 = tnPool.New();
	cout << node11 << endl;*/

	size_t begin1 = clock();
	std::vector<TreeNode*> v1;
	for (int i = 0; i < 100000; ++i)
	{
		v1.push_back(new TreeNode);
	}
	for (int i = 0; i < 100000; ++i)
	{
		delete v1[i];
	}
	v1.clear();

	for (int i = 0; i < 100000; ++i)
	{
		v1.push_back(new TreeNode);
	}

	for (int i = 0; i < 100000; ++i)
	{
		delete v1[i];
	}
	v1.clear();
	size_t end1 = clock();


	ObjectPool<TreeNode> tnPool;
	size_t begin2 = clock();
	std::vector<TreeNode*> v2;
	for (int i = 0; i < 100000; ++i)
	{
		v2.push_back(tnPool.New());
	}
	for (int i = 0; i < 100000; ++i)
	{
		tnPool.Delete(v2[i]);
	}
	v2.clear();

	for (int i = 0; i < 100000; ++i)
	{
		v2.push_back(tnPool.New());
	}
	for (int i = 0; i < 100000; ++i)
	{
		tnPool.Delete(v2[i]);
	}
	v2.clear();

	size_t end2 = clock();

	cout << end1 - begin1 << endl;
	cout << end2 - begin2 << endl;


	ObjectPool<char> chPool;

}