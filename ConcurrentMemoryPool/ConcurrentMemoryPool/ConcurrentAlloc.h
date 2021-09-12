#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"


static void* ConcurrentAlloc(size_t size) //提供给用户申请内存的接口
{
	try
	{
		if (size > MAX_BYTES) //申请64k以上的空间，去找pagecache要空间
		{
			// PageCache
			size_t npage = SizeClass::RoundUp(size) >> PAGE_SHIFT; //除以4k,得到所需大空间页数
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = size;

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT); //页id乘以4k就是这块空间地址
			return ptr;
		}
		else //申请64k以下的空间就去threadcache里面去找
		{
			if (tls_threadcache == nullptr)
			{
				tls_threadcache = new ThreadCache; 
			}

			return tls_threadcache->Allocate(size);
		}
	}

	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
	return nullptr;
	
}

//释放内存时，只需拿到被释放空间的地址即可
//如果是大内存，用户需直接还给内核，大内存以span的方式保存，通过地址拿到span，在通过中页缓存中转，调SystemFree函数还给操作系统
//如果是小内存，用户则挂回线程缓存的自由链表中，调用Deallocate函数，该函数的第二个参数，小内存大小保存在span的_objsize中
static void ConcurrentFree(void* ptr) //提供给用户释放内存的接口
{
	try
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objsize; //span中小内存大小

		if (size > MAX_BYTES) //释放大块内存直接释放给pagecache
		{
			// PageCache
			PageCache::GetInstance()->ReleaseSpanToPageCache(span); //这里还回来的是大对象，没有被切割
		}
		else //释放小块内存释放给threadcache
		{
			assert(tls_threadcache); //小块内存的tls不能为空，不能用于检查大块内存，因为大块内存没有走线程缓存
			tls_threadcache->Deallocate(ptr, size);
		}
	}

	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
}