#pragma once

//该头文件作用让每个线程拥有独立的线程缓存

#include "Common.h"


class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	// 释放对象时，自由链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];//哈希桶，映射不同类型的自由链表，桶里面存的是还回来的空间。申请空间可以从哈希桶上申请，释放的空间交给哈希桶去维护
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr; //定义一个全局的threadcache对象指针，threadcache即使全局的，但也是每个线程独有的。