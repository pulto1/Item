#pragma once

//��ͷ�ļ�������ÿ���߳�ӵ�ж������̻߳���

#include "Common.h"


class ThreadCache
{
public:
	// ������ͷ��ڴ����
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);

	// �ͷŶ���ʱ�������������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];//��ϣͰ��ӳ�䲻ͬ���͵���������Ͱ�������ǻ������Ŀռ䡣����ռ���Դӹ�ϣͰ�����룬�ͷŵĿռ佻����ϣͰȥά��
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr; //����һ��ȫ�ֵ�threadcache����ָ�룬threadcache��ʹȫ�ֵģ���Ҳ��ÿ���̶߳��еġ�