#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_inst;

// ��page cache��ȡһ��span
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{

	//ģ�µ�����ʵ��
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			return span;
		}
		span = span->_next;
	}

	//��PageCache �����һ���µĺ��ʴ�СС��span
	//����� npage ҳ�� span
	size_t npage = SizeClass::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	//��span���ڴ��и��һ����bytes��С�Ķ��������
	//ͨ��ҳ���ҵ���ַ
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);
	char* end = start + (newspan->_npage << PAGE_SHIFT);
	char* cur = start;

	char* next = cur + bytes;
	//��һ����ڴ��и��С���ڴ�
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

	//��newSpan���뵽spanlist
	spanlist->PushFront(newspan);

	return newspan;
}

// �����Ļ����ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t byte_size)
{
	//start = malloc(byte_size*n);  //��׮  ������Ԫ����
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

	//�Ե�ǰͰ���м���
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

	//�ú���� ����С��
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	span->_objlist = cur;
	span->_usecount += fetchnum;

	//��һ��spanΪ�գ��Ƶ�β��
	if (span->_objlist == nullptr)
	{
		spanlist->Erease(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

//��list����Span
void CentralCache::ReleaseListToSpans(void*start, size_t byte)
{
	size_t index = SizeClass::Index(byte);
	SpanList* spanlist = &_spanList[index];

	//RAII˼��
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//���ͷŶ���ص��յ�span���ѿյ�span�Ƶ�ͷ�ϣ�
		if (span->_objlist == nullptr)
		{
			spanlist->Erease(span);
			spanlist->PushFront(span);
		}

		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		// usecount == 0��ʾspan�г�ȥ�Ķ��󶼻�������
		// �ͷ�span�ص�pagecache���кϲ�
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