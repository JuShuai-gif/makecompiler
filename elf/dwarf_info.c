#include"dwarf.h"
#include"leb128.h"
#include"native.h"
#include"ghr_elf.h"

dwarf_info_entry_t* dwarf_info_entry_alloc()
{
	dwarf_info_entry_t* ie = calloc(1, sizeof(dwarf_info_entry_t));
	if (!ie)
		return NULL;

	ie->attributes = vector_alloc();
	if (!ie->attributes) {
		free(ie);
		return NULL;
	}

	return ie;
}

void dwarf_info_attr_free(dwarf_info_attr_t* attr)
{
	if (attr) {
		if (attr->data)
			string_free(attr->data);

		free(attr);
	}
}

void dwarf_info_entry_free(dwarf_info_entry_t* ie)
{
	if (ie) {
		if (ie->attributes) {
			vector_clear(ie->attributes, (void (*)(void*)) dwarf_info_attr_free);
			vector_free(ie->attributes);
		}

		free(ie);
	}
}

static int _abbrev_find_code(const void* v0, const void* v1)
{
	const dwarf_uword_t               code = (uintptr_t)v0;

	const dwarf_abbrev_declaration_t* d    = v1;

	return code != d->code; 
}

int dwarf_info_decode(vector_t* infos, vector_t* abbrevs, string_t* debug_str, const char*   debug_info, size_t debug_info_size, dwarf_info_header_t* header)
{
	if (!infos || !abbrevs || !debug_str || !debug_info || 0 == debug_info_size || !header) {
		loge("debug_info_size: %ld\n", debug_info_size);
		return -EINVAL;
	}

	if (debug_info_size < sizeof(dwarf_uword_t)) {
		loge("\n");
		return -EINVAL;
	}

	header->length = *(dwarf_uword_t*)debug_info;
	size_t i  = sizeof(dwarf_uword_t);

	loge("header->length: %d\n", header->length);

	if (i + header->length < debug_info_size) {
		loge("\n");
		return -EINVAL;
	}

	assert(i +   sizeof(dwarf_uhalf_t) < debug_info_size);
	header->version = *(dwarf_uhalf_t*)( debug_info + i);
	i +=         sizeof(dwarf_uhalf_t);

	assert(i +  sizeof(dwarf_uword_t) < debug_info_size);
	header->offset = *(dwarf_uword_t*)( debug_info + i);
	i +=        sizeof(dwarf_uword_t);

	assert(i +        sizeof(dwarf_ubyte_t) <= debug_info_size);
	header->address_size = *(dwarf_ubyte_t*)(  debug_info + i);
	i +=              sizeof(dwarf_ubyte_t);

	dwarf_abbrev_declaration_t* d    = NULL;
	dwarf_abbrev_attribute_t*   attr = NULL;
	dwarf_info_entry_t*         ie   = NULL;
	dwarf_info_attr_t*          iattr = NULL;

	while (i < debug_info_size) {

		ie = dwarf_info_entry_alloc();
		if (!ie)
			return -ENOMEM;
		ie->cu_byte_offset = i;

		int ret = vector_add(infos, ie);
		if (ret < 0) {
			dwarf_info_entry_free(ie);
			return ret;
		}

		size_t len;

		len = 0;
		ie->code = leb128_decode_uint32(debug_info + i, &len);
		assert(len > 0);
		i += len;
		assert(i <= debug_info_size);

		d = vector_find_cmp(abbrevs, (void*)(uintptr_t)ie->code, _abbrev_find_code);
		if (!d)
			return -1;

		int k;
		for (k = 0; k < d->attributes->size; k++) {
			attr      = d->attributes->data[k];

			iattr = calloc(1, sizeof(dwarf_info_attr_t));
			if (!iattr)
				return -ENOMEM;

			ret = vector_add(ie->attributes, iattr);
			if (ret < 0) {
				dwarf_info_attr_free(iattr);
				return ret;
			}

			iattr->name = attr->name;
			iattr->form = attr->form;

			int j;

			switch (iattr->form) {

				case DW_FORM_addr:
					assert(i + sizeof(uintptr_t) <= debug_info_size);

					iattr->address = *(uintptr_t*)(debug_info + i);
					i += sizeof(uintptr_t);
					break;

				case DW_FORM_data1:
					assert(i + sizeof(uint8_t) <= debug_info_size);

					iattr->const1 = *(uint8_t*)(debug_info + i);
					i += sizeof(uint8_t);
					break;
				case DW_FORM_data2:
					assert(i + sizeof(uint16_t) <= debug_info_size);

					iattr->const2 = *(uint16_t*)(debug_info + i);
					i += sizeof(uint16_t);
					break;
				case DW_FORM_data4:
					assert(i + sizeof(uint32_t) <= debug_info_size);

					iattr->const4 = *(uint32_t*)(debug_info + i);
					i += sizeof(uint32_t);
					break;
				case DW_FORM_data8:
					assert(i + sizeof(uint64_t) <= debug_info_size);

					iattr->const8 = *(uint64_t*)(debug_info + i);
					i += sizeof(uint64_t);
					break;

				case DW_FORM_sdata:
					len = 0;
					iattr->sdata = leb128_decode_int32(debug_info + i, &len);
					assert(len > 0);

					i += len;
					assert(i <= debug_info_size);
					break;
				case DW_FORM_udata:
					len = 0;
					iattr->udata = leb128_decode_uint32(debug_info + i, &len);
					assert(len > 0);

					i += len;
					assert(i <= debug_info_size);
					break;

				case DW_FORM_flag:
					assert(i < debug_info_size);

					iattr->flag  = debug_info[i++];
					break;
				case DW_FORM_flag_present:
					iattr->flag  = 1;
					break;

				case DW_FORM_ref1:
					assert(i + sizeof(uint8_t) <= debug_info_size);

					iattr->ref8 = *(uint8_t*)(debug_info + i);
					i += sizeof(uint8_t);
					break;
				case DW_FORM_ref2:
					assert(i + sizeof(uint16_t) <= debug_info_size);

					iattr->ref8 = *(uint16_t*)(debug_info + i);
					i += sizeof(uint16_t);
					break;
				case DW_FORM_ref4:
					assert(i + sizeof(uint32_t) <= debug_info_size);

					iattr->ref8 = *(uint32_t*)(debug_info + i);
					i += sizeof(uint32_t);
					break;
				case DW_FORM_ref8:
					assert(i + sizeof(uint64_t) <= debug_info_size);

					iattr->ref8 = *(uint64_t*)(debug_info + i);
					i += sizeof(uint64_t);
					break;
				case DW_FORM_ref_udata:
					len = 0;
					iattr->ref8 = leb128_decode_uint32(debug_info + i, &len);
					assert(len > 0);

					i += len;
					assert(i <= debug_info_size);
					break;

				case DW_FORM_string:
					j = i;
					while (j < debug_info_size && debug_info[j])
						j++;
					assert(j < debug_info_size);

					iattr->data = string_cstr_len(debug_info + i, j - i);
					if (!iattr->data)
						return -ENOMEM;

					i = j + 1;
					break;
				case DW_FORM_strp:
					assert(i + sizeof(dwarf_uword_t) <= debug_info_size);

					iattr->str_offset = *(dwarf_uword_t*)(debug_info + i);
					i += sizeof(dwarf_uword_t);

					j = iattr->str_offset;
					while (j < debug_str->len && debug_str->data[j])
						j++;
					assert(j <= debug_str->len);
					assert('\0' == debug_str->data[j]);

					iattr->data = string_cstr_len(debug_str->data + iattr->str_offset, j - iattr->str_offset);
					if (!iattr->data)
						return -ENOMEM;
					break;

				case DW_FORM_sec_offset:
					assert(i + sizeof(dwarf_uword_t) <= debug_info_size);

					iattr->lineptr = *(dwarf_uword_t*)(debug_info + i);
					i += sizeof(dwarf_uword_t);

					break;

				case DW_FORM_exprloc:
					len = 0;
					iattr->exprloc = leb128_decode_uint32(debug_info + i, &len);
					assert(len > 0);

					i += len;
					assert(i + iattr->exprloc <= debug_info_size);

					iattr->data = string_cstr_len(debug_info + i, iattr->exprloc);
					if (!iattr->data)
						return -ENOMEM;

					i += iattr->exprloc;
					break;
				case 0:
					assert(0 == iattr->name);
					break;
				default:
					loge("iattr->form: %u\n", iattr->form);
					loge("iattr->form: %s\n", dwarf_find_form(iattr->form));
					return -1;
					break;
			};
		}
	}

	return 0;
}

static int _add_rela_common(dwarf_t* debug, const char* sym, uint64_t offset, int type, int addend)
{
	rela_t* rela = calloc(1, sizeof(rela_t));
	if (!rela)
		return -ENOMEM;

	rela->name = string_cstr(sym);
	if (!rela->name) {
		free(rela);
		return -ENOMEM;
	}

	if (vector_add(debug->info_relas, rela) < 0) {
		string_free(rela->name);
		free(rela);
		return -ENOMEM;
	}

	rela->text_offset = offset;
	rela->addend      = addend;
	rela->type        = type;

	if (!strcmp(debug->arch, "arm64") || !strcmp(debug->arch, "naja")) {

		switch (type) {

			case R_X86_64_32:
				rela->type = R_AARCH64_ABS32;
				break;

			case R_X86_64_64:
				rela->type = R_AARCH64_ABS64;
				break;

			default:
				loge("\n");
				return -EINVAL;
				break;
		};

	} else if (!strcmp(debug->arch, "arm32")) {

		switch (type) {

			case R_X86_64_32:
			case R_X86_64_64:
				rela->type = R_ARM_ABS32;
				break;

			default:
				loge("\n");
				return -EINVAL;
				break;
		};
	}

	return 0;
}

static int _add_rela_subprogram(dwarf_t* debug, dwarf_info_entry_t* ie, uint64_t offset, int type)
{
	dwarf_info_attr_t* iattr = NULL;

	int k;
	for (k = 0; k < ie->attributes->size; k++) {
		iattr     = ie->attributes->data[k];

		if (DW_AT_name == iattr->name)
			break;
	}

	assert(k < ie->attributes->size);
	assert(iattr->data);

	return _add_rela_common(debug, iattr->data->data, offset, type, 0);
}

int dwarf_info_encode(dwarf_t* debug, dwarf_info_header_t* header)
{
	if (!debug || !header) {
		loge("\n");
		return -EINVAL;
	}

	vector_t* infos      = debug->infos;
	vector_t* abbrevs    = debug->abbrevs;
	string_t* debug_str  = debug->str;
	string_t* debug_info = debug->debug_info;

	if (!infos || !abbrevs || !debug_str || !debug_info || !header) {
		loge("\n");
		return -EINVAL;
	}

	vector_t* refs = vector_alloc();
	if (!refs)
		return -ENOMEM;

#define DWARF_INFO_FILL(str, buf, len) \
	do { \
		int ret = string_cat_cstr_len(str, (char*)buf, len); \
		if (ret < 0) { \
			loge("\n"); \
			return ret; \
		} \
	} while (0)

#define DWARF_INFO_ADD_REF(refs, iattr_, ie_, ref_entry_, offset_, size_) \
	do { \
		dwarf_attr_ref_t* iref = malloc(sizeof(dwarf_attr_ref_t)); \
		if (!iref) { \
			loge("\n"); \
			return -ENOMEM; \
		} \
		\
		int ret = vector_add(refs, iref); \
		if (ret < 0) { \
			loge("\n"); \
			return ret; \
		} \
		\
		iref->iattr     = iattr_; \
		iref->ie        = ie_; \
		iref->ref_entry = ref_entry_; \
		iref->offset    = offset_; \
		iref->size      = size_; \
	} while (0)

	size_t origin_len = debug_info->len;

	DWARF_INFO_FILL(debug_info, &header->length,       sizeof(header->length));
	DWARF_INFO_FILL(debug_info, &header->version,      sizeof(header->version));

	int ret = _add_rela_common(debug, ".debug_abbrev", debug_info->len, R_X86_64_32, 0);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	DWARF_INFO_FILL(debug_info, &header->offset,       sizeof(header->offset));
	DWARF_INFO_FILL(debug_info, &header->address_size, sizeof(header->address_size));

	dwarf_abbrev_declaration_t* d     = NULL;
	dwarf_abbrev_attribute_t*   attr  = NULL;
	dwarf_info_entry_t*         ie    = NULL;
	dwarf_info_attr_t*          iattr = NULL;
	dwarf_attr_ref_t*           iref  = NULL;

	int i;
	for (i = 0; i < infos->size; i++) {
		ie =        infos->data[i];

		ie->cu_byte_offset = debug_info->len;

		logd("i: %d, ie->code: %u, len: %lu\n", i, ie->code, debug_info->len);

		uint8_t buf[64];
		size_t  len;

		len = leb128_encode_uint32(ie->code, buf, sizeof(buf));
		assert(len > 0);
		DWARF_INFO_FILL(debug_info, buf, len);

		d = vector_find_cmp(abbrevs, (void*)(uintptr_t)ie->code, _abbrev_find_code);
		if (!d) {
			loge("ie->code: %u\n", ie->code);
			return -1;
		}

		if (d->attributes->size != ie->attributes->size) {
			loge("\n");
			return -1;
		}

		int k;
		for (k = 0; k < ie->attributes->size; k++) {
			iattr     = ie->attributes->data[k];

			int ret;
			int j;

			switch (iattr->form) {

				case DW_FORM_addr:

					if (DW_AT_low_pc == iattr->name) {

						if (DW_TAG_compile_unit == d->tag)

							ret = _add_rela_common(debug, ".text", debug_info->len, R_X86_64_64, 0);

						else if (DW_TAG_subprogram == d->tag)

							ret = _add_rela_subprogram(debug, ie, debug_info->len, R_X86_64_64);
						else
							ret = 0;

						if (ret < 0)
							return ret;
					}

					DWARF_INFO_FILL(debug_info, &iattr->address, sizeof(uintptr_t));
					break;

				case DW_FORM_data1:
					DWARF_INFO_FILL(debug_info, &iattr->const1, sizeof(uint8_t));
					break;
				case DW_FORM_data2:
					DWARF_INFO_FILL(debug_info, &iattr->const2, sizeof(uint16_t));
					break;
				case DW_FORM_data4:
					DWARF_INFO_FILL(debug_info, &iattr->const4, sizeof(uint32_t));
					break;
				case DW_FORM_data8:
					DWARF_INFO_FILL(debug_info, &iattr->const8, sizeof(uint64_t));
					break;

				case DW_FORM_sdata:
					len = leb128_encode_int32(iattr->sdata, buf, sizeof(buf));
					assert(len > 0);
					DWARF_INFO_FILL(debug_info, buf, len);
					break;
				case DW_FORM_udata:
					len = leb128_encode_uint32(iattr->udata, buf, sizeof(buf));
					assert(len > 0);
					DWARF_INFO_FILL(debug_info, buf, len);
					break;

				case DW_FORM_flag:
					DWARF_INFO_FILL(debug_info, &iattr->flag, 1);
					break;
				case DW_FORM_flag_present:
					break;

				case DW_FORM_ref1:
					DWARF_INFO_ADD_REF(refs, iattr, ie, iattr->ref_entry, debug_info->len, sizeof(uint8_t));
					DWARF_INFO_FILL(debug_info, &iattr->ref8, sizeof(uint8_t));
					break;
				case DW_FORM_ref2:
					DWARF_INFO_ADD_REF(refs, iattr, ie, iattr->ref_entry, debug_info->len, sizeof(uint16_t));
					DWARF_INFO_FILL(debug_info, &iattr->ref8, sizeof(uint16_t));
					break;
				case DW_FORM_ref4:
					DWARF_INFO_ADD_REF(refs, iattr, ie, iattr->ref_entry, debug_info->len, sizeof(uint32_t));
					DWARF_INFO_FILL(debug_info, &iattr->ref8, sizeof(uint32_t));
					break;
				case DW_FORM_ref8:
					DWARF_INFO_ADD_REF(refs, iattr, ie, iattr->ref_entry, debug_info->len, sizeof(uint64_t));
					DWARF_INFO_FILL(debug_info, &iattr->ref8, sizeof(uint64_t));
					break;
				case DW_FORM_ref_udata:
					DWARF_INFO_ADD_REF(refs, iattr, ie, iattr->ref_entry, debug_info->len, sizeof(uint64_t));
					DWARF_INFO_FILL(debug_info, &iattr->ref8, sizeof(uint64_t));
//					len = leb128_encode_uint32(iattr->ref8, buf, sizeof(buf));
//					assert(len > 0);
//					DWARF_INFO_FILL(debug_info, buf, len);
					break;

				case DW_FORM_string:
					DWARF_INFO_FILL(debug_info, iattr->data->data, iattr->data->len + 1);
					break;
				case DW_FORM_strp:

					ret = string_get_offset(debug_str, iattr->data->data, iattr->data->len + 1);
					if (ret < 0) {
						loge("\n");
						return ret;
					}

					iattr->str_offset = ret;

					ret = _add_rela_common(debug, ".debug_str", debug_info->len, R_X86_64_32, iattr->str_offset);
					if (ret < 0) {
						loge("\n");
						return ret;
					}

					DWARF_INFO_FILL(debug_info, &iattr->str_offset, sizeof(dwarf_uword_t));
					break;

				case DW_FORM_sec_offset:

					if (DW_TAG_compile_unit == d->tag && DW_AT_stmt_list == iattr->name) {

						ret = _add_rela_common(debug, ".debug_line", debug_info->len, R_X86_64_32, 0);
						if (ret < 0)
							return ret;
					}

					DWARF_INFO_FILL(debug_info, &iattr->lineptr, sizeof(dwarf_uword_t));
					break;

				case DW_FORM_exprloc:
					len = leb128_encode_uint32(iattr->exprloc, buf, sizeof(buf));
					assert(len > 0);
					DWARF_INFO_FILL(debug_info, buf, len);

					DWARF_INFO_FILL(debug_info, iattr->data->data, iattr->data->len);
					break;
				case 0:
					assert(0 == iattr->name);
					break;
				default:
					loge("iattr->form: %u\n", iattr->form);
					loge("iattr->form: %s\n", dwarf_find_form(iattr->form));
					return -1;
					break;
			};
		}
	}

	for (i = 0; i < refs->size; i++) {
		iref      = refs->data[i];

		if (DW_AT_sibling == iref->iattr->name)
			continue;

		if (!iref->ref_entry) {
			loge("ie: %p, ie->code: %u, iattr->name: %s\n",
					iref->ie, iref->ie->code, dwarf_find_attribute(iref->iattr->name));
			return -1;
		}

		memcpy(debug_info->data + iref->offset, &iref->ref_entry->cu_byte_offset, iref->size);
	}

	vector_free(refs);
	refs = NULL;

	logd("origin_len: %lu, debug_info->len: %lu\n", origin_len, debug_info->len);

	*(dwarf_uword_t*)(debug_info->data + origin_len) = debug_info->len - origin_len - sizeof(dwarf_uword_t);
	return 0;
}

int dwarf_info_fill_attr(dwarf_info_attr_t* iattr, uint8_t* data, size_t len)
{
	if (!iattr || !data || 0 == len) {
		loge("\n");
		return -EINVAL;
	}

	switch (iattr->form) {

		case DW_FORM_addr:
			assert(len >= sizeof(uintptr_t));

			iattr->address = *(uintptr_t*)data;
			break;

		case DW_FORM_data1:
			assert(len >= sizeof(uint8_t));

			iattr->const1 = *(uint8_t*)data;
			break;
		case DW_FORM_data2:
			assert(len >= sizeof(uint16_t));

			iattr->const2 = *(uint16_t*)data;
			break;
		case DW_FORM_data4:
			assert(len >= sizeof(uint32_t));

			iattr->const4 = *(uint32_t*)data;
			break;
		case DW_FORM_data8:
			assert(len >= sizeof(uint64_t));

			iattr->const8 = *(uint64_t*)data;
			break;

		case DW_FORM_sdata:
			assert(len >= sizeof(dwarf_sword_t));
			iattr->sdata = *(dwarf_sword_t*)data;
			break;
		case DW_FORM_udata:
			assert(len >= sizeof(dwarf_uword_t));
			iattr->udata = *(dwarf_uword_t*)data;
			break;

		case DW_FORM_flag:
			assert(len >= 1);
			iattr->flag  = data[0];
			break;
		case DW_FORM_flag_present:
			iattr->flag  = 1;
			break;

		case DW_FORM_ref1:
			assert(len >= sizeof(uint8_t));

			iattr->ref8 = *(uint8_t*)data;
			break;
		case DW_FORM_ref2:
			assert(len >= sizeof(uint16_t));

			iattr->ref8 = *(uint16_t*)data;
			break;
		case DW_FORM_ref4:
			assert(len >= sizeof(uint32_t));

			iattr->ref8 = *(uint32_t*)data;
			break;
		case DW_FORM_ref8:
			assert(len >= sizeof(uint64_t));

			iattr->ref8 = *(uint64_t*)data;
			break;
		case DW_FORM_ref_udata:
			iattr->ref8 = *(dwarf_uword_t*)data;
			break;

		case DW_FORM_string:

			iattr->data = string_cstr_len(data, len);
			if (!iattr->data)
				return -ENOMEM;
			break;
		case DW_FORM_strp:
//			assert(len >= sizeof(dwarf_uword_t));

			iattr->data = string_cstr_len(data, len);
			if (!iattr->data)
				return -ENOMEM;

			iattr->str_offset = 0;
			break;

		case DW_FORM_sec_offset:
			assert(len >= sizeof(dwarf_uword_t));

			iattr->lineptr = *(dwarf_uword_t*)data;
			break;

		case DW_FORM_exprloc:
			assert(len >= sizeof(dwarf_uword_t));
			iattr->exprloc = *(dwarf_uword_t*)data;

			assert(len >= sizeof(dwarf_uword_t) + iattr->exprloc);

			iattr->data = string_cstr_len(data + sizeof(dwarf_uword_t), iattr->exprloc);
			if (!iattr->data)
				return -ENOMEM;
			break;
		case 0:
			assert(0 == iattr->name);
			break;
		default:
			loge("iattr->form: %u\n", iattr->form);
			loge("iattr->form: %s\n", dwarf_find_form(iattr->form));
			return -1;
			break;
	};

	return 0;
}

void dwarf_info_print(vector_t* infos)
{
	if (!infos)
		return;

	dwarf_info_entry_t* ie;
	dwarf_info_attr_t*  iattr;

	int i;
	for (i = 0; i < infos->size; i++) {
		ie =        infos->data[i];

		printf("code: %u, ie->attributes->size: %d\n", ie->code, ie->attributes->size);

		int j;
		for (j = 0; j < ie->attributes->size; j++) {
			iattr     = ie->attributes->data[j];

			if (0 == iattr->name || 0 == iattr->form) {

				printf("%u, %u, ", iattr->name, iattr->form);
			} else {
				int len = printf("%s, ",dwarf_find_attribute(iattr->name));
				len = 30 - len;

				char fmt[16];
				sprintf(fmt, "%%%dc", len);
				printf(fmt, ' ');

				printf("%s, ", dwarf_find_form(iattr->form));
			}

			switch (iattr->form) {

				case DW_FORM_addr:
					printf("%#lx\n", iattr->address);
					break;

				case DW_FORM_data1:
					printf("%u\n", iattr->const1);
					break;
				case DW_FORM_data2:
					printf("%u\n", iattr->const2);
					break;
				case DW_FORM_data4:
					printf("%u\n", iattr->const4);
					break;
				case DW_FORM_data8:
					printf("%lu\n", iattr->const8);
					break;

				case DW_FORM_sdata:
					printf("%d\n", iattr->sdata);
					break;
				case DW_FORM_udata:
					printf("%u\n", iattr->udata);
					break;

				case DW_FORM_flag:
				case DW_FORM_flag_present:
					printf("%u\n", iattr->flag);
					break;

				case DW_FORM_ref1:
				case DW_FORM_ref2:
				case DW_FORM_ref4:
				case DW_FORM_ref8:
				case DW_FORM_ref_udata:
					printf("%lu\n", iattr->ref8);
					break;

				case DW_FORM_string:
					printf("%s\n", iattr->data->data);
					break;
				case DW_FORM_strp:
					if (iattr->data)
						printf("%s, ", iattr->data->data);
					printf("%u\n", iattr->str_offset);
					break;

				case DW_FORM_sec_offset:
					printf("%u\n", iattr->lineptr);
					break;

				case DW_FORM_exprloc:
					printf("%u\n", iattr->exprloc);
					break;
				case 0:
					printf("%u, %u\n", iattr->name, iattr->form);
					break;
				default:
					printf("%u, %u\n", iattr->name, iattr->form);
					break;
			};
		}
	}
}
