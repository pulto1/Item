#include "PageCache.h"

PageCache PageCache::_sInst;

// 向系统申请k页内存
void* PageCache::SystemAllocPage(size_t k) //页缓存空间不够，找系统申请
{
	return ::SystemAlloc(k); //调全局函数
}

Span* PageCache::NewSpan(size_t k)//给中心缓存分配k页的空间
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// 针对直接申请大于NPAGES的大块内存，直接找系统要
	if (k >= NPAGES) //大块内存申请一般是用户和内存直接交互，不会往中心缓存中放
	{
		void* ptr = SystemAllocPage(k);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT; //地址除以4k
		span->_n = k;

		_idSpanMap[span->_pageId] = span;

		return span;
	}

	//找线程缓存中有没有空间
	if (!_spanList[k].Empty())//如果保存k页span的桶不为空，直接从哈希桶上分割出一个span
	{
		return _spanList[k].PopFront(); //头删
	}

	//到这里说明k页span的桶为空，此时有两种做法，
	//1.从k映射位置往后找更大的一块页数，分割出k页，把剩下的页数挂到映射的位置上
	for (size_t i = k+1; i < NPAGES; ++i) //从k+1位置找到哈希表最后一个位置
	{
		// 大页给切小,采用尾切，切出的k页保存在一个新span中，重新建立映射关系，最后返回给中心缓存
		// 剩余i-k页挂回哈希表对应位置
		if (!_spanList[i].Empty()) //存在大页
		{
			// 尾切出一个k页span
			Span* span = _spanList[i].PopFront(); //从哈希表中拿出这个大页，头删。这个大页被切割之后不可能在保存在原来的位置

			Span* split = new Span; //新span，保存切出来的k页
			split->_pageId = span->_pageId + span->_n - k; //维护页号
			split->_n = k; //维护页数

			// 改变切出来span的页号和span的映射关系
			for (PageID i = 0; i < k; ++i)
			{
				_idSpanMap[split->_pageId + i] = split; //这里的页号本来就在基数树中保存着，此处的[]用作修改映射关系
			}

			span->_n -= k; //维护剩余空间页数，剩余空间页号不变

			_spanList[span->_n].PushFront(span); //把剩余空间插入到页缓存哈希表对应位置，页数就是数组下标

			return split;
		}
	}

	//2.没有合适的大页，此时需要向系统申请，这里一次申请一块最大的页128
	Span* bigSpan = new Span;//保存大页剩下的空间，span
	void* memory = SystemAllocPage(NPAGES - 1); //申请128页
	bigSpan->_pageId = (size_t)memory >> 12; //页号的本质是地址除以页的大小
	bigSpan->_n = NPAGES - 1; //页数是128页

	// 按页号和span映射关系建立
	for (PageID i = 0; i < bigSpan->_n; ++i)
	{
		PageID id = bigSpan->_pageId + i; //把从起始页号往后的127页都与这个大span建立映射关系
		_idSpanMap[id] = bigSpan;
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan); //把这个最大页头插哈希表对应位置

	return NewSpan(k); //此时确保k页后面一定有合适的大页，再递归调用一下自己
}

//找小空间对应的span
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT; //通过小块内存的地址计算映射的span页id，右移12（除以4k）

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

void PageCache::ReleaseSpanToPageCache(Span* span) //把中心缓存还回来的span进行前后合并
{
	if (span->_n >= NPAGES)//大块内存释放是用户和内存直接交互，不会往中心缓存中放
	{
		_idSpanMap.erase(span->_pageId);
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// 向前合并	
	while (1) //设置成循环，因为前一个span合并之后，可能还要合并前前一个span
	{
		PageID preId = span->_pageId - 1; //前一个span其中一个页的id

		Span* preSpan = _idSpanMap.get(preId);//通过映射关系找到前一个span
		// 如果前一个页的span不存在，未分配，结束向前合并
		if (preSpan == nullptr)
		{
			break;
		}

		// 如果前一个页的span还在中心缓存或者线程缓存使用中，结束向前合并
		if (preSpan->_usecount != 0)//页缓存中空闲span的特点是usecount == 0
		{
			break;
		}

		// 超过128页，不需要合并了
		if (preSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		// 开始合并..合并是以当前span为基准

		// 把前一个span从对应的span链表中解下来，再合并
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// 更新页之间映射关系
		for (PageID i = 0; i < preSpan->_n; ++i)
		{
			_idSpanMap[preSpan->_pageId + i] = span;
		}

		delete preSpan;
	}

	// 向后合并
	while (1)
	{
		PageID nextId = span->_pageId + span->_n; //后一个span起始页页号

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

		// 超过128页，不需要合并了
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

	// 合并出的大span，插入到页缓存对应的链表中
	_spanList[span->_n].PushFront(span);
}