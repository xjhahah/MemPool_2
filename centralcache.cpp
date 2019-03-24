#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_inst;

// 从page cache获取一个span
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{

	//模仿迭代器实现
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			return span;
		}
		span = span->_next;
	}

	//向PageCache 申请出一块新的合适大小小的span
	//申请出 npage 页的 span
	size_t npage = SizeClass::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	//将span的内存切割成一个个bytes大小的对象挂起来
	//通过页号找到地址
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);
	char* end = start + (newspan->_npage << PAGE_SHIFT);
	char* cur = start;

	char* next = cur + bytes;
	//将一大块内存切割成小块内存
	while (next < end)
	{
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + bytes;
	}
	NEXT_OBJ(cur) = nullptr;
	newspan->_objlist = start;
	newspan->_objsize = bytes;
	newspan->_usecount = 0;

	//将newSpan插入到spanlist
	spanlist->PushFront(newspan);

	return newspan;
}

// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t byte_size)
{
	//start = malloc(byte_size*n);  //打桩  用来单元测试
	//end = (char*)start + byte_size * (n - 1);

	//void* cur = start;
	//while (cur<=end)
	//{
	//	void* next = (char*)cur + byte_size;
	//	NEXT_OBJ(cur) = next;
	//	cur = next;
	//}
	//NEXT_OBJ(end) = nullptr;

	size_t index = SizeClass::Index(byte_size);
	SpanList* spanlist = &_spanList[index];

	//对当前桶进行加锁
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, byte_size);

	void* cur = span->_objlist;
	void* prev = cur;
	size_t fetchnum = 0;
	while (cur != nullptr && fetchnum < num)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}

	//好好理解 尤其小心
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	span->_objlist = cur;
	span->_usecount += fetchnum;

	//当一个span为空，移到尾上
	if (span->_objlist == nullptr)
	{
		spanlist->Erease(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

//将list还给Span
void CentralCache::ReleaseListToSpans(void*start, size_t byte)
{
	size_t index = SizeClass::Index(byte);
	SpanList* spanlist = &_spanList[index];

	//RAII思想
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//当释放对象回到空的span，把空的span移到头上，
		if (span->_objlist == nullptr)
		{
			spanlist->Erease(span);
			spanlist->PushFront(span);
		}

		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		// usecount == 0表示span切出去的对象都还回来了
		// 释放span回到pagecache进行合并
		if (--span->_usecount == 0)
		{
			spanlist->Erease(span);

			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = nullptr;
			span->_next = nullptr;

			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		start = next;
	}
}