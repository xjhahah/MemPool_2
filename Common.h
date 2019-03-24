#pragma once
#include <iostream>
#include <unordered_map>
#include <map>

#include <thread>
#include <mutex>
#include <assert.h>
#ifdef _WIN32
#include <Windows.h>
#endif //_WIN32

using std::cout;
using std::endl;

//�����������������
const size_t NLISTS = 240;
const size_t MAXSIZE = 64 * 1024;
const size_t PAGE_SHIFT = 12;
const size_t NPAGES = 129;

typedef size_t PageID;


static inline void* SystemAlloc(size_t npage)
{
#ifdef _WIN32
	// ��ϵͳ�����ڴ棬һ������128ҳ���ڴ棬�����Ļ������Ч�ʣ�һ�����빻����ҪƵ������
	void* ptr = VirtualAlloc(NULL, (npage) << PAGE_SHIFT, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
	return ptr;
#else 
#endif //_WIN32
}

static inline void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif // _WIN32
}
static inline void*& NEXT_OBJ(void* obj)
{
	return (*(void**)obj);
}

struct Span
{
	PageID _pageid = 0;  //ҳ��
	size_t _npage = 0;   //ҳ������

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _objlist = nullptr;  //��������
	size_t _objsize = 0;   //�����С
	size_t _usecount = 0;  //ʹ�ü���

};
//��ͷ˫��ѭ������
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	//ģʽ������
	Span* begin()
	{
		return _head->_next;
	}
	Span* end()
	{
		return _head;
	}

	void Insert(Span* cur, Span* newSpan)
	{
		assert(cur);

		Span* prev = cur->_prev;

		//prev newSpan cur
		newSpan->_next = cur;
		cur->_prev = newSpan;
		prev->_next = newSpan;
		newSpan->_prev = prev;
	}

	void Erease(Span* cur)
	{
		assert(cur != nullptr && cur != _head);

		Span* prev = cur->_prev;
		Span* next = cur->_next;

		
		prev->_next = next;
		next->_prev = prev;

		//delete cur;
	}
	void PushFront(Span* span)
	{
		Insert(begin(), span);
	}
	Span* PopFront()
	{
		Span* span = begin();
		Erease(span);
		return span;
	}
	void PushBack(Span* span)
	{
		Insert(end(), span);
	}

	void PopBack()
	{
		Span* tail = end();
		Erease(tail);
	}

	bool Empty()
	{
		return _head->_next == _head;
	}
public:
	std::mutex _mtx;
private:
	Span* _head = nullptr;
};

class FreeList
{
public:
	bool empty()
	{
		return _list == nullptr;
	}
	void PushRange(void*start, void* end, size_t num)
	{
		NEXT_OBJ(end) = _list;
		_list = start;
		_size += num;
	}

	//�����ڴ�
	void* Clear()
	{
		_size = 0;
		void* list = _list;
		_list = nullptr;
		return list;
	}
	//ͷɾ
	void* pop()
	{
		void* obj = _list;
		_list = NEXT_OBJ(obj);
		--_size;
		return obj;
	}
	//ͷ��
	void push(void* obj)
	{
		NEXT_OBJ(obj) = _list;
		_list = obj;
		++_size;
	}
	size_t Size()
	{
		return _size;
	}
	void SetMaxSize(size_t maxsize)
	{
		_maxsize = maxsize;
	}
	size_t MaxSize()
	{
		return _maxsize;
	}
private:
	void* _list = nullptr;
	size_t _size = 0;  //��¼�ж��ٸ�����
	size_t _maxsize = 1;  //һ������������������ԹҶ��ٸ�list
};

//�����ڴ����
class SizeClass
{
	// ������12%���ҵ�����Ƭ�˷�
	// [1,128] 8byte���� freelist[0,16)
	// [129,1024] 16byte���� freelist[16,72)
	// [1025,8*1024] 128byte���� freelist[72,128)
	// [8*1024+1,64*1024] 512byte���� freelist[128,240)
public:
	//�������  align:��׼  ����8�ֽڶ��룺��12�ֽ� (12+7)&~7
	static inline size_t _RoundUp(size_t bytes, size_t align)
	{
		return (((bytes)+align - 1)&~(align - 1));
	}
	//�����С���� ����ȡ������������Ķ������
	static inline size_t RoundUp(size_t bytes)
	{
		assert(bytes <= MAXSIZE);
		if (bytes <= 128) {
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024) {
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8192) {
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 65536) {
			return _RoundUp(bytes, 512);
		}
		return -1;
	}

	//ӳ����������λ��
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAXSIZE);

		//ÿ�������ж��ٸ���
		static int group_array[] = { 16,56,56,112 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index((bytes - 128), 4) + group_array[0];
		}
		else if (bytes <= 8192) {
			return _Index((bytes - 1024), 7) + group_array[0] + group_array[1];
		}
		else if (bytes <= 65536) {
			return _Index((bytes - 8192), 9) + group_array[0] + group_array[1] + group_array[2];
		}
		return -1;
	}

	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;
		int num = static_cast<int>(MAXSIZE / size);
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}
	//����һ����ϵͳ��ȡ���ٸ�ҳ
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);

		size_t npage = num * size;

		npage >>= 12;  //�������ҳ  >>=12 == /4096   һҳ = 4k
		if (npage == 0)
			npage = 1;
		return npage;
	}
};