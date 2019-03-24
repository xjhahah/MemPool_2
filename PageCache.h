#pragma once
#include "Common.h"

//����ģʽ
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	//��ȡһ���µ�Span
	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);

	//��ȡ����Span��ӳ��
	Span* MapObjectToSpan(void* obj);

	//�ͷſ���span��PageCache,�ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;

private:
	SpanList _pagelist[NPAGES];
	static PageCache _inst;

	std::mutex _mtx;
	//ͨ��PageID��Span��ӳ���ϵ�õ���ַ
	std::unordered_map<PageID, Span*> _id_span_map;
};