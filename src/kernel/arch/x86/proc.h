#pragma once

/// TODO: doc comment

namespace arch::proc {

extern "C" {

/// Invalidates the current kernel stack and enters userspace.
void enter_userspace();
}

} // namespace arch::proc
