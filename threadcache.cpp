#include "ThreadCache.h"
#include "CentralCache.h"

// �� central cache ��ȡ����
void* ThreadCache::FetchFromCentralCache(size_t index, size_t bytes)
{
	assert(bytes <= MAXSIZE);

	FreeList* freelist = &_freelist[index];

	//һ���ƶ����ٸ������� num_to_move ����
	size_t num_to_move = min(SizeClass::NumMoveSize(bytes), freelist->MaxSize());   //����������������

	void* start, *end;
	//��������  ���巵�ض��ٸ�����
	size_t fetchnum = CentralCache::GetIntance()->FetchRangeObj(start, end, num_to_move, bytes);
	if (fetchnum > 1)
	{
		freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);
	}
	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}

	return start;
}
//�ͷ��ڴ�
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAXSIZE);

	size_t index = SizeClass::Index(size);
	FreeList* freelist = &_freelist[index];

	freelist->push(ptr);


	//���������������������һ�����������Ļ����ƶ�������ʱ����ʼ���ն���centralcache
	if (freelist->Size() >= freelist->MaxSize())
	{
		ListTooLong(freelist, size);
	}
}

void ThreadCache::ListTooLong(FreeList* freelist, size_t bytes)
{
	void* start = freelist->Clear();
	CentralCache::GetIntance()->ReleaseListToSpans(start, bytes);
}

//�����ڴ�
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXSIZE);

	//������� 
	//����ȡ��
	size = SizeClass::RoundUp(size);

	//����֪��ȡ�����ڴ�����freelist����һ��λ��
	size_t index = SizeClass::Index(size);

	FreeList* freelist = &_freelist[index];
	if (!freelist->empty())
	{
		return freelist->pop();  //ͷɾ
	}
	else
	{
		return FetchFromCentralCache(index, size);
	}
}