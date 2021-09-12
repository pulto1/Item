#pragma once

//��ͷ�ļ��������������Ļ������ҳΪ��λ�Ŀռ䣬����ռ䲻���Ҳ���ϵͳ����ռ�

#include "Common.h"
#include "PageMap.h"

//Ϊ�˱�֤ȫ��ֻ��һ��Ψһ��CentralCache��������౻��Ƴɵ���ģʽ
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// ��ϵͳ����kҳ�ڴ�ҵ���������
	void* SystemAllocPage(size_t k);

	//�����Ļ������kҳ�Ŀռ�
	Span* NewSpan(size_t k);

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

private:
	//SpanList���������ͬҳ���Ŀռ䣬���������Ļ������
	SpanList _spanList[NPAGES];	//��ϣ����ҳ��ӳ��

	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap; //����ҳ�ź�span��ӳ���ϵ
	// tcmalloc ������  Ч�ʸ���

	//ҳ�����span���ܻᷢ���и���Բ��������Ļ�������ֻ��Ͱ�����������ҳ�����������
	std::recursive_mutex _mtx; //�ݹ��� 
	//���ﶨ�������recursive_mutex�ݹ黥������������mutex������
	//ԭ�����ڳ�Ա����NewSpan���еݹ飬����û��������ݹ���ú���ʱ����ǰ��ӵ�����δ�ͷţ��ͻ������������
	//��recursive_mutex������ר��Ϊ�ݹ�׼����

private:
	PageCache()
	{}

	PageCache(const PageCache&) = delete;

	// ����
	static PageCache _sInst;
};
