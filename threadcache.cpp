#include "ThreadCache.h"
#include "CentralCache.h"

// 从 central cache 获取对象
void* ThreadCache::FetchFromCentralCache(size_t index, size_t bytes)
{
	assert(bytes <= MAXSIZE);

	FreeList* freelist = &_freelist[index];

	//一次移动多少个对象由 num_to_move 决定
	size_t num_to_move = min(SizeClass::NumMoveSize(bytes), freelist->MaxSize());   //线性增长？？？？

	void* start, *end;
	//单例对象  具体返回多少个对象
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
//释放内存
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAXSIZE);

	size_t index = SizeClass::Index(size);
	FreeList* freelist = &_freelist[index];

	freelist->push(ptr);


	//当自由链表对象数量超过一次批量从中心缓存移动的数量时，开始回收对象到centralcache
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

//申请内存
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXSIZE);

	//对齐规则 
	//对齐取整
	size = SizeClass::RoundUp(size);

	//还得知道取出的内存属于freelist的哪一个位置
	size_t index = SizeClass::Index(size);

	FreeList* freelist = &_freelist[index];
	if (!freelist->empty())
	{
		return freelist->pop();  //头删
	}
	else
	{
		return FetchFromCentralCache(index, size);
	}
}