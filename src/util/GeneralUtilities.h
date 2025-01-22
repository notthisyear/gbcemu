#pragma once

#include <memory>
#include <stdexcept>
#include <string>

namespace gbcemu {

class GeneralUtilities final {

  public:
    template <typename... Args> inline static std::string formatted_string(std::string const &format, Args... args) {
        return formatted_string_impl(format, convert(std::forward<Args>(args))...);
    }

    static std::string get_folder_name_from_full_path(std::string const &full_path) {
        std::size_t const last_position{ full_path.find_last_of("/\\") };
        return (last_position != std::string::npos) ? full_path.substr(0, last_position) : full_path;
    }

  private:
    template <typename T> inline static auto convert(T &&t) {
        if constexpr (std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value) {
            return std::forward<T>(t).c_str();
        } else {
            return std::forward<T>(t);
        }
    }

    // Based on
    // https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    template <typename... Args> inline static std::string formatted_string_impl(std::string const &format, Args... args) {
        // Figure out the length of the result (adding one for null-termination)
        int const string_length{ std::snprintf(nullptr, 0, format.c_str(), args...) + 1 };

        if (string_length <= 0) {
            throw std::runtime_error("snprintf failed");
        }
        std::size_t const size{ static_cast<size_t>(string_length) };
        std::unique_ptr<char[]> const buffer = std::make_unique<char[]>(size);

        std::snprintf(buffer.get(), size, format.c_str(), args...);

        return std::string(buffer.get(), buffer.get() + size - 1);
    }
};
}