#ifndef _DELUGE_LIST_H_
#define _DELUGE_LIST_H_


struct list
{
	struct list *prev;
	struct list *next;
};

static inline void list_init(struct list *this)
{
	this->prev = this;
	this->next = this;
}

static inline int list_empty(const struct list *this)
{
	return (this->next == this);
}

static inline void list_push(struct list *this, struct list *elem)
{
	elem->next = this;
	elem->prev = this->prev;
	this->prev = elem;
	elem->prev->next = elem;
}

static inline struct list *list_pop(struct list *this)
{
	struct list *ret = this->prev;

	if (ret == this)
		return NULL;

	this->prev = ret->prev;
	this->prev->next = this;

	ret->next = ret;
	ret->prev = ret;

	return ret;
}

static inline void list_append(struct list *this, struct list *other)
{
	if (list_empty(other))
		return;

	this->prev->next = other->next;
	other->next->prev = this->prev;
	other->prev->next = this;
	this->prev = other->prev;

	list_init(other);
}

static inline void list_remove(struct list *elem)
{
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	list_init(elem);
}

#define list_item(_list, _type, _field)					\
	((_type *) (((void *) _list) - __builtin_offsetof(_type, _field)))


#endif
