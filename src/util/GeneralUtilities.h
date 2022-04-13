#pragma once

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gbcemu {

class GeneralUtilities {

  public:
    template <typename... Args>
    inline static std::string formatted_string(const std::string &format,
                                               Args... args) {
        return formatted_string_impl(format,
                                     convert(std::forward<Args>(args))...);
    }

  private:
    template <typename T> inline static auto convert(T &&t) {
        if constexpr (std::is_same<std::remove_cv_t<std::remove_reference_t<T>>,
                                   std::string>::value)
            return std::forward<T>(t).c_str();
        else
            return std::forward<T>(t);
    }

    // Based on
    // https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    template <typename... Args>
    inline static std::string formatted_string_impl(const std::string &format,
                                                    Args... args) {
        // Figure out the length of the result (adding one for null-termination)
        int string_length =
            std::snprintf(nullptr, 0, format.c_str(), args...) + 1;

        if (string_length <= 0)
            throw std::runtime_error("snprintf failed");

        size_t size = static_cast<size_t>(string_length);
        auto buffer = std::make_unique<char[]>(size);

        std::snprintf(buffer.get(), size, format.c_str(), args...);

        return std::string(buffer.get(), buffer.get() + size - 1);
    }
};
}