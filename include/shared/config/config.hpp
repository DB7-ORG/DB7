#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace db7 {

// ---------------------------------------------------------------------------
// All config options, one line each:
//
//   X(KEY, type, default, description)
//
// KEY is both the name in the config file (KEY = value) and the name in code
// (DbConfig::Get(KEY)). Supported types: bool, int64_t, uint32_t, uint64_t,
// double, std::string. Adding a line here is all it takes to add an option.
// ---------------------------------------------------------------------------
#define DB7_CONFIG_OPTIONS(X)                                                                      \
  X(TASK_SCHEDULER_PINNED_THREADS, bool, true, "Pin each worker thread to its own CPU")            \
  X(TASK_SCHEDULER_THREAD_COUNT, uint32_t, 0, "Number of threads to execute tasks, 0 = one per CPU")

// The config file loaded automatically at startup: the path in the DB7_CONFIG
// environment variable, otherwise ./db7.conf if it exists, otherwise defaults only.
inline constexpr const char *kConfigPathEnv = "DB7_CONFIG";
inline constexpr const char *kDefaultConfigPath = "db7.conf";

enum class ConfigOption : uint32_t {
#define DB7_X(key, type, def, desc) key,
  DB7_CONFIG_OPTIONS(DB7_X)
#undef DB7_X
};

inline constexpr size_t kConfigOptionCount = 0
#define DB7_X(key, type, def, desc) +1
    DB7_CONFIG_OPTIONS(DB7_X)
#undef DB7_X
    ;

using ConfigValue = std::variant<bool, int64_t, uint32_t, uint64_t, double, std::string>;

namespace detail {
template <typename T, typename V>
struct IsConfigType;
template <typename T, typename... Ts>
struct IsConfigType<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};

template <typename T>
struct Identity {
  using type = T;
};
} // namespace detail

// Typed handle for one option, so that Get(THREADS_PINNED) returns a bool.
template <typename T, ConfigOption O>
struct ConfigKey {
  static_assert(detail::IsConfigType<T, ConfigValue>::value, "unsupported config option type");
  using type = T;
  static constexpr ConfigOption option = O;
};

#define DB7_X(key, type, def, desc) inline constexpr ConfigKey<type, ConfigOption::key> key{};
DB7_CONFIG_OPTIONS(DB7_X)
#undef DB7_X

// Thrown when a config file can't be read or contains invalid lines.
// what() lists every problem found, one per line, as "path:line: message".
class ConfigError : public std::runtime_error {
public:
  explicit ConfigError(const std::vector<std::string> &errors);

  const std::vector<std::string> &Errors() const { return errors_; }

private:
  std::vector<std::string> errors_;
};

// Process-wide configuration. Static only, never instantiated.
//
// Loaded automatically before main() (see kConfigPathEnv / kDefaultConfigPath),
// or earlier if something reads it first. An invalid startup file prints the
// errors and exits the process; a missing ./db7.conf just means defaults.
//
// File format, one option per line:
//
//   # comment
//   THREADS_PINNED = true
//
// Keys are case-sensitive. Blank lines and lines starting with '#' are ignored.
// Each option may appear at most once. Options not in the file keep their value.
//
// Thread safety: load and set options at startup, before other threads read them.
// Get() takes no lock, so a concurrent Set() or Load*() is a data race.
class DbConfig {
public:
  DbConfig() = delete;

  // Applies the file on top of the current values. All-or-nothing: if any line is
  // invalid, nothing changes and ConfigError is thrown.
  static void LoadFile(const std::string &path);

  // Same as LoadFile, for text already in memory. `source` is used in error messages.
  static void LoadString(std::string_view text, std::string_view source = "<string>");

  // Restores every option to its default.
  static void Reset();

  // Loads the startup config if that hasn't happened yet. Runs automatically;
  // calling it yourself is never required.
  static void EnsureLoaded();

  // DbConfig::Get(THREADS_PINNED) -> bool
  template <typename T, ConfigOption O>
  static const T &Get(ConfigKey<T, O>) {
    return std::get<T>(Values()[static_cast<size_t>(O)]);
  }

  // DbConfig::Set(THREADS_PINNED, false)
  template <typename T, ConfigOption O>
  static void Set(ConfigKey<T, O>, typename detail::Identity<T>::type value) {
    Values()[static_cast<size_t>(O)] = std::move(value);
  }

  static std::string_view Name(ConfigOption option);
  static std::string_view Description(ConfigOption option);

private:
  static std::array<ConfigValue, kConfigOptionCount> &Values();
};

} // namespace db7