# directory layout

- **notes**: self-explanatory
- **scripts**: helper scripts for build/test
- **out[\<variant\>]**: build outputs. mirrors the layout of src
- **src**: self-explanatory
	- **boot**: bootloader code
	- **common**: shared bootloader/kernel code
	- **crt**: C/C++ runtime support code
	- **test**: kernel test code
	- **kernel**: kernel code
		- **arch**: architecture-specific code. Right now it's all
          x86, but if this project ever branches to other
          architectures, only the code for the correct architecture
          will actually be compiled
        - **fs**: VHS and filesystem drivers
        - **drivers**: support for particular HW devices
		- **mm**: physical and virtual memory management
		- **nonstd**: imitating standard library code
		- **proc**: process abstraction
		- **sched**: kernel thread scheduling and synchronization
		- **util**: miscellaneous utility functions
	- **userspace**: userspace code. Will be compiled separately into
      ELF images separate from the bootloader/kernel code.
