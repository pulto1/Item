#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst; //定义CentralCache类唯一的_sInst对象

//从SpanList或者page cache获取一个span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// 先在spanlist中去找还有内存的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_list) //_list不为空，说明span里面有可以被切割的空间
		{
			return it; //返回还有剩余空间的span
		}

		//到这里说明双向链表头结点全分出去了，再找下一个结点
		it = it->_next;
	}

	// 走到这里代表着span都没有内存了，只能找pagecache
	// 页缓存只会给中心缓存一个span
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	
	// 要到的span需要切分好挂在list中
	char* start = (char*)(span->_pageId << PAGE_SHIFT); //通过span页号可以找到这个span的起始位置，右移12（乘以4k）
	char* end = start + (span->_n << PAGE_SHIFT); //再找到该span的结束位置（右开，不再span内）
	while (start < end)
	{
		char* next = start + size;
		// 头插
		NextObj(start) = span->_list;
		span->_list = start;

		start = next;
	}
	//根据内存对齐规则，这里不存在切割小空间是，出现最后一个小内存不足一个对象的情况

	span->_objsize = size; 

	list.PushFront(span); //切割成功的span插入中心缓存的哈希表中

	return span; //把从页缓存申请的span返回过去
}

//返回值是实际获取到的空间
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)//从中心缓存获取一定数量的对象给thread cache
{
	size_t i = SizeClass::Index(size); //size大小在中心缓存哈希表中的映射位置

	//mtx.lock(); //加锁
	//mtx.unlock(); //解锁
	//用lock_guard对象进行加锁，好处是解决线程异常安全问题
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx); //传入锁的引用，不引用加的就不是同一把锁

	Span* span = GetOneSpan(_spanLists[i], size); //从SpanList或者page cache获取一个span

	// 到这里肯定能够获得一个span，并且这个span里面剩余空间至少有一个线程缓存需要的对象
	
	// ...切割小空间
	// 找到一个有对象的span，有多少给多少
	size_t j = 0;
	start = span->_list;
	void* cur = start;
	void* prev = start;
	//循环走多少次，就找到了多少对象。但是j是从1开始的，所以最终获取的对象个数应该是i - 1
	while (j < n && cur != nullptr) //j从1走到n，获取n个对象。循环提前结束的标志是cur走到空，此时说明span里面的空间不足 
	{
		//头删
		prev = cur;
		cur = NextObj(cur);
		++j;
		span->_usecount++; //把一段空间交给线程缓存之后，使用加一
	}

	span->_list = cur;
	end = prev;
	NextObj(prev) = nullptr;

	return j;
	
}

void CentralCache::ReleaseListToSpans(void* start, size_t byte_size) //往span里面归还小内存
{
	size_t i = SizeClass::Index(byte_size);
	
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	//遍历还回来的小内存，把他们插入到span的自由链表中（注：span的内部结构是自由链表）
	while (start)
	{
		void* next = NextObj(start);

		// 找start内存块属于哪个span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// 把对象插入到span管理的list中
		NextObj(start) = span->_list;
		span->_list = start;
		span->_usecount--; //还回来一块小空间，使用计数--

		// _usecount == 0说明这个span中切出去的大块内存都还回来回来了
		// 此时中心缓存需要把这块空间还给页缓存
		if (span->_usecount == 0)
		{
			_spanLists[i].Erase(span); //先把该span从哈希表映射位置的链表中弹出来
			span->_list = nullptr; //这里的span还是小内存链起来的，所以还回去之前直接把span里面的自由链表置空
			PageCache::GetInstance()->ReleaseSpanToPageCache(span); //中心缓存中完整的span，还给页缓存
		}

		start = next;
	}
}