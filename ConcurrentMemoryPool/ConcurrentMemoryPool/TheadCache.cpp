#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t i, size_t size) //�̻߳��������Ļ���Ҫ�ռ�
{	
	// ��ȡһ����������ʹ����������ʽ
	size_t batchNum = min(SizeClass::NumMoveSize(size), _freeLists[i].MaxSize()); //batchNum�ǻ�ȡ�������Ŀ��NumMoveSize��MaxSize�Ľ�Сֵ
	//NumMoveSize(size)�����ܻ�ȡ������Ŀ������ֵ
	//MaxSize�����������ӣ���1��ʼ��һ�μ�1

	// ȥ���Ļ����ȡbatch_num������
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, SizeClass::RoundUp(size)); //actualNum��ʵ�ʻ�ȡ�˶��ٸ�����
	//Ҫ��֤���Ļ����е�С��ռ��Ƕ�������RoundUp������sizeת�ɶ�Ӧ�Ķ�����

	assert(actualNum > 0);

	// >1������һ����ʣ�¹ҵ���������
	// ���һ����������ʣ�¹��������´�����Ͳ���Ҫ�����Ļ���
	// ���������������Ч��
	if (actualNum > 1)
	{
		_freeLists[i].PushRange(NextObj(start), end, actualNum - 1); //��һ�οռ��������������
	}

	if (_freeLists[i].MaxSize() == batchNum) //�����ͨ�����������ӵõ������뵽����ĸ���
	{
		_freeLists[i].SetMaxSize(_freeLists[i].MaxSize() + 1); //�������������ӣ��ôοռ�����֮�����������Ӽ�1�������´ξ��ܱȸôζ�����һ������Ŀռ�
	}
	
	//start�Ƿ�����û��Ŀռ�
	return start;
}

void* ThreadCache::Allocate(size_t size) //����ָ����С�Ŀռ�
{
	//��һ���ڹ�ϣ�����ң���ֵӳ���������������û�пռ�
	//����У�ֱ�Ӵ���������������ռ䣬ʵ�ֿռ���ظ�����
	size_t i = SizeClass::Index(size); //index�������ݶȷ���size�ڹ�ϣ���е�ӳ��λ��
	if (!_freeLists[i].Empty())
	{
		return _freeLists[i].Pop(); //��threadcache������������ȡ�ռ䣬ʱ�临�Ӷ�O��1�����Ҳ��ü���
	}
	//�����������Ϊ�գ������Ļ���Ҫ�ռ�
	else
	{
		return FetchFromCentralCache(i, size); //�̻߳��������Ļ���Ҫ�ռ�
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)//�ͷſռ�
{
	size_t i = SizeClass::Index(size);
	_freeLists[i].Push(ptr);

	// �����������ĳ����㹻�����Ͱ���Щ���еĿռ仹�����Ļ���
	if (_freeLists[i].Size() > _freeLists[i].MaxSize())//���ڵ�ǰһ���ܹ���ȡ��������ֵ
	{
		ListTooLong(_freeLists[i], size);
	}
}

// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
void ThreadCache::ListTooLong(FreeList& list, size_t size) //�̻߳��������Ļ��滹�ռ�
{
	size_t batchNum = list.MaxSize(); //һ�ξͻ����������ֵ
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, batchNum); //������������ͷɾ��һ�α��ͷŵĿռ䣬ʵ���ǻ������Ļ�����

	CentralCache::GetInstance()->ReleaseListToSpans(start, size); //���Ļ���ѻ������Ŀռ���뵽��Ӧspan��
}