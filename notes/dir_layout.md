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
        - **drivers**: support for particular HW devices
        - **fs**: VHS and filesystem drivers
		- **mm**: physical and virtual memory management
		- **nonstd**: imitating standard library code
		- **sched**: kernel thread scheduling and synchronization
		- **util**: miscellaneous utility functions
