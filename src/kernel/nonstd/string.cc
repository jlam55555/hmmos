#include "nonstd/string.h"

#include "mm/kmalloc.h"
#include "nonstd/string_view.h"
#include "util/assert.h"
#include <utility>

namespace nonstd {

string::string(size_t count, char ch)
    : len{count}, _capacity{len}, _data{Allocator{}.allocate(_capacity + 1)} {
  memset(_data, ch, len);
  _data[len] = '\0';
}

string::string(const char *s) : string{s, strlen(s)} {}

string::string(const char *s, size_t count)
    : len{count}, _capacity{len}, _data{Allocator{}.allocate(_capacity + 1)} {
  memcpy(_data, s, len);
  _data[len] = '\0';
}

string::string(string_view sv) : string{sv.data(), sv.length()} {}

string::string(const string &s) : string{s._data, s.len} {}

string::string(string &&s) noexcept { *this = std::move(s); }

string::~string() {
  if (_capacity != 0) {
    Allocator{}.deallocate(_data, _capacity + 1);
  }
}

string &string::operator=(const string &s) {
  *this = string(s);
  return *this;
}

string &string::operator=(string &&s) noexcept {
  std::swap(len, s.len);
  std::swap(_capacity, s._capacity);
  std::swap(_data, s._data);
  return *this;
}

string &string::operator=(const char *s) {
  *this = string(s);
  return *this;
}

string &string::operator=(string_view s) {
  *this = string(s);
  return *this;
}

char &string::at(size_t pos) {
  ASSERT(pos < size());
  return _data[pos];
}

const char &string::at(size_t pos) const {
  ASSERT(pos < size());
  return _data[pos];
}

string::operator string_view() const { return {_data, len}; }

void string::reserve(size_t new_capacity) {
  if (new_capacity <= _capacity) {
    // No effect.
    return;
  }
  // We need to resize -- resize to at least double the current
  // capacity for O(1) amortized insertion.
  new_capacity = std::max(new_capacity, _capacity * 2);
  char *const old_data = _data;
  _data = Allocator{}.allocate(new_capacity + 1);
  memcpy(_data, old_data, len);
  _data[len] = '\0';
  if (_capacity != 0) {
    Allocator{}.deallocate(old_data, _capacity + 1);
  }
  _capacity = new_capacity;
}

void string::push_back(char c) {
  reserve(len + 1);
  _data[len] = c;
  _data[++len] = '\0';
}

string &string::append(size_t count, char c) {
  resize(len + count); // note: modifies len
  memset(_data + len - count, c, count);
  return *this;
}
string &string::append(string_view sv) {
  resize(len + sv.length()); // note: modifies len
  memmove(_data + len - sv.length(), sv.data(), sv.length());
  return *this;
}
string &string::append(std::initializer_list<char> ilist) {
  return append(ilist.begin(), ilist.end());
}

void string::resize(size_t count) {
  reserve(count);
  if (count > len) {
    memset(_data + len, '\0', count - len);
  }
  len = count;
  _data[len] = '\0';
}

void string::swap(string &s) {
  std::swap(len, s.len);
  std::swap(_capacity, s._capacity);
  std::swap(_data, s._data);
}

string operator+(const string &lhs, const string &rhs) {
  string rv = lhs;
  rv.append(rhs);
  return rv;
}
string operator+(const string &lhs, const char *rhs) {
  string rv = lhs;
  rv.append(rhs);
  return rv;
}
string operator+(const string &lhs, char rhs) {
  string rv = lhs;
  rv.push_back(rhs);
  return rv;
}
string operator+(const string &lhs, string_view rhs) {
  string rv = lhs;
  rv.append(rhs);
  return rv;
}
string operator+(const char *lhs, const string &rhs) {
  string rv = lhs;
  rv.append(rhs);
  return rv;
}
string operator+(char lhs, const string &rhs) {
  string rv = string(1, lhs);
  rv.append(rhs);
  return rv;
}
string operator+(string_view lhs, const string &rhs) {
  string rv{lhs};
  rv.append(rhs);
  return rv;
}

bool operator==(const string &lhs, const string &rhs) {
  return string_view{lhs} == string_view{rhs};
}
bool operator==(const string &lhs, const char *rhs) {
  return string_view{lhs} == string_view{rhs};
}
bool operator==(const char *lhs, const string &rhs) {
  return string_view{lhs} == string_view{rhs};
}

} // namespace nonstd
