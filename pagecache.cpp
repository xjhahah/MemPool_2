#include "PageCache.h"

PageCache PageCache::_inst;

Span* PageCache::NewSpan(size_t npage)
{
	// 加锁，防止多个线程同时到PageCache中申请span
	// 这里必须是给全局加锁，不能单独的给每个桶加锁
	// 如果对应桶没有span,是需要向系统申请的
	// 可能存在多个线程同时向系统申请内存的可能
	std::unique_lock<std::mutex> lock(_mtx);

	if (npage >= NPAGES)
	{
		//需要向系统申请内存
		void* ptr = SystemAlloc(npage);

		Span* span = new Span();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = npage;
		span->_objsize = npage << PAGE_SHIFT;

		_id_span_map[span->_pageid] = span;
		return span;
	}

	// 申请64k-128页的内存的时候
	// 需要将span的objsize设置为span的大小
	// 因为向操作系统归还这个span时需要大小
	Span* span = _NewSpan(npage);
	span->_objsize = span->_npage << PAGE_SHIFT;

	return span;
}
Span* PageCache::_NewSpan(size_t npage)
{
	//std::unique_lock<std::mutex> lock(_mtx);  //不支持自动解锁，造成死锁


	//这里的  _pagelist 和 CentralCache里的spanlist 意义不同，这里的_paglist代表的是npage页的spanlist
	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}
	//依次往后挂spanlist，将一个大的pagecache切割成需要的span并将剩下的内存挂到对应的 pagecache 中
	for (size_t i = npage + 1; i < NPAGES; ++i)
	{
		if (!_pagelist[i].Empty())
		{
			// 进行切割
			Span* span = _pagelist[i].PopFront();
			Span* split = new Span();

			// 页号:从span的后面进行切割
			split->_pageid = span->_pageid + span->_npage - npage;
			// 页数
			split->_npage = npage;
			span->_npage = span->_npage - npage;

			// 将新分割出来的页都映射到新的span上
			for (size_t i = 0; i < npage; i++)
			{
				_id_span_map[split->_pageid + i] = split;
			}

			_pagelist[span->_npage].PushFront(span);

			return split;
		}
	}

	//需要向系统申请内存   VirtualAlloc   一般都是大于64K
	//该函数的功能是在调用进程的虚地址空间,预定或者提交一部分页如果用于内存分配的话, 并且分配类型未指定MEM_RESET, 则该内存自动被初始化为0;
	void* ptr = SystemAlloc(128);

	Span* largespan = new Span();
	largespan->_pageid = (PageID)ptr >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;

	_pagelist[NPAGES - 1].PushFront(largespan);

	//页号到span的映射
	for (size_t i = 0; i < largespan->_npage; ++i)
	{
		//这里的 [] 是用来修改里面的值(value的别名) 参考 map::operator[]
		_id_span_map[largespan->_pageid + i] = largespan;
	}

	//尾递归一次
	return _NewSpan(npage);
}

//获取对象到Span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_span_map.find(pageid);
	assert(it != _id_span_map.end());

	return it->second;
}

//释放空闲span到PageCache,合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (span->_npage >= NPAGES)
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		// 归还之前删除掉页到span的映射
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;
		return;
	}

	//得到前一页
	auto previt = _id_span_map.find(span->_pageid - 1);
	while (previt != _id_span_map.end())
	{
		//不是空闲直接退出
		Span* prevSpan = previt->second;
		if (prevSpan->_usecount != 0)
			break;

		//如果合并出的span超过NPAGES页的span，则不合并，否则没办法管理
		if ((prevSpan->_npage + span->_npage) >= NPAGES)
			break;

		//说明前一个span是空闲的
		_pagelist[prevSpan->_npage].Erease(prevSpan);
		prevSpan->_npage += span->_npage;
		delete span;
		span = prevSpan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//后一个连续的页
	auto nextit = _id_span_map.find(span->_pageid + span->_npage);
	while (nextit != _id_span_map.end())
	{
		//如果下一个页不是空闲直接退出
		Span* nextspan = nextit->second;
		if (nextspan->_usecount != 0)
			break;
		if ((nextspan->_npage + span->_npage) >= NPAGES)
			break;

		_pagelist[nextspan->_npage].Erease(nextspan);
		span->_npage += nextspan->_npage;
		delete nextspan;

		nextit = _id_span_map.find(span->_pageid + span->_npage);
	}
	for (size_t i = 0; i < span->_npage; ++i)
	{
		_id_span_map[span->_pageid + i] = span;
	}
	_pagelist[span->_npage].PushFront(span);
}