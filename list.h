#define LIST_NAME(ty) List_##ty

#define LIST(ty)                                                               \
	typedef struct {                                                           \
		ty *data;                                                              \
		size_t size;                                                           \
		size_t capacity;                                                       \
	} LIST_NAME(ty)

#define CREATE_LIST(ty)                                                        \
	LIST_NAME(ty) create_list_##ty(size_t capacity) {                          \
		ty *data = malloc(sizeof(*data) * capacity);                           \
		LIST_NAME(ty) l;                                                       \
		l.data = data;                                                         \
		l.size = 0;                                                            \
		l.capacity = capacity;                                                 \
		return l;                                                              \
	}

#define APPEND_LIST(ty)                                                        \
	void append_list_##ty(LIST_NAME(ty) * l, ty t) {                           \
		if (l->size + 1 > l->capacity) {                                       \
			int new_capacity = 2 * l->capacity;                                \
			ty *data = malloc(sizeof(*data) * new_capacity);                   \
			memcpy(data, l->data, l->size);                                    \
			free(l->data);                                                     \
			l->data = data;                                                    \
			l->capacity = new_capacity;                                        \
		}                                                                      \
		l->data[l->size] = t;                                                  \
		l->size += 1;                                                          \
	}

#define FREE_LIST(ty)                                                          \
	void free_list_##ty(LIST_NAME(ty) * l) { free(l->data); }

#define FILTER_LIST(ty)                                                        \
	void filter_list_##ty(LIST_NAME(ty) * l, int (*p)(ty *)) {                 \
		size_t filter_count = 0;                                               \
		for (size_t i = 0; i < l->size; i++) {                                 \
			if (p(&l->data[i])) {                                              \
				ty tmp = l->data[i];                                           \
				l->data[i] = l->data[filter_count];                            \
				l->data[filter_count] = tmp;                                   \
				++filter_count;                                                \
			}                                                                  \
		}                                                                      \
		l->size = filter_count;                                                \
		l->capacity = filter_count;                                            \
	}
