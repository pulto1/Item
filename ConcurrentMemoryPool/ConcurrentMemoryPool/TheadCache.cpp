#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t i, size_t size) //线程缓存找中心缓存要空间
{	
	// 获取一批对象，数量使用慢启动方式
	size_t batchNum = min(SizeClass::NumMoveSize(size), _freeLists[i].MaxSize()); //batchNum是获取对象的数目，NumMoveSize和MaxSize的较小值
	//NumMoveSize(size)是所能获取对象数目的上限值
	//MaxSize是慢启动因子，从1开始，一次加1

	// 去中心缓存获取batch_num个对象
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, SizeClass::RoundUp(size)); //actualNum是实际获取了多少个对象
	//要保证中心缓存切的小块空间是对齐数，RoundUp函数把size转成对应的对齐数

	assert(actualNum > 0);

	// >1，返回一个，剩下挂到自由链表
	// 如果一次申请多个，剩下挂起来，下次申请就不需要找中心缓存
	// 减少锁竞争，提高效率
	if (actualNum > 1)
	{
		_freeLists[i].PushRange(NextObj(start), end, actualNum - 1); //把一段空间插入自由链表中
	}

	if (_freeLists[i].MaxSize() == batchNum) //如果是通过慢启动因子得到的申请到对象的个数
	{
		_freeLists[i].SetMaxSize(_freeLists[i].MaxSize() + 1); //重置慢启动因子，该次空间申请之后，慢启动因子加1，这样下次就能比该次多申请一个对象的空间
	}
	
	//start是分配给用户的空间
	return start;
}

void* ThreadCache::Allocate(size_t size) //申请指定大小的空间
{
	//第一步在哈希表中找，该值映射的自由链表中有没有空间
	//如果有，直接从自由链表上申请空间，实现空间的重复利用
	size_t i = SizeClass::Index(size); //index函数分梯度返回size在哈希表中的映射位置
	if (!_freeLists[i].Empty())
	{
		return _freeLists[i].Pop(); //从threadcache的自由链表中取空间，时间复杂度O（1），且不用加锁
	}
	//如果自由链表为空，找中心缓存要空间
	else
	{
		return FetchFromCentralCache(i, size); //线程缓存找中心缓存要空间
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)//释放空间
{
	size_t i = SizeClass::Index(size);
	_freeLists[i].Push(ptr);

	// 如果自由链表的长度足够长，就把这些空闲的空间还给中心缓存
	if (_freeLists[i].Size() > _freeLists[i].MaxSize())//大于当前一次能够获取对象的最大值
	{
		ListTooLong(_freeLists[i], size);
	}
}

// 释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t size) //线程缓存向中心缓存还空间
{
	size_t batchNum = list.MaxSize(); //一次就还申请的上限值
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, batchNum); //从自由链表中头删这一段被释放的空间，实际是还给中心缓存了

	CentralCache::GetInstance()->ReleaseListToSpans(start, size); //中心缓存把还回来的空间插入到对应span中
}