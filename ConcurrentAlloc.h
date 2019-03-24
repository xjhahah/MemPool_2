#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//#include <cstdlib>

//�����ڴ�
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXSIZE)
	{
		//return malloc(size);

		//�����������,��ҳΪ��λ
		size_t roundsize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t npage = roundsize >> PAGE_SHIFT;  //�������ҳ

		//����128ҳҲ�ù�  �Ͱ����ӵ�map��
		Span* span = PageCache::GetInstance()->NewSpan(npage);
		//++span->_objsize;
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//ͨ��tls��ȡ�߳��Լ���thread cache
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