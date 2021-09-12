#pragma once

//��ͷ�ļ����м�������ã����̻߳������ռ䣬��ҳ��������ռ䡣���̻߳��滹�����Ŀռ䣬�ϲ��ɴ�ռ��ٻ���ҳ����

#include "Common.h"

//Ϊ�˱�֤ȫ��ֻ��һ��Ψһ��CentralCache��������౻��Ƴɵ���ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstance() //�ṩһ����̬�ĳ�Ա������������ȡ���ȫ��Ψһ�Ķ���
	{
		return &_sInst;
	}

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);

	// ��SpanList����page cache��ȡһ��span
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanLists[NFREELISTS]; // ��ϣͰ��������+�ݶȷ�ʽӳ��


//����ģʽ������
private:
	CentralCache() //��ӡ���캯����ֻ����������Χ�������
	{}

	CentralCache(const CentralCache&) = delete; //��������ֻ�����������壬��������˽��

	static CentralCache _sInst; //��������ֻ��һ��_sInst����
};