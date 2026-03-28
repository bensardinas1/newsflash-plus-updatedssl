# Newsflash Plus — Security & Build Modernization Changelog

> **This update was produced with Claude (Anthropic) assistance.**  
> **Platform: Windows only (MSYS2 MinGW-w64). Linux contributions welcome.**

PERSONAL NOTE: I love this software and have used it for many years. I hope to add some functionality it should have. I do this to honor it, not detract. Thanks to Enisoft for a great program that has stood the test of time!

---

## Summary

This fork modernizes the Newsflash Plus codebase from its original state (circa 2019, OpenSSL 1.0.2k, C++11, MSVC/GCC dual-target) to a security-hardened, C++17, MSYS2 MinGW-w64 build using current libraries. All changes are listed below with rationale.

---

## Critical Bug Fix

### `engine/bigfile.cpp` — Inverted `FlushFileBuffers()` Error Logic

The original code had:

```cpp
if (FlushFileBuffers(file))
    throw std::runtime_error("flush failed");
```

`FlushFileBuffers()` returns **TRUE on success**. This meant the application threw an exception every time a flush *succeeded*, and silently ignored actual failures. Fixed to:

```cpp
if (!FlushFileBuffers(file))
    throw std::runtime_error("flush failed");
```

Also fixed an assert that compared a pointer instead of the byte count: `buff > 0` → `bytes > 0`.

---

## Security Fixes

### 1. SSL/TLS Hardening (`engine/sslcontext.cpp`)

| Before | After |
|--------|-------|
| `SSLv23_method()` | `TLS_method()` (OpenSSL 1.1+) with fallback |
| Cipher string `"ALL"` | `"HIGH:!aNULL:!eNULL:!MD5:!RC4:!3DES:!DES:!EXPORT:!PSK:!SRP"` |
| No certificate verification | `SSL_CTX_set_verify(SSL_VERIFY_PEER)` + `SSL_CTX_set_default_verify_paths()` |
| SSLv2/v3/TLS 1.0/1.1 allowed | All disabled via `SSL_CTX_set_options()` |
| `SSL_library_init()` always called | Conditional on OpenSSL < 1.1.0 (auto-init in 1.1+) |
| `SSL_load_error_strings()` always called | Conditional on OpenSSL < 1.1.0 |

### 2. TLS Hostname Verification & SNI (`engine/sslsocket.h`, `engine/sslsocket.cpp`)

- Added `SetHostname(const std::string&)` method
- Enables **SNI** via `SSL_set_tlsext_host_name()`
- Enables **hostname verification** via `X509_VERIFY_PARAM_set1_host()` (OpenSSL 1.0.2+)
- Wired into `engine/connection.cpp` before SSL handshake

### 3. HTTP → HTTPS Migration

| File | Change |
|------|--------|
| `app/telephone.cpp` | `http://` → `https://` for phone-home URL |
| `app/feedback.cpp` | `http://` → `https://` for feedback submission URL |
| `app/omdb.cpp` | `http://` → `https://` for OMDB API URL |
| `newsflash/config.h` | All hardcoded URLs updated to `https://` |

### 4. Keygen Security (`tools/keygen/keygen.cpp`)

- **Removed backdoor**: `if (keycode == BACKDOOR) return true;` — removed entirely
- **Hash upgrade**: `QCryptographicHash::Md5` → `QCryptographicHash::Sha256`
- Created placeholder `tools/keygen/secret.h`

### 5. PHP Backend Security (`www/*.php`)

| File | Fix |
|------|-----|
| `www/database.php` | `mysql_connect()` → PDO with prepared statements |
| `www/common.php` | Removed raw `sql_string()`, added `sanitize_input()` and `pdo_check_spam()` with whitelisted table names |
| `www/callhome.php` | PDO prepared statements, `shell_exec()` → `finfo`, `md5()` → `random_bytes(16)` |
| `www/feedback.php` | PDO prepared statements |
| `www/import.php` | Removed defunct nzbsooti.sx API key |

### 6. Defunct Service Cleanup

- Removed references to nzbsooti.sx (domain expired/defunct)

---

## Build System Changes

### CMakeLists.txt

- **CMake minimum**: 3.9.1
- **C++ standard**: C++17 (was C++11)
- **MinGW support**: Full MSYS2 MinGW-w64 build path added
  - Qt5 prefix auto-detection for MSYS2 vs MSVC
  - System OpenSSL/zlib/Boost on MinGW (no bundled copies)
  - Removed `-rdynamic -ldl` for MinGW
  - Added Windows libraries: `ws2_32 dbghelp crypt32 user32 shell32 gdi32 advapi32 shlwapi`
  - OpenSSL link names: `ssl` / `crypto` for MinGW (was `ssleay32` / `libeay32`)
  - Dynamic Boost linking for MSYS2

### `newsflash/config.h`

- `_WIN32_WINNT` raised from `0x0501` (XP) to `0x0600` (Vista) — guarded with `#ifndef`
- **Critical fix**: MinGW (GCC on Windows) was incorrectly defining `LINUX_OS`. Now detects `_WIN32` / `__MINGW32__` and correctly defines `WINDOWS_OS`.

### Source Compatibility Fixes

| File | Fix |
|------|-----|
| `engine/engine.cpp` | `utf8::decode(file)` → `utf8::decode(file).c_str()` for `wstring` → `wchar_t*` in `fstream` constructors |
| `engine/session.cpp` | Conditional `#include <zlib.h>` vs `<zlib/zlib.h>` for system vs bundled zlib |
| `app/version.cpp` | Same zlib include fix |
| `engine/unit_test/unit_test_bigfile.cpp` | Explicit `(char)` casts for narrowing conversion (GCC 15 strict) |
| `engine/unit_test/unit_test_sslsocket.cpp` | Updated to modern OpenSSL APIs with version guards |

---

## Library Versions (MSYS2 MinGW-w64)

| Library | Old Version | Current Version |
|---------|-------------|-----------------|
| OpenSSL | 1.0.2k (bundled, EOL) | 3.6.1 (MSYS2 system) |
| Qt | 5.12.6 | 5.15.18 (MSYS2) |
| Boost | 1.72.0 | 1.90.0 (MSYS2) |
| zlib | 1.2.11 (bundled) | System (MSYS2) |
| GCC | Various | 15.2.0 (MSYS2 MinGW-w64) |
| CMake | 3.x | 4.3.0 |

---

## Caveats & Known Limitations

### Windows Only

This fork has been built and tested exclusively on **Windows** using the **MSYS2 MinGW-w64** toolchain. The original Linux build path (GCC + system Qt/Boost + bundled OpenSSL) has **not been tested** with these changes and will likely need adjustments.

**Linux contributors are welcome** — the main areas that need attention:
- CMakeLists.txt: The MinGW guards may need equivalent Linux paths updated
- System OpenSSL 3.x includes/linking on Linux
- The bundled OpenSSL 1.0.2k source tree in `third_party/` is no longer used; Linux builds should also link against system OpenSSL

### OpenSSL Source Tree

The `third_party/openssl` directory contains OpenSSL 3.4.1 source (cloned during this update). However, the actual build uses MSYS2's prebuilt OpenSSL 3.6.1 packages. The source tree is retained for reference but is **not compiled from source**.

The original OpenSSL 1.0.2k source was renamed to `openssl_1.0.2k_backup` and should not be used.

### Debug Build

The current build configuration is **Debug**. A Release build has not been tested but should work with the same toolchain.

### PHP Backend

The PHP files in `www/` have been updated to use PDO and prepared statements, but the backend services (ensisoft.com) appear to be defunct. These fixes are included for code quality but are not actively used.

### No XP Support

The minimum Windows version is now **Vista** (`_WIN32_WINNT=0x0600`). Windows XP support has been dropped.

---

## Build Instructions (Windows / MSYS2)

### Prerequisites

Install MSYS2 from https://www.msys2.org/ (default: `C:\msys64`)

Open the **MSYS2 MinGW64** shell and install dependencies:

```bash
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-make \
          mingw-w64-x86_64-openssl \
          mingw-w64-x86_64-qt5-base \
          mingw-w64-x86_64-qt5-activeqt \
          mingw-w64-x86_64-qt5-xmlpatterns \
          mingw-w64-x86_64-boost \
          mingw-w64-x86_64-ca-certificates
```

### Build

From a Windows command prompt with MSYS2 MinGW64 on PATH:

```cmd
set PATH=C:\msys64\mingw64\bin;%PATH%

cd newsflash-plus
mkdir build_d
cd build_d
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

### Run

```cmd
cd build_d
newsflash.exe
```

---

## NZB DTD 1.1 Metadata Support

Added parsing and propagation of NZB 1.1 DTD `<head><meta>` elements. The NZB 1.1 specification defines four metadata types: `password`, `title`, `category`, and `tag`. These are embedded in the NZB XML as:

```xml
<nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">
  <head>
    <meta type="password">secret123</meta>
    <meta type="title">My Download</meta>
    <meta type="category">Movies</meta>
    <meta type="tag">HD</meta>
    <meta type="tag">x264</meta>
  </head>
  <file ...>
    ...
  </file>
</nzb>
```

### Data Flow

The password flows through the full download-to-extraction pipeline:

```
NZB XML → parseNZB() → NZBMetaData → NZBThread → NZBFile
  → Download.password → Engine (stored by path)
  → FilePackInfo.password → ArchiveManager
  → Archive.password → Unrar (-p flag)
```

### Files Changed

| File | Change |
|------|--------|
| `app/nzbparse.h` | Added `NZBMetaData` struct (passwords, title, category, tags); added overloaded `parseNZB()` with metadata parameter |
| `app/nzbparse.cpp` | SAX handler now processes `<head>` and `<meta>` elements; accumulates text content and dispatches by `type` attribute (password/title/category/tag) in `endElement()` |
| `app/nzbthread.h` | Added `NZBMetaData meta_` member; added overloaded `result()` that returns metadata |
| `app/nzbthread.cpp` | Background thread now uses metadata-aware `parseNZB()` overload |
| `app/nzbfile.h` | Added `NZBMetaData mMeta` member and `getMetaData()` accessor |
| `app/nzbfile.cpp` | Retrieves metadata from thread; passes first password to `Download` struct in `downloadSel()` and `downloadAll()` |
| `app/download.h` | Added `QString password` field to `Download` struct |
| `app/engine.h` | Added `std::map<QString, QString> passwords_` to map download paths to archive passwords |
| `app/engine.cpp` | Stores password by absolute path when download starts; attaches password to `FilePackInfo` when batch completes; cleans up map entry after use |
| `app/fileinfo.h` | Added `QString password` field to `FilePackInfo` struct |
| `app/arcman.cpp` | Propagates `pack.password` to all `Archive` objects created for par2 repair and unrar extraction, including across the repair→unpack flow |
| `app/archive.h` | Added `QString password` field to `Archive` struct; included in `clear()` |
| `app/unrar.cpp` | Passes `-p<password>` to unrar when password is set; passes `-p-` (disable password prompting) when no password is available, preventing unrar from hanging on interactive prompt |
| `app/nzbcore.cpp` | Auto-download from watched folders now parses NZB metadata and passes first password to `Download` |

### Security Notes

- **No injection risk**: Passwords are passed to `QProcess` as `QStringList` argv entries, never through a shell. A malicious password string cannot escape into shell interpretation.
- **No XXE risk**: `QXmlSimpleReader` (SAX) does not resolve external entities by default.
- **No path traversal**: Meta field values are never used in filesystem path construction.
- **Non-interactive unrar**: The `-p-` flag is always set when no password is provided, preventing unrar from hanging on a TTY password prompt.

---

## Complete Modified File List

### Security & Functionality
- `engine/sslcontext.cpp` — SSL/TLS hardening
- `engine/sslsocket.h` — Hostname verification API
- `engine/sslsocket.cpp` — SNI + hostname verification implementation
- `engine/socket.h` — Virtual `SetHostname()` base
- `engine/connection.cpp` — Wire hostname before handshake
- `app/telephone.cpp` — HTTPS
- `app/feedback.cpp` — HTTPS
- `app/omdb.cpp` — HTTPS
- `newsflash/config.h` — HTTPS URLs, MinGW detection, Vista minimum
- `tools/keygen/keygen.cpp` — Backdoor removal, SHA-256
- `tools/keygen/secret.h` — New placeholder
- `www/database.php` — PDO
- `www/common.php` — Input sanitization
- `www/callhome.php` — PDO, secure random
- `www/feedback.php` — PDO
- `www/import.php` — Defunct service removal

### Build & Compatibility
- `CMakeLists.txt` — MinGW/MSYS2 support, C++17, system libraries
- `engine/engine.cpp` — wstring fix
- `engine/bigfile.cpp` — Flush bug fix, assert fix
- `engine/session.cpp` — zlib include
- `app/version.cpp` — zlib include
- `engine/unit_test/unit_test_bigfile.cpp` — Narrowing fix
- `engine/unit_test/unit_test_sslsocket.cpp` — Modern OpenSSL APIs

### NZB DTD 1.1 Metadata
- `app/nzbparse.h` — NZBMetaData struct, overloaded parseNZB
- `app/nzbparse.cpp` — SAX parsing of `<head><meta>` elements
- `app/nzbthread.h` — Metadata member and result overload
- `app/nzbthread.cpp` — Metadata-aware parsing
- `app/nzbfile.h` — Metadata storage and accessor
- `app/nzbfile.cpp` — Password propagation to Download
- `app/download.h` — Password field
- `app/engine.h` — Password-by-path map
- `app/engine.cpp` — Password storage and FilePackInfo attachment
- `app/fileinfo.h` — Password in FilePackInfo
- `app/arcman.cpp` — Password propagation to Archive objects
- `app/archive.h` — Password field
- `app/unrar.cpp` — Password flag for unrar extraction
- `app/nzbcore.cpp` — Metadata parsing for auto-download
