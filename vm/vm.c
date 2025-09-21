#include"vm.h"

extern vm_ops_t  vm_ops_naja;
extern vm_ops_t  vm_ops_naja_asm;

static vm_ops_t* vm_ops_array[] =
{
	&vm_ops_naja,
	&vm_ops_naja_asm,

	NULL,
};

int vm_open(vm_t** pvm, const char* arch)
{
	vm_ops_t* ops = NULL;
	vm_t*     vm;

	int  i;
	for (i = 0; vm_ops_array[i]; i++) {

		if (!strcmp(vm_ops_array[i]->name, arch)) {
			ops =   vm_ops_array[i];
			break;
		}
	}

	if (!ops) {
		loge("\n");
		return -EINVAL;
	}

	vm = calloc(1, sizeof(vm_t));
	if (!vm)
		return -ENOMEM;

	vm->ops = ops;

	if (vm->ops->open) {
		int ret = vm->ops->open(vm);
		if (ret < 0)
			return ret;
	}

	*pvm = vm;
	return 0;
}

int vm_clear(vm_t* vm)
{
	if (!vm)
		return -EINVAL;

	if (vm->elf) {
		elf_close(vm->elf);
		vm->elf = NULL;
	}

	if (vm->sofiles) {
		int  i;

		for (i = 0; i < vm->sofiles->size; i++)
			dlclose(vm->sofiles->data[i]);

		vector_free(vm->sofiles);
		vm->sofiles = NULL;
	}

	vm->text = NULL;
	vm->rodata = NULL;
	vm->data = NULL;

	return 0;
}

int vm_close(vm_t* vm)
{
	if (vm) {
		vm_clear(vm);

		if (vm->ops && vm->ops->close)
			vm->ops->close(vm);

		free(vm);
		vm = NULL;
	}

	return 0;
}

int vm_run(vm_t* vm, const char* path, const char* sys)
{
	if (vm  && vm->ops && vm->ops->run && path)
		return vm->ops->run(vm, path, sys);

	loge("\n");
	return -EINVAL;
}

