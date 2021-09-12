#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst; //����CentralCache��Ψһ��_sInst����

//��SpanList����page cache��ȡһ��span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// ����spanlist��ȥ�һ����ڴ��span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_list) //_list��Ϊ�գ�˵��span�����п��Ա��и�Ŀռ�
		{
			return it; //���ػ���ʣ��ռ��span
		}

		//������˵��˫������ͷ���ȫ�ֳ�ȥ�ˣ�������һ�����
		it = it->_next;
	}

	// �ߵ����������span��û���ڴ��ˣ�ֻ����pagecache
	// ҳ����ֻ������Ļ���һ��span
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	
	// Ҫ����span��Ҫ�зֺù���list��
	char* start = (char*)(span->_pageId << PAGE_SHIFT); //ͨ��spanҳ�ſ����ҵ����span����ʼλ�ã�����12������4k��
	char* end = start + (span->_n << PAGE_SHIFT); //���ҵ���span�Ľ���λ�ã��ҿ�������span�ڣ�
	while (start < end)
	{
		char* next = start + size;
		// ͷ��
		NextObj(start) = span->_list;
		span->_list = start;

		start = next;
	}
	//�����ڴ����������ﲻ�����и�С�ռ��ǣ��������һ��С�ڴ治��һ����������

	span->_objsize = size; 

	list.PushFront(span); //�и�ɹ���span�������Ļ���Ĺ�ϣ����

	return span; //�Ѵ�ҳ���������span���ع�ȥ
}

//����ֵ��ʵ�ʻ�ȡ���Ŀռ�
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)//�����Ļ����ȡһ�������Ķ����thread cache
{
	size_t i = SizeClass::Index(size); //size��С�����Ļ����ϣ���е�ӳ��λ��

	//mtx.lock(); //����
	//mtx.unlock(); //����
	//��lock_guard������м������ô��ǽ���߳��쳣��ȫ����
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx); //�����������ã������üӵľͲ���ͬһ����

	Span* span = GetOneSpan(_spanLists[i], size); //��SpanList����page cache��ȡһ��span

	// ������϶��ܹ����һ��span���������span����ʣ��ռ�������һ���̻߳�����Ҫ�Ķ���
	
	// ...�и�С�ռ�
	// �ҵ�һ���ж����span���ж��ٸ�����
	size_t j = 0;
	start = span->_list;
	void* cur = start;
	void* prev = start;
	//ѭ���߶��ٴΣ����ҵ��˶��ٶ��󡣵���j�Ǵ�1��ʼ�ģ��������ջ�ȡ�Ķ������Ӧ����i - 1
	while (j < n && cur != nullptr) //j��1�ߵ�n����ȡn������ѭ����ǰ�����ı�־��cur�ߵ��գ���ʱ˵��span����Ŀռ䲻�� 
	{
		//ͷɾ
		prev = cur;
		cur = NextObj(cur);
		++j;
		span->_usecount++; //��һ�οռ佻���̻߳���֮��ʹ�ü�һ
	}

	span->_list = cur;
	end = prev;
	NextObj(prev) = nullptr;

	return j;
	
}

void CentralCache::ReleaseListToSpans(void* start, size_t byte_size) //��span����黹С�ڴ�
{
	size_t i = SizeClass::Index(byte_size);
	
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	//������������С�ڴ棬�����ǲ��뵽span�����������У�ע��span���ڲ��ṹ����������
	while (start)
	{
		void* next = NextObj(start);

		// ��start�ڴ�������ĸ�span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// �Ѷ�����뵽span�����list��
		NextObj(start) = span->_list;
		span->_list = start;
		span->_usecount--; //������һ��С�ռ䣬ʹ�ü���--

		// _usecount == 0˵�����span���г�ȥ�Ĵ���ڴ涼������������
		// ��ʱ���Ļ�����Ҫ�����ռ仹��ҳ����
		if (span->_usecount == 0)
		{
			_spanLists[i].Erase(span); //�ȰѸ�span�ӹ�ϣ��ӳ��λ�õ������е�����
			span->_list = nullptr; //�����span����С�ڴ��������ģ����Ի���ȥ֮ǰֱ�Ӱ�span��������������ÿ�
			PageCache::GetInstance()->ReleaseSpanToPageCache(span); //���Ļ�����������span������ҳ����
		}

		start = next;
	}
}