#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//�����ڴ�
	void* Allocate(size_t size);
	//�ͷ��ڴ�
	void Deallocate(void* ptr, size_t size);
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
	// �����ж���̫�࣬��ʼ���ա�
	void ListTooLong(FreeList* freelist, size_t byte);
private:
	//��������
	FreeList _freelist[NLISTS];
};

//���������̶߳��ܿ�����������ͬһ��
static _declspec (thread) ThreadCache* tls_threadcache = nullptr;