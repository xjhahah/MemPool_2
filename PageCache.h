#pragma once
#include "Common.h"

//单例模式
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	//获取一个新的Span
	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);

	//获取对象到Span的映射
	Span* MapObjectToSpan(void* obj);

	//释放空闲span到PageCache,合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;

private:
	SpanList _pagelist[NPAGES];
	static PageCache _inst;

	std::mutex _mtx;
	//通过PageID和Span的映射关系得到地址
	std::unordered_map<PageID, Span*> _id_span_map;
};