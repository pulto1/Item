#pragma once

//��ͷ�ļ������Ÿ����ļ�Ҫ������һЩ�������ݡ�

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

//���ﶨ��Ķ���const���εĳ����������Ǻ꣬ԭ���Ǻ겻�������
static const size_t MAX_BYTES = 64 * 1024; //64k,����64k���µĿռ��ȥthreadcache����ȥ��
static const size_t NFREELISTS = 184; //�̻߳��漰���Ļ����ϣ���ȣ����ݶȼ�����
static const size_t NPAGES = 129; //ҳ�����ϣ���ȣ�����ҳ�����ٷ�Ϊ129���ȼ���1��ʾspan��һҳ��128��ʾspan��128ҳ
static const size_t PAGE_SHIFT = 12; //λ����λ��������12λ���ڳ���4k

//�����ƽ̨��������
//64λƽ̨�£����͵Ĵ�С���ܲ�������һЩ���ݣ������ַ��
#ifdef _WIN32
typedef size_t ADDRES_INT;
#else
typedef unsigned long long ADDRES_INT;
#endif // _WIN32


// 2 ^ 32 / 2 ^ 12
#ifdef _WIN32
typedef size_t PageID; //32λƽ̨�£�ҳ�����޷������ͱ�ʾ
#else
typedef unsigned long long ADDRES_INT;//64λϵͳ�£�size_t���ܲ�������ҳ������������2 ^ 64 / 2 ^ 12ҳ����ʱҳ�ž�Ҫ��long long����ʾ
#endif // _WIN32

//�����������ڵ��õط�ֱ��չ����û��ջ֡���ġ��Կռ任ȡʱ�䣬���Ч�ʡ�
inline void*& NextObj(void* obj)//��������������objָ��ռ�ָ���������
{	
	//���ڲ�ͬƽ̨��ָ��Ĵ�С�ǲ���ͬ�ģ����Բ���ȷ����ȡǰ4���ֽڻ���ǰ8���ֽ����䵱ָ����
	return *((void**)obj);//��������ȡ�������ǣ�ͨ����ǿת��void**���ٽ����ã�����鱻�ͷŵĿռ����ó�һ��void*��С�Ŀռ䡣
}

// ��������ӳ��ȹ�ϵ
class SizeClass
{
public:
	// ������1%-12%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	     freelist[0,16)
	// [129,1024]				16byte����		 freelist[16,72)
	// [1025,8*1024]			128byte����	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)

	//С��64k�ռ䣬���̻߳��棬�̻߳����ܹ�����184��Ͱ�����ڹ�ϣ�������±��0��ʼ�������ڽṹ�Ͽ���0-183

	static inline size_t _RoundUp(size_t bytes, size_t align) //align�Ƕ�����
	{
		return (((bytes)+align - 1) & ~(align - 1)); //���ݶ���������ʵ������ռ乫ʽ
		//��[1,8]�ڶ�����������alignֵΪ8����7��Χ��Ϊ[8,15] 
		//7ȡ��Ϊ11111000������λΪ0������λȫ1���ٰ�λ����ڰѺ���λ��Ϊ0
		//����& ~(align - 1))�ȼ��ڳ�8����������
	}

	// �����С���㣬�˷Ѵ����1%-12%����
	//��bytesת�ɶ��������ȷ�˵bytes��5���������������8
	static inline size_t RoundUp(size_t bytes) //���ݶ���������bytes��Сʵ��ʵ�����뵽�Ŀռ�
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
			return _RoundUp(bytes, 1 << PAGE_SHIFT); //����64k�� ������Ϊ4k
			//��������65k��ʵ�������뵽�ľ���68K		
		}

		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift) //����index�������õ��Ӻ�����ֱ�Ӽ���bytesӳ�������һ��Ͱ
	{
		// align_shift��ֵ��bytes��2�Ķ��ٴη�
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1; //��ʽ
	}

	// ����ӳ�����һ����������Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 56 }; //���ݶȣ�ռ�ù�ϣ��ĳ���
		
		// ������ݶȱ�ӳ���λ��
		//�ֲ�ȥ��
		if (bytes <= 128) 
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) 
		{
			return _Index(bytes - 128, 4) + group_array[0]; //�ò��0-128�ֽ��ǰ�8�ֽڶ���ģ����Լ����ʱ���ȼ�ȥ128������129-1024��ֵ���ò������16����_index����2��4
			//��Ϊ���������ǵ�����129-1024��������ó�������ʽ�����������_Index�����������bytes���ֵ��129-1024�������ĵڼ���Ͱ��
			//����Ͱ��λ������������ģ�129-1024��Ͱ��0-128Ͱ�ĺ��棬���Լ��������λ�û�Ҫ����0-128Ͱ�ĸ���
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

	// һ�δ����Ļ����ȡ���ٸ������(������)����ֵ
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		int num = MAX_BYTES / size; 
		
		//���size�ϴ󣬱�֤���ٸ�����
		if (num < 2) //32k-64k
			num = 2;

		//���size��С������512��
		if (num > 512) //0-128�ֽ�
			num = 512;

		//�÷���ֵ�Ǵ���Ч�ʿ�����
		return num;
	}

	// ����һ����ϵͳ��ȡ����ҳ
	// �������� 8byte
	// ...
	// �������� 64KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size); //��Ҫ��ȡ���ٸ�����
		size_t npage = num * size; //��Ҫ��ȡ������ܴ�С

		npage >>= 12; // ����4k���õ���Ҫ��ȡ�����ҳ��
		if (npage == 0) //�������ٸ�����ȡһ��
			npage = 1;

		return npage;
	}
};

class FreeList //�������������ϡ���㡱��С��ͬ��ά�����ͷ��Ҵ�С��ͬ�ġ�ͬһ�ࡱ�ռ�
{
public:
	void PushRange(void* start, void* end, int n) //�����������в���һ�οռ䣨n�����󣩣���οռ��Ǵ����Ļ����ȡ���Ŀռ�ʣ�µ�
	{
		NextObj(end) = _head;
		_head = start;
		_size += n; //����һ�οռ䣬��������������������Ϊ�˸����Ļ��滹�ռ����̵棬�����㹻��ʱ�������Ļ��滹�ռ�
	}

	void PopRange(void*& start, void*& end, int n) //�������������Ƴ�һ�οռ䣨n�����󣩣���οռ��ǻ������Ļ����
	{
		start = _head;
		for (int i = 0; i < n; ++i) //ѭ��n�Σ��Ƴ�n������
		{
			end = _head; //��Ҫ�Ҵ�endλ��
			_head = NextObj(_head);
		}

		NextObj(end) = nullptr; //��end��ָ�����ÿգ�������οռ�������������û����ϵ
		_size -= n; //���������е���Ч��������n
	}

	// ͷ�壬���ͷŵĿռ�
	void Push(void* obj)
	{
		NextObj(obj) = _head;
		_head = obj;//�����������ʼλ�ñ�Ϊ��ɾ���Ŀռ�
		_size += 1;
	}

	// ͷɾ, ��������ռ䣬������������ȡһ��
	void* Pop()
	{
		void* obj = _head;
		_head = NextObj(_head); //�򵥴���ͷ���
		_size -= 1;

		return obj; //����ռ��Ч����malloc����һ����Ҫ�������뵽�ռ�ķ���ֵ�����Դ˴��践��ͷ���ָ��
	}

	bool Empty() //�ж����������Ƿ�Ϊ��
	{
		return _head == nullptr;
	}

	size_t MaxSize() //��ȡ���������ӣ������Ļ���Ҫ�ռ�ʱΪ�����Ч�ʣ�һ��Ҫ��������
	{
		return _max_size;
	}

	void SetMaxSize(size_t n) //��������������
	{
		_max_size = n;
	}

	size_t Size() //��������������
	{
		return _size;
	}

private:
	void* _head = nullptr; //���������ͷ���
	size_t _max_size = 1; //����������
	size_t _size = 0; //���������ȣ��ͷŵ�ʱ����Ҫ�����ж����������ϹҵĿռ��ǲ���̫��
};

////////////////////////////////////////////////////////////
// ����һ����ȵĴ���ڴ棬��ҳΪ��λ�����ڸ��̻߳������ռ�
struct Span 
{
	//������������ҳ�����ã������и�ͺϲ�����ռ�
	PageID _pageId = 0;   // ��ʼ��ҳ�ţ�Ϊ�˺ϲ�
	size_t _n = 0;        // ��ʼҳ��ʼ��ҳ������

	//Span�����˫���������
	Span* _next = nullptr;
	Span* _prev = nullptr;

	//���������������Ļ����ã������Ļ����ڲ��г���������������̻߳������ռ䣬�Լ��ѻ��յĿռ仹��ҳ����
	void* _list = nullptr; // ����ڴ���С�����������������ջ������ڴ�Ҳ��������
	size_t _usecount = 0;  // ʹ�ü���������span����ָ��˶���С�ռ� ==0 ˵�����ж��󶼻�����

	size_t _objsize = 0;   // �г����ĵ�������Ĵ�С
	//span����Ĵ��ڴ������ĳ���ض���Сȥ�еģ�Ҫôȫ�г�8�ֽڣ�Ҫôȫ�г�16�ֽڣ����ܼ���8����16��
};

class SpanList //�����и���ͬ��Сspan�Ĵ�ͷ˫������
{
public:
	SpanList()
	{
		_head = new Span; //��ͷ˫��ѭ������
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

	void PushFront(Span* span) //span����ǰ��
	{
		Insert(Begin(), span);
	}

	Span* PopFront() //span����ͷɾ����Ҫ���Ϸ���ֵ��ɾ���Ŀռ�Ҫ����������
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
		//ɾ���Ľ�㲻���ͷţ���span�׾�ȫ��������֮�󣬻�Ҫ����pagecache
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

private:
	Span* _head;

public:

	//�ڶ����Ļ������ʱ��Ӧ���Ƕ�ÿһ��Ͱ������ʵ�ֳ�Ͱ��
	//��ͬ�̷߳��ʲ�ͬ��Ͱʱ�����ᷢ����ͻ�����Բ���Ҫ����
	//������һ��Ͱ��spanlist���ڶ���һ����
	std::mutex _mtx;
};

inline static void* SystemAlloc(size_t kpage) //��ϵͳ����ռ�
{
#ifdef _WIN32 //�������룬������Ե���32λƽ̨�µĴ���
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), //PAGE_SHIFT��12������12���ڳ���4k
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr) //��ϵͳ�ͷſռ�
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}