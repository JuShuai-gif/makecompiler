#ifndef PACK_H
#define PACK_H

#include"def.h"

typedef struct pack_info_s  pack_info_t;

struct pack_info_s
{
	const char*      name;
	long             size;
	long             offset;
	long             noffset;
	long             msize;
	pack_info_t* members;
	long             n_members;
};

long pack       (void*  p,  pack_info_t* infos, long n_infos,       uint8_t** pbuf, long* plen);
long unpack     (void** pp, pack_info_t* infos, long n_infos, const uint8_t*  buf,  long  len);
long unpack_free(void*  p,  pack_info_t* infos, long n_infos);

#define PACK_DEF_VAR(type, var)    type  var
#define PACK_DEF_VARS(type, vars)  long  n_##vars; type* vars

#define PACK_DEF_OBJ(type, obj)    type* obj
#define PACK_DEF_OBJS(type, objs)  long  n_##objs; type** objs

#define PACK_N_INFOS(type)         (sizeof(pack_info_##type) / sizeof(pack_info_##type[0]))

#define PACK_INFO_VAR(type, var)            {#var,  sizeof(((type*)0)->var),   offsetof(type, var), -1, -1, NULL, 0}
#define PACK_INFO_OBJ(type, obj, objtype)   {#obj,  sizeof(((type*)0)->obj),   offsetof(type, obj), -1, -1, pack_info_##objtype, PACK_N_INFOS(objtype)}

#define PACK_INFO_VARS(type, vars, vtype) \
	{"n_"#vars, sizeof(((type*)0)->n_##vars), offsetof(type, n_##vars), -1, -1, NULL, 0}, \
	{#vars,     sizeof(((type*)0)->vars),     offsetof(type, vars),     offsetof(type, n_##vars), sizeof(vtype), NULL, 0}

#define PACK_INFO_OBJS(type, objs, objtype) \
	{"n_"#objs, sizeof(((type*)0)->n_##objs), offsetof(type, n_##objs), -1, -1, NULL, 0}, \
	{#objs,     sizeof(((type*)0)->objs),     offsetof(type, objs),     offsetof(type, n_##objs), sizeof(objtype*), pack_info_##objtype, PACK_N_INFOS(objtype)}

#define PACK_TYPE(type) \
static pack_info_t pack_info_##type[] = {


#define PACK_END(type) \
}; \
static long type##_pack(type* p, uint8_t** pbuf, long* plen) \
{ \
	return pack(p, pack_info_##type, PACK_N_INFOS(type), pbuf, plen); \
} \
static long type##_unpack(type** pp, uint8_t* buf, long len) \
{ \
	return unpack((void**)pp, pack_info_##type, PACK_N_INFOS(type), buf, len); \
} \
static long type##_free(type* p) \
{ \
	return unpack_free(p, pack_info_##type, PACK_N_INFOS(type)); \
}

long pack_read(uint8_t** pbuf, const char* cpk);

#endif
