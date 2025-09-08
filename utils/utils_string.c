#include "utils_string.h"

#define STRING_NUMBER_INC 4

// 字符串分配
string_t* string_alloc(){
    // 先分配一个 string_t 结构体
    string_t* s = malloc(sizeof(string_t));
    if (!s)
        return NULL;

    // 分配数据 多加一个1 是 \0
    s->data = malloc(STRING_NUMBER_INC + 1);
    if (!s->data)
    {
        free(s);
        return NULL;
    }

    // 容量
    s->capacity = STRING_NUMBER_INC;
    // 长度
    s->len = 0;
    // 结尾
    s->data[0] = '\0';
    return s;
}

// 字符串克隆
string_t* string_clone(string_t* s){
    if (!s)
        return NULL;
    
    string_t* s1 = malloc(sizeof(string_t));
    if(!s1)
        return NULL;
    
    if (s->capacity > 0)
        s1->capacity = s->capacity;
    else if(s->len > 0)
        s1->capacity = s->len;
    else
        s1->capacity = STRING_NUMBER_INC;
    
    s1->data = malloc(s1->capacity + 1);
    if (!s1->data)
    {
        free(s1);
        return NULL;
    }

    s1->len = s->len;
    if (s->len > 0)
        memcpy(s1->data,s->data,s->len);
    
    s1->data[s1->len] = '\0';
    return s1;
}

// 从char数组创建 string
string_t* string_cstr(const char* str){
    if (!str)
        return NULL;
    return string_cstr_len(str,strlen(str));
}

// 根据char数组和长度
string_t* string_cstr_len(const char* str,size_t len){
    if (!str)
        return NULL;
    
    string_t s;
    s.capacity = -1;
    s.len = len;
    s.data = (char*)str;

    return string_clone(&s);
}

// 字符串释放
void string_free(string_t* s){
    if (!s)
        return;
    
    if (s->capacity > 0)
        free(s->data);
    
    free(s);
    s = NULL;
}

// 打印字符串
void string_print_bin(string_t* s){
    if (!s)
        return;
    int i;
    for (i = 0; i < s->len; i++)
    {
        unsigned char c = s->data[i];

        if (i>0 && i % 10 == 0)
            printf("\n");
        printf("%#02x ",c);
    }
    printf("\n");
}

// 字符串比较
int string_cmp(const string_t* s0,const string_t* s1){
    if (s0->len < s1->len)
        return -1;
    if (s0->len > s1->len)
        return 1;
    return strncmp(s0->data,s1->data,s0->len);
}

// 字符串和char数组比较
int	string_cmp_cstr(const string_t* s0, const char* str)
{
	return string_cmp_cstr_len(s0, str, strlen(str));
}

// string与原始char数组进行对比
int	string_cmp_cstr_len(const string_t* s0, const char* str, size_t len)
{
	string_t s1;
	s1.capacity	= -1;
	s1.len		= len;
	s1.data		= (char*)str;

	return string_cmp(s0, &s1);
}

// 字符串复制
int string_copy(string_t* s0,const string_t* s1){
    if (!s0 || !s1 || !s0->data || !s1->data)
        return -EINVAL;
    
    assert(s0->capacity > 0);

    if (s1->len > s0->capacity)
    {
        char* p = realloc(s0->data,s1->len + 1);
        if (!p)
            return -ENOMEM;
        s0->data = p;
        s0->capacity = s1->len;        
    }

    memcpy(s0->data,s1->data,s1->len);
    s0->data[s1->len] = '\0';
    s0->len = s1->len;
    return 0;
}

// 字符串拼接
int string_cat(string_t* s0,const string_t* s1){
    if (!s0 || !s1 || !s0->data || !s1->data)
        return -EINVAL;
    
    assert(s0->capacity > 0);
    
    // 判断容量是否够用
    if (s0->len + s1->len > s0->capacity)
    {
        char* p = realloc(s0->data,s0->len + s1->len + STRING_NUMBER_INC + 1);
        if (!p)
            return -ENOMEM;
        s0->data = p;
        s0->capacity = s0->len + s1->len + STRING_NUMBER_INC;
    }

    memcpy(s0->data + s0->len,s1->data,s1->len);
    s0->data[s0->len + s1->len] = '\0';
    s0->len += s1->len;
    return 0;
}

// 将string与char数组拼接
int string_cat_cstr(string_t* s0,const char* str){
    if (!s0 || !s0->data || !str)
    {
        return -EINVAL;
    }
    return string_cat_cstr_len(s0,str,strlen(str));
}

// 将string与char数组原始数据进行拼接
int string_cat_cstr_len(string_t* s0,const char* str,size_t len){
    if (!s0 || !s0->data || !str)
        return -EINVAL;
    
    string_t s1;
    s1.capacity = -1;
    s1.len = len;
    s1.data = (char*)str;

    return string_cat(s0,&s1);    
}   

// 字符串填充0
int string_fill_zero(string_t* s0,size_t len){
    if (!s0 || !s0->data)
        return -EINVAL;
    
    assert(s0->capacity > 0);

    if (s0->len + len > s0->capacity)
    {
        char* p = realloc(s0->data,s0->len + len + STRING_NUMBER_INC + 1);

        if (!p)
            return -ENOMEM;
        s0->data = p;
        s0->capacity = s0->len + len + STRING_NUMBER_INC;
    }

    memset(s0->data + s0->len,0,len);

    s0->data[s0->len + len]='\0';
    s0->len += len;
    return 0;
}

// kmp 比对
static int* _prefix_kmp(const uint8_t* P,int m){
	int* prefix = malloc(sizeof(int) * (m + 1));
	if (!prefix)
		return NULL;

	prefix[0] = -1;

	int k = -1;
	int q;

	for (q = 1; q < m; q++) {

		while (k > -1 && P[k + 1] != P[q])
			k = prefix[k];

		if (P[k + 1] == P[q])
			k++;

		prefix[q] = k;
	}
	return prefix;
}

// kmp匹配
static int _match_kmp(const uint8_t* T,int Tlen,const uint8_t* P,int Plen,vector_t* offsets){
if (Tlen <= 0 || Plen <= 0)
		return -EINVAL;

	int n = Tlen;
	int m = Plen;

	int* prefix = _prefix_kmp(P, m);
	if (!prefix)
		return -1;

	int q = -1;
	int i;

	for (i = 0; i < n; i++) {

		while (q > -1 && P[q + 1] != T[i])
			q = prefix[q];

		if (P[q + 1] == T[i])
			q++;

		if (q == m - 1) {
			logd("KMP find P: %s in T: %s, offset: %d\n", P, T, i - m + 1);

			int ret = vector_add(offsets, (void*)(intptr_t)(i - m + 1));
			if (ret < 0)
				return ret;

			q = prefix[q];
		}
	}

	free(prefix);
	return 0;
}

// 字符串 kmp 匹配
int string_match_kmp(const string_t* T,const string_t* P,vector_t* offsets){
    if (!T || !P || !offsets)
    {
        return -EINVAL;
    }

    return _match_kmp(T->data,T->len,P->data,P->len,offsets);
}


int string_match_kmp_cstr(const uint8_t* T, const uint8_t* P, vector_t* offsets)
{
	if (!T || !P || !offsets)
		return -EINVAL;

	return _match_kmp(T, strlen(T), P, strlen(P), offsets);
}


int string_match_kmp_cstr_len(const string_t* T, const uint8_t* P, size_t Plen, vector_t* offsets)
{
	if (!T || !P || 0 == Plen || !offsets)
		return -EINVAL;

	return _match_kmp(T->data, T->len, P, Plen, offsets);
}


int string_get_offset(string_t* str, const char* data, size_t len)
{
	int ret;

	if (0 == str->len) {
		if (string_cat_cstr_len(str, data, len) < 0)
			return -1;
		return 0;
	}

	vector_t* vec = vector_alloc();
	if (!vec)
		return -ENOMEM;

	ret = string_match_kmp_cstr_len(str, data, len, vec);
	if (ret < 0) {

	} else if (0 == vec->size) {
		ret = str->len;

		if (string_cat_cstr_len(str, data, len) < 0)
			ret = -1;
	} else
		ret = (intptr_t)(vec->data[0]);

	vector_free(vec);
	return ret;
}











