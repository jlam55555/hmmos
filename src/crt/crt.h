#pragma once

/// C/C++ runtime support. This is separated out into its own
/// directory since the order of crt{i,n}.o when linking is important.
///
/// TODO: runtime support for dynamic memory allocation, etc.
///
/// Runtime feature support:
///
/// ----------------------------------------------------------------------------
/// Virtual functions
/// ----------------------------------------------------------------------------
/// A trivial implementation of __cxa_pure_virtual() is provided. This
/// function is needed for using virtual functions (but will never be
/// called unless a virtual function call can't be made.
///
/// ----------------------------------------------------------------------------
/// Global ctors/dtors
/// ----------------------------------------------------------------------------
/// GCC/clang both compile constructor and destructor thunks into
/// special sections called `.init_array` and `.fini_array`,
/// respectively. We need to manually run all ctor code at startup and
/// all dtor code at shutdown (although this may not be necessary, as
/// we're already shutting down).
///
/// Constructor thunks include global object constructors, and
/// functions declared with attribute "constructor". Destructor thunks
/// only include functions declared with attribute "destructor" --
/// global object destructors are called via the __cxa_atexit()
/// mechanism.
///
/// The way we do this is by getting the start and end of these
/// sections. ld doesn't create `__{start,stop}_{init,fini}_array`
/// symbols since the section names aren't valid identifiers (they
/// start with a dot), so we have to manually insert symbols at the
/// start and end of these sections. We do this in `crti.o` and
/// `crtn.o`, which are the first and last objects fed to the linker,
/// respectively.
///
/// Note that this is the newer way of specifying global ctors/dtors;
/// this supercedes the .ctor/.cdtor/.init/.fini sections (which is
/// what OSDev still currently describes for x86). GCC/clang use this
/// newer .init_array/.fini_array method.
///
/// ----------------------------------------------------------------------------
/// __cxa_atexit()/__cxa_finalize()
/// ----------------------------------------------------------------------------
/// These are required for global object destructors. For simplicity
/// this uses a static-sized array, which can be bumped manually when
/// needed.
///
/// The implementation is mostly ripped off of OSDev.
///

namespace crt {

/// Run global ctors/dtors.
void run_global_ctors();
void run_global_dtors();

namespace detail {

using funcptr = void (*)();

// Usually ld should define symbols like this automatically, but this
// only works if the section name is a valid C identifier. .init_array
// starts with a period so we need to define this symbol manually.
extern funcptr __start_init_array[0], __stop_init_array[0];
extern funcptr __start_fini_array[0], __stop_fini_array[0];

} // namespace detail

} // namespace crt
