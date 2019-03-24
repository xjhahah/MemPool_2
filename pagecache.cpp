#include "PageCache.h"

PageCache PageCache::_inst;

Span* PageCache::NewSpan(size_t npage)
{
	// ��������ֹ����߳�ͬʱ��PageCache������span
	// ��������Ǹ�ȫ�ּ��������ܵ����ĸ�ÿ��Ͱ����
	// �����ӦͰû��span,����Ҫ��ϵͳ�����
	// ���ܴ��ڶ���߳�ͬʱ��ϵͳ�����ڴ�Ŀ���
	std::unique_lock<std::mutex> lock(_mtx);

	if (npage >= NPAGES)
	{
		//��Ҫ��ϵͳ�����ڴ�
		void* ptr = SystemAlloc(npage);

		Span* span = new Span();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = npage;
		span->_objsize = npage << PAGE_SHIFT;

		_id_span_map[span->_pageid] = span;
		return span;
	}

	// ����64k-128ҳ���ڴ��ʱ��
	// ��Ҫ��span��objsize����Ϊspan�Ĵ�С
	// ��Ϊ�����ϵͳ�黹���spanʱ��Ҫ��С
	Span* span = _NewSpan(npage);
	span->_objsize = span->_npage << PAGE_SHIFT;

	return span;
}
Span* PageCache::_NewSpan(size_t npage)
{
	//std::unique_lock<std::mutex> lock(_mtx);  //��֧���Զ��������������


	//�����  _pagelist �� CentralCache���spanlist ���岻ͬ�������_paglist�������npageҳ��spanlist
	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}
	//���������spanlist����һ�����pagecache�и����Ҫ��span����ʣ�µ��ڴ�ҵ���Ӧ�� pagecache ��
	for (size_t i = npage + 1; i < NPAGES; ++i)
	{
		if (!_pagelist[i].Empty())
		{
			// �����и�
			Span* span = _pagelist[i].PopFront();
			Span* split = new Span();

			// ҳ��:��span�ĺ�������и�
			split->_pageid = span->_pageid + span->_npage - npage;
			// ҳ��
			split->_npage = npage;
			span->_npage = span->_npage - npage;

			// ���·ָ������ҳ��ӳ�䵽�µ�span��
			for (size_t i = 0; i < npage; i++)
			{
				_id_span_map[split->_pageid + i] = split;
			}

			_pagelist[span->_npage].PushFront(span);

			return split;
		}
	}

	//��Ҫ��ϵͳ�����ڴ�   VirtualAlloc   һ�㶼�Ǵ���64K
	//�ú����Ĺ������ڵ��ý��̵����ַ�ռ�,Ԥ�������ύһ����ҳ��������ڴ����Ļ�, ���ҷ�������δָ��MEM_RESET, ����ڴ��Զ�����ʼ��Ϊ0;
	void* ptr = SystemAlloc(128);

	Span* largespan = new Span();
	largespan->_pageid = (PageID)ptr >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;

	_pagelist[NPAGES - 1].PushFront(largespan);

	//ҳ�ŵ�span��ӳ��
	for (size_t i = 0; i < largespan->_npage; ++i)
	{
		//����� [] �������޸������ֵ(value�ı���) �ο� map::operator[]
		_id_span_map[largespan->_pageid + i] = largespan;
	}

	//β�ݹ�һ��
	return _NewSpan(npage);
}

//��ȡ����Span��ӳ��
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_span_map.find(pageid);
	assert(it != _id_span_map.end());

	return it->second;
}

//�ͷſ���span��PageCache,�ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (span->_npage >= NPAGES)
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		// �黹֮ǰɾ����ҳ��span��ӳ��
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;
		return;
	}

	//�õ�ǰһҳ
	auto previt = _id_span_map.find(span->_pageid - 1);
	while (previt != _id_span_map.end())
	{
		//���ǿ���ֱ���˳�
		Span* prevSpan = previt->second;
		if (prevSpan->_usecount != 0)
			break;

		//����ϲ�����span����NPAGESҳ��span���򲻺ϲ�������û�취����
		if ((prevSpan->_npage + span->_npage) >= NPAGES)
			break;

		//˵��ǰһ��span�ǿ��е�
		_pagelist[prevSpan->_npage].Erease(prevSpan);
		prevSpan->_npage += span->_npage;
		delete span;
		span = prevSpan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//��һ��������ҳ
	auto nextit = _id_span_map.find(span->_pageid + span->_npage);
	while (nextit != _id_span_map.end())
	{
		//�����һ��ҳ���ǿ���ֱ���˳�
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