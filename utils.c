#include <sys/queue.h>
#include "utils.h"
struct buffer{
	char *buf;
	int size;
	LIST_ENTRY(buffer) next;
};
static LIST_HEAD(buffer_chain, buffer) buffer_chain_head;
int buffer_total_size = 0;
static void clear_buffer_chain(){
	while(buffer_chain_head.lh_first != NULL){
		struct buffer *tmp = buffer_chain_head.lh_first;
		LIST_REMOVE(buffer_chain_head.lh_first, next);
		free(tmp->buf);
		free(tmp);
	}
	buffer_total_size = 0;
}
static char *buffer_chain_enlarge(int size){
	struct buffer *buf = malloc(sizeof(struct buffer));
	buf->buf = malloc(size);
	buf->size = size;
	LIST_INSERT_HEAD(&buffer_chain_head, buf, next);
	buffer_total_size += size;
	return buf->buf;
}
static void buffer_chain_store(char **str){
	int position = buffer_total_size;
	*str = malloc(buffer_total_size);
	struct buffer *bent;
	LIST_FOREACH(bent, &buffer_chain_head, next){
		position -= bent->size;
		memcpy(str+position, bent->buf, bent->size);
	}
}

void *zmalloc(size_t size){
	void *tmp = malloc(size);
	memset(tmp, 0, size);
	return tmp;
}
