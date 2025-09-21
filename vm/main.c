#include"scf_vm.h"

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("usage: ./nvm file [opt]\n\n");
		printf("file: an ELF file with naja bytecode\n");
		printf("opt : 0 to run, 1 to disassemble, default 0\n");
		return -1;
	}

	char* arch = "naja";

	if (argc > 2) {
		if (1 == atoi(argv[2]))
			arch = "naja_asm";
	}

	scf_vm_t* vm = NULL;

	int ret = scf_vm_open(&vm, arch);
	if (ret < 0) {
		scf_loge("\n");
		return -1;
	}

	ret = scf_vm_run(vm, argv[1], "x64");
	if (ret < 0) {
		scf_loge("\n");
		return -1;
	}

	printf("main ok\n");
	return ret;
}
