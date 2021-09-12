#pragma once

//该头文件里面存放各个文件要包含的一些公共数据。

#include <iostream>
#include <exception>
#include <vector>
#include <time.h>
#include <assert.h>
#include <map>
#include <unordered_map>

#include <thread>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
	// ..
#endif

using std::cout;
using std::endl;

//这里定义的都是const修饰的常变量，而非宏，原因是宏不方便调试
static const size_t MAX_BYTES = 64 * 1024; //64k,申请64k以下的空间就去threadcache里面去找
static const size_t NFREELISTS = 184; //线程缓存及中心缓存哈希表长度，分梯度计算结果
static const size_t NPAGES = 129; //页缓存哈希表长度，按照页数多少分为129个等级，1表示span有一页，128表示span有128页
static const size_t PAGE_SHIFT = 12; //位运算位数，左移12位等于除以4k

//处理跨平台数据类型
//64位平台下，整型的大小可能不够保存一些数据（比如地址）
#ifdef _WIN32
typedef size_t ADDRES_INT;
#else
typedef unsigned long long ADDRES_INT;
#endif // _WIN32


// 2 ^ 32 / 2 ^ 12
#ifdef _WIN32
typedef size_t PageID; //32位平台下，页号用无符号整型表示
#else
typedef unsigned long long ADDRES_INT;//64位系统下，size_t可能不够保存页的数量，共有2 ^ 64 / 2 ^ 12页，此时页号就要用long long来表示
#endif // _WIN32

//内联函数，在调用地方直接展开，没有栈帧消耗。以空间换取时间，提高效率。
inline void*& NextObj(void* obj)//返回自由链表中obj指向空间指针域的引用
{	
	//由于不同平台下指针的大小是不相同的，所以不能确定是取前4个字节还是前8个字节来充当指针域
	return *((void**)obj);//这里所采取的做法是，通过先强转成void**，再解引用，从这块被释放的空间里拿出一个void*大小的空间。
}

// 管理对齐和映射等关系
class SizeClass
{
public:
	// 控制在1%-12%左右的内碎片浪费
	// [1,128]					8byte对齐	     freelist[0,16)
	// [129,1024]				16byte对齐		 freelist[16,72)
	// [1025,8*1024]			128byte对齐	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)

	//小于64k空间，找线程缓存，线程缓存总共建了184个桶，由于哈希表数组下标从0开始，所以在结构上开是0-183

	static inline size_t _RoundUp(size_t bytes, size_t align) //align是对齐数
	{
		return (((bytes)+align - 1) & ~(align - 1)); //根据对齐规则计算实际申请空间公式
		//拿[1,8]内对齐数举例，align值为8，加7范围变为[8,15] 
		//7取反为11111000，后三位为0，其他位全1，再按位与等于把后三位变为0
		//所以& ~(align - 1))等价于除8（对齐数）
	}

	// 对齐大小计算，浪费大概在1%-12%左右
	//把bytes转成对齐数，比放说bytes是5，其对齐数必须是8
	static inline size_t RoundUp(size_t bytes) //根据对齐规则计算bytes大小实际实际申请到的空间
	{
		if (bytes <= 128) {
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024) {
			return  _RoundUp(bytes, 16);
		}
		else if (bytes <= 8192) {
			return  _RoundUp(bytes, 128);
		}
		else if (bytes <= 65536) {
			return  _RoundUp(bytes, 1024);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT); //大于64k， 对齐数为4k
			//举例：申65k，实际上申请到的就是68K		
		}

		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift) //用于index函数调用的子函数，直接计算bytes映射的是哪一个桶
	{
		// align_shift的值是bytes是2的多少次方
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1; //公式
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 }; //各梯度，占用哈希表的长度
		
		// 计算各梯度被映射的位置
		//分层去算
		if (bytes <= 128) 
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) 
		{
			return _Index(bytes - 128, 4) + group_array[0]; //该层的0-128字节是按8字节对齐的，所以计算的时候先减去128，计算129-1024的值，该层对齐数16，故_index参数2传4
			//因为这里我们是单独把129-1024这块区间拿出来带公式，所以这里的_Index函数计算的是bytes这个值是129-1024这段区间的第几个桶，
			//但是桶的位置是往后递增的，129-1024的桶在0-128桶的后面，所以计算出来的位置还要加上0-128桶的个数
		}
		else if (bytes <= 8192) 
		{
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 65536) 
		{
			return _Index(bytes - 8192, 10) + group_array[2] + group_array[1] + group_array[0];
		}

		assert(false);

		return -1;
	}

	// 一次从中心缓存获取多少个对象的(慢启动)上限值
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		int num = MAX_BYTES / size; 
		
		//如果size较大，保证至少给两个
		if (num < 2) //32k-64k
			num = 2;

		//如果size较小，最多给512个
		if (num > 512) //0-128字节
			num = 512;

		//该返回值是处于效率考量的
		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 64KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size); //需要获取多少个对象
		size_t npage = num * size; //需要获取对象的总大小

		npage >>= 12; // 除以4k，得到需要获取对象的页数
		if (npage == 0) //控制至少给外界获取一个
			npage = 1;

		return npage;
	}
};

class FreeList //自由链表，链表上“结点”大小相同，维护被释放且大小相同的“同一类”空间
{
public:
	void PushRange(void* start, void* end, int n) //往自由链表中插入一段空间（n个对象），这段空间是从中心缓存获取到的空间剩下的
	{
		NextObj(end) = _head;
		_head = start;
		_size += n; //插入一段空间，自由链表长度增大，这里是为了给中心缓存还空间做铺垫，长度足够长时就像中心缓存还空间
	}

	void PopRange(void*& start, void*& end, int n) //从自由链表中移除一段空间（n个对象），这段空间是还给中心缓存的
	{
		start = _head;
		for (int i = 0; i < n; ++i) //循环n次，移除n个对象
		{
			end = _head; //需要找打end位置
			_head = NextObj(_head);
		}

		NextObj(end) = nullptr; //将end的指针域置空，至此这段空间与自由链表彻底没有联系
		_size -= n; //自由链表中的有效对象数减n
	}

	// 头插，被释放的空间
	void Push(void* obj)
	{
		NextObj(obj) = _head;
		_head = obj;//自由链表的起始位置变为刚删除的空间
		_size += 1;
	}

	// 头删, 有人申请空间，从自由链表中取一块
	void* Pop()
	{
		void* obj = _head;
		_head = NextObj(_head); //简单处理头结点
		_size -= 1;

		return obj; //申请空间的效果和malloc函数一样，要返回申请到空间的返回值，所以此处需返回头结点指针
	}

	bool Empty() //判断自由链表是否为空
	{
		return _head == nullptr;
	}

	size_t MaxSize() //获取慢启动因子，找中心缓存要空间时为了提高效率，一次要多个对象的
	{
		return _max_size;
	}

	void SetMaxSize(size_t n) //重置慢启动因子
	{
		_max_size = n;
	}

	size_t Size() //返回自由链表长度
	{
		return _size;
	}

private:
	void* _head = nullptr; //自由链表的头结点
	size_t _max_size = 1; //慢启动因子
	size_t _size = 0; //自由链表长度，释放的时候需要用来判断自由链表上挂的空间是不是太长
};

////////////////////////////////////////////////////////////
// 管理一个跨度的大块内存，以页为单位，用于给线程缓存分配空间
struct Span 
{
	//这两个变量给页缓存用，用来切割和合并整块空间
	PageID _pageId = 0;   // 开始的页号，为了合并
	size_t _n = 0;        // 起始页开始，页的数量

	//Span结点用双向链表管理
	Span* _next = nullptr;
	Span* _prev = nullptr;

	//这两个变量给中心缓存用，把中心缓存内部切成自由链表，方便给线程缓存分配空间，以及把回收的空间还给页缓存
	void* _list = nullptr; // 大块内存切小链接起来，这样回收回来的内存也方便链接
	size_t _usecount = 0;  // 使用计数，计算span往外分割了多少小空间 ==0 说明所有对象都回来了

	size_t _objsize = 0;   // 切出来的单个对象的大小
	//span里面的大内存是针对某个特定大小去切的，要么全切成8字节，要么全切成16字节，不能既切8又切16。
};

class SpanList //管理切割相同大小span的带头双向链表
{
public:
	SpanList()
	{
		_head = new Span; //带头双向循环链表
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	void PushFront(Span* span) //span链表前插
	{
		Insert(Begin(), span);
	}

	Span* PopFront() //span链表头删，需要带上返回值，删掉的空间要交给别人用
	{
		Span* span = Begin();
		Erase(span);

		return span; 
	}

	void Insert(Span* cur, Span* newspan)
	{
		Span* prev = cur->_prev;
		// prev newspan cur
		prev->_next = newspan;
		newspan->_prev = prev;

		newspan->_next = cur;
		cur->_prev = newspan;
	}

	void Erase(Span* cur)
	{
		assert(cur != _head);

		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
		//删除的结点不用释放，等span孔径全部还回来之后，还要还给pagecache
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

private:
	Span* _head;

public:

	//在对中心缓存加锁时，应该是对每一个桶加锁，实现成桶锁
	//不同线程访问不同的桶时，不会发生冲突，所以不需要加锁
	//所以在一个桶（spanlist）内定义一把锁
	std::mutex _mtx;
};

inline static void* SystemAlloc(size_t kpage) //向系统申请空间
{
#ifdef _WIN32 //条件编译，这里针对的是32位平台下的处理
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), //PAGE_SHIFT是12，左移12等于乘以4k
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr) //向系统释放空间
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}