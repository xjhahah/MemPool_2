#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//申请内存
	void* Allocate(size_t size);
	//释放内存
	void Deallocate(void* ptr, size_t size);
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
	// 链表中对象太多，开始回收。
	void ListTooLong(FreeList* freelist, size_t byte);
private:
	//自由链表
	FreeList _freelist[NLISTS];
};

//声明所有线程都能看到，但不是同一个
static _declspec (thread) ThreadCache* tls_threadcache = nullptr;