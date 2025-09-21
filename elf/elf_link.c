#include"elf_link.h"

void ar_sym_free(ar_sym_t* sym)
{
	if (sym) {
		if (sym->name)
			string_free(sym->name);
		free(sym);
	}
}

int __elf_file_open(elf_file_t** pfile)
{
	elf_file_t* ef = calloc(1, sizeof(elf_file_t));
	if (!ef)
		return -ENOMEM;

	ef->text_idx   = -1;
	ef->rodata_idx = -1;
	ef->data_idx   = -1;

	ef->abbrev_idx = -1;
	ef->info_idx   = -1;
	ef->line_idx   = -1;
	ef->str_idx    = -1;

	int ret;

	ef->text = string_alloc();
	if (!ef->text) {
		ret = -ENOMEM;
		goto text_error;
	}

	ef->rodata = string_alloc();
	if (!ef->rodata) {
		ret = -ENOMEM;
		goto rodata_error;
	}

	ef->data = string_alloc();
	if (!ef->data) {
		ret = -ENOMEM;
		goto data_error;
	}

	ef->debug_abbrev = string_alloc();
	if (!ef->debug_abbrev) {
		ret = -ENOMEM;
		goto debug_abbrev_error;
	}

	ef->debug_info = string_alloc();
	if (!ef->debug_info) {
		ret = -ENOMEM;
		goto debug_info_error;
	}

	ef->debug_line = string_alloc();
	if (!ef->debug_line) {
		ret = -ENOMEM;
		goto debug_line_error;
	}

	ef->debug_str = string_alloc();
	if (!ef->debug_str) {
		ret = -ENOMEM;
		goto debug_str_error;
	}

	ef->syms = vector_alloc();
	if (!ef->syms) {
		ret = -ENOMEM;
		goto syms_error;
	}

	ef->text_relas = vector_alloc();
	if (!ef->text_relas) {
		ret = -ENOMEM;
		goto text_relas_error;
	}

	ef->data_relas = vector_alloc();
	if (!ef->data_relas) {
		ret = -ENOMEM;
		goto data_relas_error;
	}

	ef->debug_line_relas = vector_alloc();
	if (!ef->debug_line_relas) {
		ret = -ENOMEM;
		goto debug_line_relas_error;
	}

	ef->debug_info_relas = vector_alloc();
	if (!ef->debug_info_relas) {
		ret = -ENOMEM;
		goto debug_info_relas_error;
	}

	ef->dyn_syms  = vector_alloc();
	ef->dyn_needs = vector_alloc();
	ef->rela_plt  = vector_alloc();

	*pfile = ef;
	return 0;

elf_open_error:
	vector_free(ef->debug_info_relas);
debug_info_relas_error:
	vector_free(ef->debug_line_relas);
debug_line_relas_error:
	vector_free(ef->data_relas);
data_relas_error:
	vector_free(ef->text_relas);
text_relas_error:
	vector_free(ef->syms);
syms_error:
	string_free(ef->debug_str);
debug_str_error:
	string_free(ef->debug_line);
debug_line_error:
	string_free(ef->debug_info);
debug_info_error:
	string_free(ef->debug_abbrev);
debug_abbrev_error:
	string_free(ef->data);
data_error:
	string_free(ef->rodata);
rodata_error:
	string_free(ef->text);
text_error:
	if (ef->name)
		string_free(ef->name);
	free(ef);
	return ret;
}

int elf_file_open(elf_file_t** pfile, const char* path, const char* mode, const char* arch)
{
	elf_file_t* ef = NULL;

	int ret = __elf_file_open(&ef);
	if (ret < 0)
		return ret;

	ret = elf_open(&ef->elf, arch, path, mode);
	if (ret < 0) {
		elf_file_close(ef, NULL, NULL);
		return ret;
	}

	ef->name = string_cstr(path);

	*pfile = ef;
	return 0;
}

int so_file_open(elf_file_t** pso, const char* path, const char* mode, const char* arch)
{
	elf_file_t* so = NULL;

	int ret = __elf_file_open(&so);
	if (ret < 0)
		return ret;

	ret = elf_open(&so->elf, arch, path, mode);
	if (ret < 0) {
		elf_file_close(so, NULL, NULL);
		return ret;
	}

	so->name = string_cstr(path);

	so->dyn_syms = vector_alloc();
	if (!so->dyn_syms)
		return -ENOMEM;

	so->rela_plt = vector_alloc();
	if (!so->rela_plt)
		return -ENOMEM;

	ret = elf_read_syms(so->elf, so->dyn_syms, ".dynsym");
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = elf_read_relas(so->elf, so->rela_plt, ".rela.plt");
	if (ret < 0 && -404 != ret) {
		loge("\n");
		return ret;
	}

#if 1
	int i;
	for (i = 0; i < so->dyn_syms->size; i++) {
		elf_sym_t* esym = so->dyn_syms->data[i];

		logd("i: %d, sym: %s, shndx: %d\n", i, esym->name, esym->st_shndx);
	}

	for (i = 0; i < so->rela_plt->size; i++) {
		elf_rela_t* er = so->rela_plt->data[i];

		logd("i: %d, rela: %s\n", i, er->name);
	}
#endif

	*pso = so;
	return 0;
}

int elf_file_close(elf_file_t* ef, void (*rela_free)(void*), void (*sym_free)(void*))
{
	if (ef->elf)
		elf_close(ef->elf);

	string_free(ef->text);
	string_free(ef->data);

	string_free(ef->debug_abbrev);
	string_free(ef->debug_info);
	string_free(ef->debug_line);
	string_free(ef->debug_str);

	vector_clear(ef->text_relas,       rela_free);
	vector_clear(ef->data_relas,       rela_free);
	vector_clear(ef->debug_line_relas, rela_free);

	vector_free (ef->text_relas);
	vector_free (ef->data_relas);
	vector_free (ef->debug_line_relas);

	vector_clear(ef->syms, sym_free);
	vector_free (ef->syms);

	free(ef);
	return 0;
}

int elf_file_read(elf_file_t* ef)
{
#define ELF_READ_SECTION(sname, idx) \
	do {\
		elf_section_t* s = NULL; \
		\
		int ret = elf_read_section(ef->elf, &s, "."#sname); \
		if (ret < 0) { \
			if (-404 != ret) { \
				loge("\n"); \
				return ret; \
			} \
		} else { \
			if (s->data_len > 0) \
				ret = string_cat_cstr_len(ef->sname, s->data, s->data_len); \
			\
			(idx) = s->index; \
			free(s);  \
			s = NULL; \
			if (ret < 0) { \
				loge("\n"); \
				return ret; \
			} \
		} \
	} while (0)

	ELF_READ_SECTION(text,   ef->text_idx);
	ELF_READ_SECTION(rodata, ef->rodata_idx);
	ELF_READ_SECTION(data,   ef->data_idx);

	ELF_READ_SECTION(debug_abbrev, ef->abbrev_idx);
	ELF_READ_SECTION(debug_info,   ef->info_idx);
	ELF_READ_SECTION(debug_line,   ef->line_idx);
	ELF_READ_SECTION(debug_str,    ef->str_idx);

	int ret = elf_read_syms(ef->elf, ef->syms, ".symtab");
	if (ret < 0) {
		loge("\n");
		return ret;
	}

#define ELF_READ_RELAS(sname) \
	do { \
		int ret = elf_read_relas(ef->elf, ef->sname##_relas, ".rela."#sname); \
		if (ret < 0) { \
			if (ret != -404) { \
				loge("\n"); \
				return ret; \
			} \
			\
			ret = elf_read_relas(ef->elf, ef->sname##_relas, ".rel."#sname); \
			if (ret < 0 && ret != -404) { \
				loge("\n"); \
				return ret; \
			} \
		} \
	} while(0);

	ELF_READ_RELAS(text);
	ELF_READ_RELAS(data);
	ELF_READ_RELAS(debug_info);
	ELF_READ_RELAS(debug_line);

	return 0;
}

static int ar_symbols(ar_file_t* ar)
{
	ar_sym_t* sym;

	char magic[SARMAG];

	int ret = fread(magic, SARMAG, 1, ar->fp);
	if (ret < 0)
		return ret;

	if (strcmp(magic, ARMAG))
		return -1;

	struct ar_hdr hdr;

	ret = fread(&hdr, sizeof(hdr), 1, ar->fp);
	if (ret != 1)
		return -1;

	uint64_t ar_size = 0;
	uint32_t nb_syms = 0;
	uint32_t offset  = 0;
	uint8_t* buf     = NULL;
	int      i;
	int      j;

	for (i = 0; i < sizeof(hdr.ar_size) / sizeof(hdr.ar_size[0]); i++) {

		if (' ' == hdr.ar_size[i])
			break;

		ar_size *= 10;
		ar_size += hdr.ar_size[i] - '0';
	}

	buf = malloc(ar_size);
	if (!buf)
		return -ENOMEM;

	ret = fread(buf, ar_size, 1, ar->fp);
	if (ret != 1) {
		ret = -1;
		goto error;
	}

	for (i = 0; i  < sizeof(nb_syms); i++) {
		nb_syms   <<= 8;
		nb_syms   +=  buf[i];
	}

	for (i = 0; i < nb_syms; i++) {

		int k  = sizeof(nb_syms) + i * sizeof(offset);
		offset = 0;

		for (j = 0; j < sizeof(offset); j++) {

			offset <<= 8;
			offset  += buf[k + j];
		}

		sym = calloc(1, sizeof(ar_sym_t));
		if (!sym) {
			ret = -ENOMEM;
			goto error;
		}

		if (vector_add(ar->symbols, sym) < 0) {
			free(sym);
			ret = -ENOMEM;
			goto error;
		}

		sym->offset = offset;
	}

	j = sizeof(nb_syms) + sizeof(offset) * nb_syms;

	for (i  = 0; i < nb_syms; i++) {

		sym = ar->symbols->data[i];

		int k = 0;

		assert(j < ar_size);

		while (buf[j + k])
			k++;

		sym->name = string_cstr_len(buf + j, k);
		if (!sym->name) {
			ret = -ENOMEM;
			goto error;
		}

		j += k + 1;
	}

	assert(j == ar_size);
	return 0;

error:
	vector_clear(ar->symbols, ( void (*)(void*) )ar_sym_free);
	free(buf);
	return ret;
}

int ar_file_open(ar_file_t** par, const char* path)
{
	ar_file_t* ar = calloc(1, sizeof(ar_file_t));
	if (!ar)
		return -ENOMEM;

	int ret;

	ar->symbols = vector_alloc();
	if (!ar->symbols) {
		ret = -ENOMEM;
		goto sym_error;
	}

	ar->files = vector_alloc();
	if (!ar->files) {
		ret = -ENOMEM;
		goto file_error;
	}

	ar->fp = fopen(path, "rb");
	if (!ar->fp) {
		ret = -1;
		goto open_error;
	}

	ret = ar_symbols(ar);
	if (ret < 0)
		goto error;

	*par = ar;
	return 0;

error:
	fclose(ar->fp);
open_error:
	vector_free(ar->files);
file_error:
	vector_free(ar->symbols);
sym_error:
	free(ar);
	return ret;
}

static int merge_relas(vector_t* dst, vector_t* src, size_t offset, int nb_syms, const int bits)
{
	elf_rela_t* rela;
	elf_rela_t* rela2;

	int  j;
	for (j    = 0; j < src->size; j++) {
		rela2 =        src->data[j];

		rela2->r_offset += offset;

		if (64 == bits)
			rela2->r_info = ELF64_R_INFO(ELF64_R_SYM(rela2->r_info) + nb_syms, ELF64_R_TYPE(rela2->r_info));
		else
			rela2->r_info = ELF32_R_INFO(ELF32_R_SYM(rela2->r_info) + nb_syms, ELF32_R_TYPE(rela2->r_info));

		rela = calloc(1, sizeof(elf_rela_t));
		if (!rela)
			return -ENOMEM;

		rela->name = strdup(rela2->name);
		if (!rela->name) {
			free(rela);
			return -ENOMEM;
		}

		if (vector_add(dst, rela) < 0) {
			free(rela->name);
			free(rela);
			return -ENOMEM;
		}

		logd("j: %d, nb_syms: %d:%ld, sym: %s\n", j, nb_syms, ELF32_R_SYM(rela2->r_info), rela2->name);

		rela->r_offset = rela2->r_offset;
		rela->r_info   = rela2->r_info;
		rela->r_addend = rela2->r_addend;
	}

	return 0;
}

static int merge_syms(elf_file_t* exec, elf_file_t* obj)
{
	elf_sym_t*  sym;
	elf_sym_t*  sym2;

	int  j;
	for (j   = 0; j < obj->syms->size; j++) {
		sym2 =        obj->syms->data[j];

		if (obj->text_idx == sym2->st_shndx) {

			sym2->st_value  += exec->text->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(text);

		} else if (obj->rodata_idx == sym2->st_shndx) {

			sym2->st_value  += exec->rodata->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(rodata);

		} else if (obj->data_idx == sym2->st_shndx) {

			sym2->st_value  += exec->data->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(data);

		} else if (obj->abbrev_idx == sym2->st_shndx) {

			sym2->st_value  += exec->debug_abbrev->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(debug_abbrev);

		} else if (obj->info_idx == sym2->st_shndx) {

			sym2->st_value  += exec->debug_info->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(debug_info);

		} else if (obj->line_idx == sym2->st_shndx) {

			sym2->st_value  += exec->debug_line->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(debug_line);

		} else if (obj->str_idx == sym2->st_shndx) {

			sym2->st_value  += exec->debug_str->len;
			sym2->st_shndx  =  ELF_FILE_SHNDX(debug_str);
		} else
			logd("sym2->st_shndx: %d, cs: %d, ds: %d\n", sym2->st_shndx, obj->cs_idx, obj->ds_idx);

		sym = calloc(1, sizeof(elf_sym_t));
		if (!sym)
			return -ENOMEM;

		sym->name = strdup(sym2->name);
		if (!sym->name) {
			free(sym);
			return -ENOMEM;
		}

		logd("sym: %d, %s\n", exec->syms->size, sym->name);

		if (vector_add(exec->syms, sym) < 0) {
			loge("\n");
			return -ENOMEM;
		}

		sym->st_size  = sym2->st_size;
		sym->st_value = sym2->st_value;
		sym->st_shndx = sym2->st_shndx;
		sym->st_info  = sym2->st_info;
	}

	return 0;
}

static int merge_obj(elf_file_t* exec, elf_file_t* obj, const int bits)
{
	int nb_syms = exec->syms->size;

#define MERGE_RELAS(dst, src, offset) \
		do { \
			int ret = merge_relas(dst, src, offset, nb_syms, bits); \
			if (ret < 0) { \
				loge("\n"); \
				return ret; \
			} \
		} while (0)

		MERGE_RELAS(exec->text_relas,       obj->text_relas,       exec->text->len);
		MERGE_RELAS(exec->data_relas,       obj->data_relas,       exec->data->len);
		MERGE_RELAS(exec->debug_line_relas, obj->debug_line_relas, exec->debug_line->len);
		MERGE_RELAS(exec->debug_info_relas, obj->debug_info_relas, exec->debug_info->len);

		if (merge_syms(exec, obj) < 0) {
			loge("\n");
			return -1;
		}

		nb_syms += obj->syms->size;

#define MERGE_BIN(dst, src) \
		do { \
			if (src->len > 0) { \
				int ret = string_cat(dst, src); \
				if (ret < 0) { \
					loge("\n"); \
					return ret; \
				} \
			} \
		} while (0)

		MERGE_BIN(exec->text,         obj->text);
		MERGE_BIN(exec->rodata,       obj->rodata);
		MERGE_BIN(exec->data,         obj->data);

		MERGE_BIN(exec->debug_abbrev, obj->debug_abbrev);
		MERGE_BIN(exec->debug_info,   obj->debug_info);
		MERGE_BIN(exec->debug_line,   obj->debug_line);
		MERGE_BIN(exec->debug_str,    obj->debug_str);
		return 0;
}

static int merge_objs(elf_file_t* exec, char* inputs[], int nb_inputs, const char* arch)
{
	int nb_syms = 0;
	int bits;
	int i;

	if (!strcmp(arch, "x64") || !strcmp(arch, "arm64") || !strcmp(arch, "naja"))
		bits = 64;
	else
		bits = 32;

	for (i = 0; i < nb_inputs; i++) {

		elf_sym_t*  sym;
		elf_sym_t*  sym2;

		elf_file_t* obj = NULL;

		if (elf_file_open(&obj, inputs[i], "rb", arch) < 0) {
			loge("inputs[%d]: %s\n", i, inputs[i]);
			return -1;
		}

		if (elf_file_read(obj) < 0) {
			loge("\n");
			return -1;
		}

		logi("i: %d, input: %s\n", i, inputs[i]);

		int ret = merge_obj(exec, obj, bits);
		if (ret < 0) {
			loge("\n");
			return -1;
		}

		elf_file_close(obj, free, free);
		obj = NULL;
	}

	return 0;
}

static int _find_sym(elf_sym_t** psym, elf_rela_t* rela, vector_t* symbols, const int bits)
{
	elf_sym_t*  sym;
	elf_sym_t*  sym2;

	int sym_idx;
	int j;

	if (64 == bits)
		sym_idx = ELF64_R_SYM(rela->r_info);
	else
		sym_idx = ELF32_R_SYM(rela->r_info);

	assert(sym_idx >= 1);
	assert(sym_idx -  1 < symbols->size);

	sym = symbols->data[sym_idx - 1];

	if (0 == sym->st_shndx) {

		int n = 0;

		for (j   = 0; j < symbols->size; j++) {
			sym2 =        symbols->data[j];

			if (0 == sym2->st_shndx)
				continue;

			if (STB_LOCAL == ELF64_ST_BIND(sym2->st_info))
				continue;

			if (!strcmp(sym2->name, sym->name)) {
				sym     = sym2;
				sym_idx = j + 1;
				n++;
			}
		}

		if (0 == n)
			return -1;
		else if (n > 1) {
			logi("tow global symbol: %s\n", sym->name);
			return -1;
		}
	}

	*psym = sym;
	return sym_idx;
}

static int _find_lib_sym(ar_file_t** par, uint32_t* poffset, uint32_t* psize, vector_t* libs, elf_sym_t* sym)
{
	ar_file_t* ar   = NULL;
	ar_sym_t*  asym = NULL;

	int j;
	for (j = 0; j < libs->size; j++) {
		ar =        libs->data[j];

		int k;
		for (k   = 0; k < ar->symbols->size; k++) {
			asym =        ar->symbols->data[k];

			if (!strcmp(sym->name, asym->name->data))
				break;
		}

		if (k < ar->symbols->size)
			break;
	}

	if (j == libs->size)
		return -1;

	fseek(ar->fp, asym->offset, SEEK_SET);

	struct ar_hdr hdr;

	int ret = fread(&hdr, sizeof(hdr), 1, ar->fp);
	if (ret != 1)
		return -1;

	uint64_t ar_size = 0;

	int i;
	for (i = 0; i < sizeof(hdr.ar_size) / sizeof(hdr.ar_size[0]); i++) {

		if (' ' == hdr.ar_size[i])
			break;

		ar_size *= 10;
		ar_size += hdr.ar_size[i] - '0';
	}

	*poffset = asym->offset + sizeof(hdr);
	*psize   = ar_size;
	*par     = ar;

	return 0;
}

static int merge_ar_obj(elf_file_t* exec, ar_file_t* ar, uint32_t offset, uint32_t size, const char* arch)
{
	elf_file_t* obj = NULL;

	int bits;
	if (!strcmp(arch, "x64") || !strcmp(arch, "arm64") || !strcmp(arch, "naja"))
		bits = 64;
	else
		bits = 32;

	int ret = __elf_file_open(&obj);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	obj->elf = calloc(1, sizeof(elf_context_t));
	assert(obj->elf);

	obj->elf->fp = ar->fp;
	obj->elf->start = offset;
	obj->elf->end   = offset + size;

	ret = elf_open2(obj->elf, arch);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	ret = elf_file_read(obj);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	ret = merge_obj(exec, obj, bits);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	obj->elf->fp = NULL;
	elf_file_close(obj, free, free);
	obj = NULL;

	return 0;
}

static int _find_so_sym(elf_file_t** pso, vector_t* dlls, elf_sym_t* sym)
{
	elf_file_t* so = NULL;
	elf_sym_t*  sym2;

	int j;
	for (j = 0; j < dlls->size; j++) {
		so =        dlls->data[j];

		logd("so: %p\n", so);
		int k;
		for (k   = 0; k < so->dyn_syms->size; k++) {
			sym2 =        so->dyn_syms->data[k];

			if (0 == sym2->st_shndx)
				continue;

			if (!strcmp(sym->name, sym2->name))
				break;
		}

		if (k < so->dyn_syms->size)
			break;
	}

	if (j == dlls->size) {
		loge("linker don't find symbel '%s'\n", sym->name);
		return -1;
	}

	if (pso)
		*pso = so;
	return 0;
}

static int link_relas(elf_file_t* exec, char* afiles[], int nb_afiles, char* sofiles[], int nb_sofiles, const char* arch)
{
	elf_rela_t* rela = NULL;
	elf_sym_t*  sym  = NULL;
	elf_sym_t*  sym2 = NULL;
	ar_file_t*  ar   = NULL;
	elf_file_t* so   = NULL;
	elf_file_t* so2  = NULL;
	vector_t*   libs = vector_alloc();
	vector_t*   dlls = vector_alloc();

	int i;
	int bits;
	if (!strcmp(arch, "x64") || !strcmp(arch, "arm64") || !strcmp(arch, "naja"))
		bits = 64;
	else
		bits = 32;

	for (i = 0; i < nb_afiles; i++) {

		if (ar_file_open(&ar, afiles[i]) < 0) {
			loge("\n");
			return -1;
		}

		if (vector_add(libs, ar) < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i = 0; i < nb_sofiles; i++) {

		if (so_file_open(&so, sofiles[i], "rb", arch) < 0) {
			loge("\n");
			return -1;
		}

		if (vector_add(dlls, so) < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i = 0; i < exec->text_relas->size; ) {
		rela      = exec->text_relas->data[i];

		sym = NULL;
		int sym_idx = _find_sym(&sym, rela, exec->syms, bits);

		if (sym_idx >= 0) {
			i++;
			continue;
		}

		if (64 == bits)
			sym_idx = ELF64_R_SYM(rela->r_info);
		else
			sym_idx = ELF32_R_SYM(rela->r_info);

		sym = exec->syms->data[sym_idx - 1];

		uint32_t offset = 0;
		uint32_t size   = 0;

		int ret = _find_lib_sym(&ar, &offset, &size, libs, sym);
		if (ret >= 0) {
			logd("sym: %s, offset: %d, size: %d\n\n", sym->name, offset, size);

			ret = merge_ar_obj(exec, ar, offset, size, arch);
			if (ret < 0) {
				loge("\n");
				return -1;
			}

			i++;
			continue;
		}

		ret = _find_so_sym(&so, dlls, sym);
		if (ret < 0) {
			loge("\n");
			return -1;
		}
		sym->dyn_flag = 1;

		int j;
		for (j = 0; j < exec->dyn_syms->size; j++) {
			sym2      = exec->dyn_syms->data[j];

			if (!strcmp(sym2->name, sym->name))
				break;
		}

		if (j == exec->dyn_syms->size) {
			sym2 = calloc(1, sizeof(elf_sym_t));
			if (!sym2)
				return -ENOMEM;

			sym2->name = strdup(sym->name);
			if (!sym2->name) {
				free(sym2);
				return -ENOMEM;
			}
			sym2->st_info  = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
			sym2->dyn_flag = 1;

			if (vector_add(exec->dyn_syms, sym2) < 0) {
				loge("\n");
				return -ENOMEM;
			}
		}

		vector_add_unique(exec->dyn_needs, so);
		vector_del(exec->text_relas, rela);
		vector_add(exec->rela_plt,   rela);

		if (64 == bits)
			rela->r_info = ELF64_R_INFO(j + 1, ELF64_R_TYPE(rela->r_info));
		else
			rela->r_info = ELF32_R_INFO(j + 1, ELF32_R_TYPE(rela->r_info));

		logd("j: %d, sym: %s, r_offset: %#lx, r_addend: %ld\n", j,
				sym->name, rela->r_offset, rela->r_addend);
	}

	for (i = 0; i < exec->data_relas->size; i++) {
		rela      = exec->data_relas->data[i];

		sym = NULL;
		int sym_idx = _find_sym(&sym, rela, exec->syms, bits);

		if (sym_idx >= 0)
			continue;

		if (64 == bits)
			sym_idx = ELF64_R_SYM(rela->r_info);
		else
			sym_idx = ELF32_R_SYM(rela->r_info);

		sym = exec->syms->data[sym_idx - 1];

		uint32_t offset = 0;
		uint32_t size   = 0;

		int ret = _find_lib_sym(&ar, &offset, &size, libs, sym);
		if (ret < 0) {
			loge("\n");
			return -1;
		}

		logi("sym: %s, offset: %d, size: %d\n\n", sym->name, offset, size);

		ret = merge_ar_obj(exec, ar, offset, size, arch);
		if (ret < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i = 0; i < exec->dyn_needs->size; i++) {
		so =        exec->dyn_needs->data[i];

		logd("so: %s\n", so->name->data);

		int sym_idx;
		int j;
		for (j = 0; j < so->rela_plt->size; j++) {
			rela      = so->rela_plt->data[j];

			if (64 == bits)
				sym_idx = ELF64_R_SYM(rela->r_info);
			else
				sym_idx = ELF32_R_SYM(rela->r_info);

			if (sym_idx <= 0)
				continue;

			sym = so->dyn_syms->data[sym_idx - 1];

			int ret = _find_so_sym(&so2, dlls, sym);
			if (ret < 0) {
				loge("can't find sym '%s' used in '%s'\n", sym->name, so->name->data);
				return -1;
			}

			vector_add_unique(exec->dyn_needs, so2);
		}
	}
	return 0;
}

int elf_link(vector_t* objs, vector_t* afiles, vector_t* sofiles, const char* sysroot, const char* arch, const char* out, int dyn_flag)
{
	elf_file_t* exec = NULL;
	elf_file_t* so   = NULL;
	elf_rela_t* rela = NULL;
	elf_sym_t*  sym  = NULL;

	int ret;
	int i;

	ret = elf_file_open(&exec, out, "wb", arch);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = merge_objs(exec, (char**)objs->data, objs->size, arch);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = link_relas(exec, (char**)afiles->data, afiles->size, (char**)sofiles->data, sofiles->size, arch);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	for (i  = 0; i < exec->syms->size; i++) {
		sym =        exec->syms->data[i];

		if (elf_add_sym(exec->elf, sym, ".symtab") < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i  = 0; i < exec->dyn_syms->size; i++) {
		sym =        exec->dyn_syms->data[i];

		if (elf_add_sym(exec->elf, sym, ".dynsym") < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i   = 0; i < exec->rela_plt->size; i++) {
		rela =        exec->rela_plt->data[i];

		if (elf_add_dyn_rela(exec->elf, rela) < 0) {
			loge("\n");
			return -1;
		}
	}

	for (i = 0; i < exec->dyn_needs->size; i++) {
		so =        exec->dyn_needs->data[i];

		if (elf_add_dyn_need(exec->elf, so->name->data) < 0) {
			loge("\n");
			return -1;
		}
	}

	size_t bytes = exec->debug_abbrev->len
		         + exec->debug_info->len
		         + exec->debug_line->len
		         + exec->debug_str->len;

	bytes &= 0x7;
	if (bytes) {
		ret = string_fill_zero(exec->debug_str, 8 - bytes);
		if (ret < 0)
			return ret;
	}

	bytes = 0;

#define ADD_SECTION(sname, flags, align, value) \
	do { \
		elf_section_t s   = {0}; \
		elf_sym_t     sym = {0}; \
		\
		s.name                = "."#sname; \
		s.sh_type             = SHT_PROGBITS; \
		s.sh_flags            = flags; \
		s.sh_addralign        = align; \
		s.data                = exec->sname->data; \
		s.data_len            = exec->sname->len; \
		\
		ret = elf_add_section(exec->elf, &s); \
		if (ret < 0) { \
			loge("\n"); \
			return ret; \
		} \
		\
		sym.name      = "."#sname;  \
		sym.st_size   = s.data_len; \
		sym.st_value  = value;      \
		sym.st_shndx  = ELF_FILE_SHNDX(sname); \
		sym.st_info   = ELF64_ST_INFO(STB_LOCAL, STT_SECTION); \
		bytes        += s.data_len; \
		\
		ret = elf_add_sym(exec->elf, &sym, ".symtab"); \
		if (ret < 0) { \
			loge("\n"); \
			return ret; \
		} \
	} while (0)

	ADD_SECTION(text,   SHF_ALLOC | SHF_EXECINSTR, 1, 0);
	ADD_SECTION(rodata, SHF_ALLOC,                 8, 0);
	ADD_SECTION(data,   SHF_ALLOC | SHF_WRITE,     8, 0);

	ADD_SECTION(debug_abbrev, 0, 8, bytes);
	ADD_SECTION(debug_info,   0, 8, bytes);
	ADD_SECTION(debug_line,   0, 8, bytes);
	ADD_SECTION(debug_str,    0, 8, bytes);

#define ADD_RELA_SECTION(sname, info) \
	do { \
		if (exec->sname##_relas->size > 0) { \
			\
			elf_section_t s; \
			\
			s.name         = ".rela."#sname; \
			s.sh_type      = SHT_RELA; \
			s.sh_flags     = SHF_INFO_LINK; \
			s.sh_addralign = 8; \
			s.data         = NULL; \
			s.data_len     = 0; \
			s.sh_link      = 0; \
			s.sh_info      = info; \
			\
			ret = elf_add_rela_section(exec->elf, &s, exec->sname##_relas); \
			if (ret < 0) { \
				loge("\n"); \
				return ret; \
			} \
		} \
	} while(0)

	ADD_RELA_SECTION(text,       ELF_FILE_SHNDX(text));
	ADD_RELA_SECTION(data,       ELF_FILE_SHNDX(data));
	ADD_RELA_SECTION(debug_info, ELF_FILE_SHNDX(debug_info));
	ADD_RELA_SECTION(debug_line, ELF_FILE_SHNDX(debug_line));

	if (dyn_flag)
		ret = elf_write_dyn(exec->elf, sysroot);
	else
		ret = elf_write_exec(exec->elf, sysroot);
	if (ret < 0)
		return ret;

	elf_file_close(exec, free, free);

	return 0;
}
