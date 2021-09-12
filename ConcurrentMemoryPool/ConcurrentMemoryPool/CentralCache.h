#pragma once

//该头文件其中间调度作用，给线程缓存分配空间，找页缓存申请空间。把线程缓存还回来的空间，合并成大空间再还给页缓存

#include "Common.h"

//为了保证全局只有一个唯一的CentralCache对象，这个类被设计成单例模式
class CentralCache
{
public:
	static CentralCache* GetInstance() //提供一个静态的成员函数，让外界获取这个全局唯一的对象
	{
		return &_sInst;
	}

	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);

	// 从SpanList或者page cache获取一个span
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanLists[NFREELISTS]; // 哈希桶，按对齐+梯度方式映射


//单例模式的特性
private:
	CentralCache() //封印构造函数，只允许在类域范围构造对象
	{}

	CentralCache(const CentralCache&) = delete; //防拷贝，只声明，不定义，并且声明私有

	static CentralCache _sInst; //声明该类只有一个_sInst对象
};