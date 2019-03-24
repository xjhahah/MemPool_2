#pragma once
#include "Common.h"

//单例模式   懒汉模式
// 1.central cache本质是由一个哈希映射的span对象自由链表构成
// 2.每个映射大小的empty span挂在一个链表中，nonempty span挂在一个链表中
// 3.为了保证全局只有唯一的central cache，这个类被设计成了单例模式。

typedef size_t PageID;


class CentralCache
{
public:
	static CentralCache* GetIntance()
	{
		return &_inst;
	}
	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);
	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);
	// 从page cache获取一个span
	Span* GetOneSpan(SpanList* list, size_t byte_size);
private:
	CentralCache() = default;  //指定生成默认构造函数
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
private:
	//单例对象
	static CentralCache _inst;  //静态变量   类外实现
	//中心缓存自由链表
	SpanList _spanList[NLISTS];
};