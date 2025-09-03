#include "../utils_list.h"

typedef struct
{
    list_t l;
    int d;
}d_t;

int main(){
    list_t h;
    list_init(&h);

    int i;
    for (i = 0; i < 10; i++)
    {
        d_t* d = malloc(sizeof(d_t));
        assert(d);

        d->d = i;
        list_add_front(&h,&d->l);
    }

    list_t* l;
    for (l = list_head(&h); l != list_sentinel(&h); l = list_next(l))
    {
        d_t* d = list_data(l,d_t,l);
        printf("%d\n",d->d);
    }
    
    while (!list_empty(&h)) {
		list_t* l = list_head(&h);

		d_t* d = list_data(l, d_t, l);

		list_del(&d->l);
		free(d);
		d = NULL;
	}

	return 0;



}











