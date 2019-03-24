#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//#include <cstdlib>

//申请内存
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXSIZE)
	{
		//return malloc(size);

		//先算出对齐数,以页为单位
		size_t roundsize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t npage = roundsize >> PAGE_SHIFT;  //算出多少页

		//超过128页也得管  就把他扔到map中
		Span* span = PageCache::GetInstance()->NewSpan(npage);
		//++span->_objsize;
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//通过tls获取线程自己的thread cache
		if (tls_threadcache == nullptr)
		{
			//cout << std::this_thread::get_id() << "->" << tls_threadcache << endl;
			tls_threadcache = new ThreadCache;
		}
		return tls_threadcache->Allocate(size);
	}

}
static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;

	if (size > MAXSIZE)
	{
		//return free(ptr);
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	}
	else
	{
		tls_threadcache->Deallocate(ptr, size);
	}
}