#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"


static void* ConcurrentAlloc(size_t size) //�ṩ���û������ڴ�Ľӿ�
{
	try
	{
		if (size > MAX_BYTES) //����64k���ϵĿռ䣬ȥ��pagecacheҪ�ռ�
		{
			// PageCache
			size_t npage = SizeClass::RoundUp(size) >> PAGE_SHIFT; //����4k,�õ������ռ�ҳ��
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = size;

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT); //ҳid����4k�������ռ��ַ
			return ptr;
		}
		else //����64k���µĿռ��ȥthreadcache����ȥ��
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

//�ͷ��ڴ�ʱ��ֻ���õ����ͷſռ�ĵ�ַ����
//����Ǵ��ڴ棬�û���ֱ�ӻ����ںˣ����ڴ���span�ķ�ʽ���棬ͨ����ַ�õ�span����ͨ����ҳ������ת����SystemFree������������ϵͳ
//�����С�ڴ棬�û���һ��̻߳�������������У�����Deallocate�������ú����ĵڶ���������С�ڴ��С������span��_objsize��
static void ConcurrentFree(void* ptr) //�ṩ���û��ͷ��ڴ�Ľӿ�
{
	try
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objsize; //span��С�ڴ��С

		if (size > MAX_BYTES) //�ͷŴ���ڴ�ֱ���ͷŸ�pagecache
		{
			// PageCache
			PageCache::GetInstance()->ReleaseSpanToPageCache(span); //���ﻹ�������Ǵ����û�б��и�
		}
		else //�ͷ�С���ڴ��ͷŸ�threadcache
		{
			assert(tls_threadcache); //С���ڴ��tls����Ϊ�գ��������ڼ�����ڴ棬��Ϊ����ڴ�û�����̻߳���
			tls_threadcache->Deallocate(ptr, size);
		}
	}

	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
}