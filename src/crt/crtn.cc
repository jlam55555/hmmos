using funcptr = void (*)();

namespace crt::detail {

__attribute__((section(".init_array"))) funcptr __stop_init_array[0];
__attribute__((section(".fini_array"))) funcptr __stop_fini_array[0];

} // namespace crt::detail
