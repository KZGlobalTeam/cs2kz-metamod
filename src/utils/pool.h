#pragma once
#include "arena.h"

template<class T>
struct Pool
{
	union Node
	{
		T value;
		Node *next;
	};

	Arena *arena;
	Node *freelist;

	T *Alloc();
	void Free(const T *value);
};

template<class T>
T *Pool<T>::Alloc()
{
	if (this->freelist)
	{
		Node *node = this->freelist;
		this->freelist = this->freelist->next;
		return (T *)node;
	}
	else
	{
		Node *node = (Node *)arena->Alloc(sizeof(Node), alignof(Node));
		return (T *)node;
	}
}

template<class T>
void Pool<T>::Free(const T *value)
{
	Node *node = (Node *)value;
	node->next = this->freelist;
	this->freelist = node;
}
