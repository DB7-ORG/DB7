#include "shared/config/config.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace db7 {

namespace {

// ---------------------------------------------------------------------------
// Value parsers, one per supported type
// ---------------------------------------------------------------------------

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) ==
                  std::tolower(static_cast<unsigned char>(y));
         });
}

bool ParseValue(std::string_view s, bool &out) {
  if (EqualsIgnoreCase(s, "true")) {
    out = true;
    return true;
  }
  if (EqualsIgnoreCase(s, "false")) {
    out = false;
    return true;
  }
  return false;
}

template <typename T>
std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool>
ParseValue(std::string_view s, T &out) {
  const char *end = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(s.data(), end, out); // rejects overflow and '-' for unsigned
  return ec == std::errc() && ptr == end;
}

[[maybe_unused]] bool ParseValue(std::string_view s, double &out) {
  std::string tmp(s);
  char *end = nullptr;
  errno = 0;
  double v = std::strtod(tmp.c_str(), &end);
  if (end != tmp.c_str() + tmp.size() || errno == ERANGE || !std::isfinite(v)) return false;
  out = v;
  return true;
}

[[maybe_unused]] bool ParseValue(std::string_view s, std::string &out) {
  out.assign(s);
  return true;
}

template <typename T>
std::string Expected() {
  if constexpr (std::is_same_v<T, bool>)
    return "true or false";
  else if constexpr (std::is_same_v<T, double>)
    return "a number";
  else if constexpr (std::is_same_v<T, std::string>)
    return "a string";
  else if constexpr (std::is_signed_v<T>)
    return "an integer";
  else
    return "a non-negative integer up to " + std::to_string(std::numeric_limits<T>::max());
}

template <typename T>
std::string ParseInto(std::string_view text, ConfigValue &slot) {
  T value{};
  if (!ParseValue(text, value)) return "expected " + Expected<T>();
  slot = std::move(value);
  return {};
}

// ---------------------------------------------------------------------------
// Option table, generated from DB7_CONFIG_OPTIONS
// ---------------------------------------------------------------------------

struct OptionInfo {
  std::string_view name;
  std::string_view description;
  std::string (*parse)(std::string_view text, ConfigValue &slot); // "" on success, else error
};

constexpr OptionInfo kOptions[] = {
#define DB7_X(key, type, def, desc) {#key, desc, &ParseInto<type>},
    DB7_CONFIG_OPTIONS(DB7_X)
#undef DB7_X
};
static_assert(std::size(kOptions) == kConfigOptionCount);

std::array<ConfigValue, kConfigOptionCount> Defaults() {
  return {
#define DB7_X(key, type, def, desc) ConfigValue(static_cast<type>(def)),
      DB7_CONFIG_OPTIONS(DB7_X)
#undef DB7_X
  };
}

int FindOption(std::string_view name) {
  for (size_t i = 0; i < kConfigOptionCount; i++) {
    if (kOptions[i].name == name) return int(i);
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Line parsing helpers
// ---------------------------------------------------------------------------

bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string_view Trim(std::string_view s) {
  while (!s.empty() && IsSpace(s.front())) s.remove_prefix(1);
  while (!s.empty() && IsSpace(s.back())) s.remove_suffix(1);
  return s;
}

std::string JoinErrors(const std::vector<std::string> &errors) {
  std::string out = "invalid configuration:";
  for (const auto &e : errors) {
    out += "\n  ";
    out += e;
  }
  return out;
}

using Values_t = std::array<ConfigValue, kConfigOptionCount>;

std::string ReadFileText(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw ConfigError({path + ": cannot open file"});
  std::ostringstream buf;
  buf << in.rdbuf();
  if (in.bad()) throw ConfigError({path + ": read error"});
  return buf.str();
}

// Parses `text` into `values`. All-or-nothing: on any error, `values` is untouched.
void ApplyText(std::string_view text, std::string_view source, Values_t &values);

// Defaults, overridden by the startup config file if there is one.
Values_t LoadAtStartup() {
  Values_t values = Defaults();
  const char *env = std::getenv(kConfigPathEnv);
  const bool explicit_path = env != nullptr && *env != '\0';
  const std::string path = explicit_path ? env : kDefaultConfigPath;

  if (!explicit_path && !std::ifstream(path).good()) return values; // no file: defaults

  try {
    ApplyText(ReadFileText(path), path, values);
  } catch (const ConfigError &e) {
    std::fprintf(stderr, "db7: %s\n", e.what());
    std::exit(EXIT_FAILURE);
  }
  return values;
}

// Loads the config before main() runs. Anything that reads the config even
// earlier (another global's constructor) triggers the same load through Values().
[[maybe_unused]] const bool kLoadedAtStartup = (DbConfig::EnsureLoaded(), true);

} // namespace

ConfigError::ConfigError(const std::vector<std::string> &errors)
    : std::runtime_error(JoinErrors(errors)), errors_(errors) {}

Values_t &DbConfig::Values() {
  static Values_t values = LoadAtStartup(); // runs once, thread-safe
  return values;
}

void DbConfig::EnsureLoaded() { (void)Values(); }

void DbConfig::Reset() { Values() = Defaults(); }

std::string_view DbConfig::Name(ConfigOption option) {
  return kOptions[static_cast<size_t>(option)].name;
}

std::string_view DbConfig::Description(ConfigOption option) {
  return kOptions[static_cast<size_t>(option)].description;
}

void DbConfig::LoadFile(const std::string &path) { LoadString(ReadFileText(path), path); }

void DbConfig::LoadString(std::string_view text, std::string_view source) {
  ApplyText(text, source, Values());
}

namespace {

void ApplyText(std::string_view text, std::string_view source, Values_t &values) {
  Values_t updated = values; // apply to a copy so a bad file changes nothing
  std::vector<std::string> errors;
  std::array<bool, kConfigOptionCount> seen{};

  size_t line_no = 0;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(pos, end - pos);
    if (auto comment = line.find('#'); comment != std::string_view::npos) {
      line = line.substr(0, comment);
    }
    line = Trim(line);
    pos = end + 1;
    line_no++;

    if (line.empty() || line.front() == '#') continue;

    auto where = [&] { return std::string(source) + ":" + std::to_string(line_no) + ": "; };

    size_t eq = line.find('=');
    if (eq == std::string_view::npos) {
      errors.push_back(where() + "expected KEY = VALUE");
      continue;
    }
    std::string_view key = Trim(line.substr(0, eq));
    std::string_view value = Trim(line.substr(eq + 1));

    if (key.empty()) {
      errors.push_back(where() + "missing option name");
      continue;
    }
    int idx = FindOption(key);
    if (idx < 0) {
      errors.push_back(where() + "unknown option '" + std::string(key) + "'");
      continue;
    }
    if (seen[idx]) {
      errors.push_back(where() + "option " + std::string(key) + " is set more than once");
      continue;
    }
    seen[idx] = true;
    if (value.empty()) {
      errors.push_back(where() + "missing value for " + std::string(key));
      continue;
    }
    std::string err = kOptions[idx].parse(value, updated[idx]);
    if (!err.empty()) {
      errors.push_back(where() + "invalid value '" + std::string(value) + "' for " +
                       std::string(key) + ": " + err);
    }
  }

  if (!errors.empty()) throw ConfigError(errors);
  values = std::move(updated);
}

} // namespace

} // namespace db7