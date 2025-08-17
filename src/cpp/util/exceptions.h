#ifndef WOVED_UTIL_EXCEPTIONS_H
#define WOVED_UTIL_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace woved::util {

/**
 * @brief Base class for all exceptions thrown by WOVeD.
 */
class WovedException : public std::runtime_error {
public:
    explicit WovedException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown for configuration-related errors.
 */
class ConfigException : public WovedException {
public:
    explicit ConfigException(const std::string& message)
        : WovedException("Configuration Error: " + message) {}
};

/**
 * @brief Exception thrown for I/O errors.
 */
class IOException : public WovedException {
public:
    explicit IOException(const std::string& message)
        : WovedException("I/O Error: " + message) {}
};

/**
 * @brief Exception for invalid arguments passed to a function.
 */
class InvalidArgumentException : public WovedException {
public:
    explicit InvalidArgumentException(const std::string& message)
        : WovedException("Invalid Argument: " + message) {}
};

} // namespace woved::util

#endif // WOVED_UTIL_EXCEPTIONS_H