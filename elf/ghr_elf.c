#include"ghr_elf.h"

extern elf_ops_t	elf_ops_x64;
extern elf_ops_t	elf_ops_arm64;
extern elf_ops_t	elf_ops_arm32;
extern elf_ops_t	elf_ops_naja;

elf_ops_t*			elf_ops_array[] =
{
	&elf_ops_x64,
	&elf_ops_arm64,
	&elf_ops_arm32,
	&elf_ops_naja,

	NULL,
};

void elf_rela_free(elf_rela_t* rela)
{
	if (rela) {
		if (rela->name)
			free(rela->name);

		free(rela);
	}
}

int elf_open2(elf_context_t* elf, const char* machine)
{
	if (!elf->fp) {
		loge("\n");
		return -1;
	}

	int i;
	for (i = 0; elf_ops_array[i]; i++) {
		if (!strcmp(elf_ops_array[i]->machine, machine)) {
			elf->ops = elf_ops_array[i];
			break;
		}
	}

	if (!elf->ops) {
		loge("\n");
		return -1;
	}

	if (elf->ops->open && elf->ops->open(elf) == 0)
		return 0;

	loge("\n");
	return -1;
}

int elf_open(elf_context_t** pelf, const char* machine, const char* path, const char* mode)
{
	elf_context_t* elf;
	int i;

	elf = calloc(1, sizeof(elf_context_t));
	if (!elf)
		return -ENOMEM;

	for (i = 0; elf_ops_array[i]; i++) {
		if (!strcmp(elf_ops_array[i]->machine, machine)) {
			elf->ops = elf_ops_array[i];
			break;
		}
	}

	if (!elf->ops) {
		loge("\n");
		free(elf);
		return -1;
	}

	elf->fp = fopen(path, mode);
	if (!elf->fp) {
		loge("\n");
		free(elf);
		return -1;
	}

	if (elf->ops->open && elf->ops->open(elf) == 0) {
		*pelf = elf;
		return 0;
	}

	loge("\n");

	fclose(elf->fp);
	free(elf);
	elf = NULL;
	return -1;
}

int elf_close(elf_context_t* elf)
{
	if (elf) {
		if (elf->ops && elf->ops->close)
			elf->ops->close(elf);

		if (elf->fp)
			fclose(elf->fp);

		free(elf);
		elf = NULL;
		return 0;
	}

	loge("\n");
	return -1;
}

int elf_add_sym(elf_context_t* elf, const elf_sym_t* sym, const char* sh_name)
{
	if (elf && sym && sh_name) {

		if (elf->ops && elf->ops->add_sym)
			return elf->ops->add_sym(elf, sym, sh_name);
	}

	loge("\n");
	return -1;
}

int elf_add_section(elf_context_t* elf, const elf_section_t* section)
{
	if (elf && section) {

		if (elf->ops && elf->ops->add_section)
			return elf->ops->add_section(elf, section);
	}

	loge("\n");
	return -1;
}

int	elf_add_rela_section(elf_context_t* elf, const elf_section_t* section, vector_t* relas)
{
	if (elf && section && relas) {

		if (elf->ops && elf->ops->add_rela_section)
			return elf->ops->add_rela_section(elf, section, relas);
	}

	loge("\n");
	return -1;
}

int	elf_add_dyn_rela(elf_context_t* elf, const elf_rela_t* rela)
{
	if (elf && rela) {

		if (elf->ops && elf->ops->add_dyn_rela)
			return elf->ops->add_dyn_rela(elf, rela);
	}

	loge("\n");
	return -1;
}

int	elf_add_dyn_need(elf_context_t* elf, const char* soname)
{
	if (elf && soname) {

		if (elf->ops && elf->ops->add_dyn_need)
			return elf->ops->add_dyn_need(elf, soname);
	}

	loge("\n");
	return -1;
}

int elf_read_section(elf_context_t* elf, elf_section_t** psection, const char* name)
{
	if (elf && psection && name) {

		if (elf->ops && elf->ops->read_section)
			return elf->ops->read_section(elf, psection, name);
	}

	loge("\n");
	return -1;
}

int elf_read_syms(elf_context_t* elf, vector_t* syms, const char* sh_name)
{
	if (elf && syms && sh_name) {

		if (elf->ops && elf->ops->read_syms)
			return elf->ops->read_syms(elf, syms, sh_name);
	}

	loge("\n");
	return -1;
}

int elf_read_relas(elf_context_t* elf, vector_t* relas, const char* sh_name)
{
	if (elf && relas && sh_name) {

		if (elf->ops && elf->ops->read_relas)
			return elf->ops->read_relas(elf, relas, sh_name);
	}

	loge("\n");
	return -1;
}

int elf_read_phdrs(elf_context_t* elf, vector_t* phdrs)
{
	if (elf && elf->ops && elf->ops->read_phdrs && phdrs)
		return elf->ops->read_phdrs(elf, phdrs);

	loge("\n");
	return -1;
}

int elf_write_rel(elf_context_t* elf)
{
	if (elf && elf->ops && elf->ops->write_rel)
		return elf->ops->write_rel(elf);

	loge("\n");
	return -1;
}

int elf_write_exec(elf_context_t* elf, const char* sysroot)
{
	if (elf && elf->ops && elf->ops->write_exec && sysroot)
		return elf->ops->write_exec(elf, sysroot);

	loge("\n");
	return -1;
}

int elf_write_dyn(elf_context_t* elf, const char* sysroot)
{
	if (elf && elf->ops && elf->ops->write_dyn && sysroot)
		return elf->ops->write_dyn(elf, sysroot);

	loge("\n");
	return -1;
}
