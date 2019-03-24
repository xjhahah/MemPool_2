#pragma once
#include "Common.h"

//����ģʽ   ����ģʽ
// 1.central cache��������һ����ϣӳ���span��������������
// 2.ÿ��ӳ���С��empty span����һ�������У�nonempty span����һ��������
// 3.Ϊ�˱�֤ȫ��ֻ��Ψһ��central cache������౻��Ƴ��˵���ģʽ��

typedef size_t PageID;


class CentralCache
{
public:
	static CentralCache* GetIntance()
	{
		return &_inst;
	}
	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);
	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);
	// ��page cache��ȡһ��span
	Span* GetOneSpan(SpanList* list, size_t byte_size);
private:
	CentralCache() = default;  //ָ������Ĭ�Ϲ��캯��
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
private:
	//��������
	static CentralCache _inst;  //��̬����   ����ʵ��
	//���Ļ�����������
	SpanList _spanList[NLISTS];
};