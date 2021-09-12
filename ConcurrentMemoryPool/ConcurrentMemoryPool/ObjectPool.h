#pragma once

#include"common.h"

//template<size_t  size> //C���Ե�д����ģ�������һ������Ĵ�С��ֻ�������ڴ档ȱ���ǲ��ܳ�ʼ���ڴ档
//class ObjectPool
//{
//
//};

template<class T> //ʵ��һ�������ģ�壬����������͡�C++д���������͵ĺô��ǣ�֪������֮����������ռ�֮�󣬿����ö�λnew��ʽ���ø����͵Ĺ��캯����ʼ��һ�¡�
class ObjectPool
{
public:
	~ObjectPool()
	{
		// ...
	}

	void*& NextObj(void* obj) //�������������е�objָ���������
	{
		//ƽ������ά��һ������ṹ������һ�����Ľṹ��ȥά���ģ����ﻹ��Ҫ����һ�����Ľṹ��ȥά����
		//���ṹ��ĺ������뱣��һ��val�����������������û�б�Ҫ���val
		//����ͷŻ����Ŀռ䱾��Ϳ��Գ䵱�������Ľṹ����ǰ4��32λ������8��64λ�����ֽڿ��Դ��ָ����һ��λ�õĵ�ַ
		//Ҳ����˵���ռ����ø����ã��㻹����֮���һ�Ҫ�����ռ䱾��ȥ���ַ��ά������ṹ�����ý��������ռ俪����
		//���ڲ�ͬƽ̨��ָ��Ĵ�С�ǲ���ͬ�ģ����Բ���ȷ����ȡǰ4���ֽڻ���ǰ8���ֽ����䵱ָ����
		return *((void**)obj); //��������ȡ�������ǣ�ͨ����ǿת��void**���ٽ����ã�����鱻�ͷŵĿռ����ó�һ��void*��С�Ŀռ䡣
		//��Ϊָ���������ǣ�������֮��ȡ�����ֽڵĿռ䡣����*����int*��obj���õ��ľ���һ��int��С�Ŀռ䣬���� *((void**)obj)�õ�����һ��void*��С�Ŀռ�
		//����32λƽ̨�ó����ľ���4�ֽڣ�64λƽ̨���ó����ľ���8�ֽ�
		//�������ҪĿ�����ó�һ��ָ��Ŀռ䣬����������char**����int**���ǿ��Ե�
	}

	T* New() //�����ڴ�
	{
		T* obj = nullptr; //�������ڴ�ķ���ֵ���൱��malloc�����ķ���ֵ
		if (_freeList) //�����������Ϊ�գ���ȥ������������ȥ�����ڴ�
		{
			//�˴��൱��ͷɾ
			obj = (T*)_freeList; //_freeList��������void*����ֵ��ʱ��ǵ�ǿת
			//_freeList = *((void**)_freeList);
			_freeList = NextObj(_freeList);
		}
		else //��������Ϊ�գ�ȥ_memoryָ��Ŀռ�����ȥ����
		{
			if (_leftSize < sizeof(T)) //���_memoryָ��Ŀռ��ڴ治������ʱ��Ҫ��չ�ռ�
			{
				_leftSize = 1024 * 100;
				_memory = (char*)malloc(_leftSize); //�¿��ٳ���һ��ռ�
				if (_memory == nullptr) //�ռ俪��ʧ��
				{
					//exit(-1);
					//cout << "malloc fail" << endl;
					throw std::bad_alloc(); //���쳣��bad_alloc�ǿ���������쳣�ڴ�����ʧ�ܵ�����
				}
			}
			//�������һ������ȷ��_memoryָ���λ�ã����㹻�Ŀռ�ȥ��

			obj = (T*)_memory; //ͨ������ǿת���ָ������T����Ŀռ�
			_memory += sizeof(T); //�ռ�ָ��ȥ֮��_memoryָ���λ��Խ���ָ��ȥ�Ŀռ�
			_leftSize -= sizeof(T); //����ʣ��ռ��С
			//ͨ���˴����߼������жϳ����ָ��ȥ�����ռ䣬������ṹ�Ͽ������Ǹ�λ�ã�ֻ�Ǵ��߼��ṹ�ĽǶ�ȥ����������ڴ��Ѿ�������_memoryָ��Ŀռ���
		}

		new(obj)T; //ͨ����λnew�����뵽���ڴ棬����T���͵�Ĭ�Ϲ��캯����ʼ��
		return obj;
	}

	void Delete(T* obj) //�ͷ��ڴ溯��
	{
		obj->~T(); //�ȵ�������T����������

		// ͷ�鵽freeList
		//*((int*)obj) = (int)_freeList;
		//*((void**)obj) = _freeList;

		//��ɾ�����Ŀռ�ͷ�嵽����������
		NextObj(obj) = _freeList; //��ɾ�ռ����һ�����ָ���ʱ�����������ʼλ�ã�NextObj�����������Ƿ���obj��ָ����ʵ�����Ǳ�ɾ�ռ��ǰ4��32λ������8��64λ�����ֽڣ�

		_freeList = obj; //�����������ʼλ�ñ�Ϊ��ɾ���Ŀռ�
	}

private:
	//����ĳ�Ա����������ȱʡֵ��Ŀ����Ϊ����Ĭ�Ϲ��캯��ȥ����
	char* _memory = nullptr; //ָ���ڴ����������Ŀռ䣬��char*ָ��ָ������ԭ���ǣ��ڷ���ռ��и�ʱ��_memoryֱ�Ӽ���������ռ�Ĵ�С�����ɽ��ÿռ�ָ��ȥ���˴���ͼ�Ƚ�������⣩
	int   _leftSize = 0; //_memoryʣ��ռ�Ĵ�С���������Ʒ����ڴ�ı߽�
	void* _freeList = nullptr; //ָ����������ʹ�ù��Ŀռ���ڸ����������ϣ��´�����ռ�ʱ�������������Ϊ�գ���ֱ�ӵ�����������ȡ�ռ䣬������ȥ_memory�Ϸָʵ�ֿռ���ظ�����
};

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	/*ObjectPool<TreeNode> tnPool;
	std::vector<TreeNode*> v;
	for (size_t i = 0; i < 100; ++i)
	{
		TreeNode* node = tnPool.New();
		cout << node << endl;
		v.push_back(node);
	}

	for (auto e : v)
	{
		tnPool.Delete(e);
	}*/

	/*ObjectPool<TreeNode> tnPool;
	TreeNode* node1 = tnPool.New();
	TreeNode* node2 = tnPool.New();
	TreeNode* node3 = tnPool.New();
	TreeNode* node4 = tnPool.New();
	cout << node1 << endl;
	cout << node2 << endl;
	cout << node3 << endl;
	cout << node4 << endl;
	tnPool.Delete(node1);
	tnPool.Delete(node4);


	TreeNode* node10 = tnPool.New();
	cout << node10 << endl;

	TreeNode* node11 = tnPool.New();
	cout << node11 << endl;*/

	size_t begin1 = clock();
	std::vector<TreeNode*> v1;
	for (int i = 0; i < 100000; ++i)
	{
		v1.push_back(new TreeNode);
	}
	for (int i = 0; i < 100000; ++i)
	{
		delete v1[i];
	}
	v1.clear();

	for (int i = 0; i < 100000; ++i)
	{
		v1.push_back(new TreeNode);
	}

	for (int i = 0; i < 100000; ++i)
	{
		delete v1[i];
	}
	v1.clear();
	size_t end1 = clock();


	ObjectPool<TreeNode> tnPool;
	size_t begin2 = clock();
	std::vector<TreeNode*> v2;
	for (int i = 0; i < 100000; ++i)
	{
		v2.push_back(tnPool.New());
	}
	for (int i = 0; i < 100000; ++i)
	{
		tnPool.Delete(v2[i]);
	}
	v2.clear();

	for (int i = 0; i < 100000; ++i)
	{
		v2.push_back(tnPool.New());
	}
	for (int i = 0; i < 100000; ++i)
	{
		tnPool.Delete(v2[i]);
	}
	v2.clear();

	size_t end2 = clock();

	cout << end1 - begin1 << endl;
	cout << end2 - begin2 << endl;


	ObjectPool<char> chPool;

}