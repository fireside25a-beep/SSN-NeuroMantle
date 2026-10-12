#include "io_internal.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace surreal::detail {
namespace {
[[noreturn]] void sysfail(const std::string& what) {
  throw std::system_error(errno, std::generic_category(), what);
}
void close_checked(int fd, const std::string& what) {
  if (::close(fd) != 0) sysfail(what);
}
void fsync_directory(const std::filesystem::path& dir) {
  const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) sysfail("open directory for fsync: " + dir.string());
  if (::fsync(fd) != 0) { const int e=errno; ::close(fd); errno=e; sysfail("fsync directory: " + dir.string()); }
  close_checked(fd, "close directory: " + dir.string());
}
}

bool regular_file_nosymlink(const std::filesystem::path& path) {
  std::error_code ec;
  const auto st = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_regular_file(st) && !std::filesystem::is_symlink(st);
}

std::string read_file_bounded(const std::filesystem::path& path, std::size_t max_bytes) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) sysfail("open: " + path.string());
  std::string out;
  out.reserve(std::min<std::size_t>(max_bytes, 1U << 20));
  char buf[16384];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n < 0) { if (errno == EINTR) continue; const int e=errno; ::close(fd); errno=e; sysfail("read: " + path.string()); }
    if (n == 0) break;
    const auto got = static_cast<std::size_t>(n);
    if (out.size() > max_bytes || got > max_bytes - out.size()) { ::close(fd); throw std::runtime_error("file exceeds configured size bound: " + path.string()); }
    out.append(buf, got);
  }
  close_checked(fd, "close: " + path.string());
  return out;
}

void atomic_write_file(const std::filesystem::path& path, std::string_view data) {
  auto parent = path.parent_path();
  if (parent.empty()) parent = ".";
  std::string pattern = (parent / (path.filename().string() + ".tmp.XXXXXX")).string();
  int fd = ::mkstemp(pattern.data());
  if (fd < 0) sysfail("mkstemp: " + path.string());
  (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
  bool renamed = false;
  try {
    std::size_t off = 0;
    while (off < data.size()) {
      const ssize_t n = ::write(fd, data.data() + off, data.size() - off);
      if (n < 0) { if (errno == EINTR) continue; sysfail("write: " + path.string()); }
      if (n == 0) throw std::runtime_error("zero-length write: " + path.string());
      off += static_cast<std::size_t>(n);
    }
    if (::fsync(fd) != 0) sysfail("fsync: " + path.string());
    close_checked(fd, "close: " + path.string());
    fd = -1;
    if (::rename(pattern.c_str(), path.c_str()) != 0) sysfail("rename: " + path.string());
    renamed = true;
    fsync_directory(parent);
  } catch (...) {
    if (fd >= 0) ::close(fd);
    if (!renamed) ::unlink(pattern.c_str());
    throw;
  }
}
}
