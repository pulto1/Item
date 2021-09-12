#pragma once

//该头文件的作用是向中心缓存分配页为单位的空间，如果空间不足找操作系统申请空间

#include "Common.h"
#include "PageMap.h"

//为了保证全局只有一个唯一的CentralCache对象，这个类被设计成单例模式
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// 向系统申请k页内存挂到自由链表
	void* SystemAllocPage(size_t k);

	//给中心缓存分配k页的空间
	Span* NewSpan(size_t k);

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

private:
	//SpanList管理的是相同页数的空间，用来给中心缓存分配
	SpanList _spanList[NPAGES];	//哈希表，按页数映射

	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap; //建立页号和span的映射关系
	// tcmalloc 基数树  效率更高

	//页缓存的span可能会发生切割，所以不能像中心缓存那样只对桶加锁，必须对页缓存整体加锁
	std::recursive_mutex _mtx; //递归锁 
	//这里定义的锁是recursive_mutex递归互斥锁，而不是mutex互斥锁
	//原因是在成员函数NewSpan中有递归，如果用互斥锁，递归调用函数时，锁前面加的锁还未释放，就会造成死锁问题
	//而recursive_mutex锁就是专门为递归准备的

private:
	PageCache()
	{}

	PageCache(const PageCache&) = delete;

	// 单例
	static PageCache _sInst;
};
