#include "PageCache.h"

PageCache PageCache::_sInst;

// ��ϵͳ����kҳ�ڴ�
void* PageCache::SystemAllocPage(size_t k) //ҳ����ռ䲻������ϵͳ����
{
	return ::SystemAlloc(k); //��ȫ�ֺ���
}

Span* PageCache::NewSpan(size_t k)//�����Ļ������kҳ�Ŀռ�
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// ���ֱ���������NPAGES�Ĵ���ڴ棬ֱ����ϵͳҪ
	if (k >= NPAGES) //����ڴ�����һ�����û����ڴ�ֱ�ӽ��������������Ļ����з�
	{
		void* ptr = SystemAllocPage(k);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT; //��ַ����4k
		span->_n = k;

		_idSpanMap[span->_pageId] = span;

		return span;
	}

	//���̻߳�������û�пռ�
	if (!_spanList[k].Empty())//�������kҳspan��Ͱ��Ϊ�գ�ֱ�Ӵӹ�ϣͰ�Ϸָ��һ��span
	{
		return _spanList[k].PopFront(); //ͷɾ
	}

	//������˵��kҳspan��ͰΪ�գ���ʱ������������
	//1.��kӳ��λ�������Ҹ����һ��ҳ�����ָ��kҳ����ʣ�µ�ҳ���ҵ�ӳ���λ����
	for (size_t i = k+1; i < NPAGES; ++i) //��k+1λ���ҵ���ϣ�����һ��λ��
	{
		// ��ҳ����С,����β�У��г���kҳ������һ����span�У����½���ӳ���ϵ����󷵻ظ����Ļ���
		// ʣ��i-kҳ�һع�ϣ���Ӧλ��
		if (!_spanList[i].Empty()) //���ڴ�ҳ
		{
			// β�г�һ��kҳspan
			Span* span = _spanList[i].PopFront(); //�ӹ�ϣ�����ó������ҳ��ͷɾ�������ҳ���и�֮�󲻿����ڱ�����ԭ����λ��

			Span* split = new Span; //��span�������г�����kҳ
			split->_pageId = span->_pageId + span->_n - k; //ά��ҳ��
			split->_n = k; //ά��ҳ��

			// �ı��г���span��ҳ�ź�span��ӳ���ϵ
			for (PageID i = 0; i < k; ++i)
			{
				_idSpanMap[split->_pageId + i] = split; //�����ҳ�ű������ڻ������б����ţ��˴���[]�����޸�ӳ���ϵ
			}

			span->_n -= k; //ά��ʣ��ռ�ҳ����ʣ��ռ�ҳ�Ų���

			_spanList[span->_n].PushFront(span); //��ʣ��ռ���뵽ҳ�����ϣ���Ӧλ�ã�ҳ�����������±�

			return split;
		}
	}

	//2.û�к��ʵĴ�ҳ����ʱ��Ҫ��ϵͳ���룬����һ������һ������ҳ128
	Span* bigSpan = new Span;//�����ҳʣ�µĿռ䣬span
	void* memory = SystemAllocPage(NPAGES - 1); //����128ҳ
	bigSpan->_pageId = (size_t)memory >> 12; //ҳ�ŵı����ǵ�ַ����ҳ�Ĵ�С
	bigSpan->_n = NPAGES - 1; //ҳ����128ҳ

	// ��ҳ�ź�spanӳ���ϵ����
	for (PageID i = 0; i < bigSpan->_n; ++i)
	{
		PageID id = bigSpan->_pageId + i; //�Ѵ���ʼҳ�������127ҳ���������span����ӳ���ϵ
		_idSpanMap[id] = bigSpan;
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan); //��������ҳͷ���ϣ���Ӧλ��

	return NewSpan(k); //��ʱȷ��kҳ����һ���к��ʵĴ�ҳ���ٵݹ����һ���Լ�
}

//��С�ռ��Ӧ��span
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT; //ͨ��С���ڴ�ĵ�ַ����ӳ���spanҳid������12������4k��

	Span* span = _idSpanMap.get(id);
	if (span != nullptr)
	{
		return span;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span) //�����Ļ��滹������span����ǰ��ϲ�
{
	if (span->_n >= NPAGES)//����ڴ��ͷ����û����ڴ�ֱ�ӽ��������������Ļ����з�
	{
		_idSpanMap.erase(span->_pageId);
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// ��ǰ�ϲ�	
	while (1) //���ó�ѭ������Ϊǰһ��span�ϲ�֮�󣬿��ܻ�Ҫ�ϲ�ǰǰһ��span
	{
		PageID preId = span->_pageId - 1; //ǰһ��span����һ��ҳ��id

		Span* preSpan = _idSpanMap.get(preId);//ͨ��ӳ���ϵ�ҵ�ǰһ��span
		// ���ǰһ��ҳ��span�����ڣ�δ���䣬������ǰ�ϲ�
		if (preSpan == nullptr)
		{
			break;
		}

		// ���ǰһ��ҳ��span�������Ļ�������̻߳���ʹ���У�������ǰ�ϲ�
		if (preSpan->_usecount != 0)//ҳ�����п���span���ص���usecount == 0
		{
			break;
		}

		// ����128ҳ������Ҫ�ϲ���
		if (preSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		// ��ʼ�ϲ�..�ϲ����Ե�ǰspanΪ��׼

		// ��ǰһ��span�Ӷ�Ӧ��span�����н��������ٺϲ�
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// ����ҳ֮��ӳ���ϵ
		for (PageID i = 0; i < preSpan->_n; ++i)
		{
			_idSpanMap[preSpan->_pageId + i] = span;
		}

		delete preSpan;
	}

	// ���ϲ�
	while (1)
	{
		PageID nextId = span->_pageId + span->_n; //��һ��span��ʼҳҳ��

		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr)
		{
			break;
		}

		//Span* nextSpan = ret->second;
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// ����128ҳ������Ҫ�ϲ���
		if (nextSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		_spanList[nextSpan->_n].Erase(nextSpan);

		span->_n += nextSpan->_n;
		for (PageID i = 0; i < nextSpan->_n; ++i)
		{
			_idSpanMap[nextSpan->_pageId + i] = span;
		}

		delete nextSpan;
	}

	// �ϲ����Ĵ�span�����뵽ҳ�����Ӧ��������
	_spanList[span->_n].PushFront(span);
}