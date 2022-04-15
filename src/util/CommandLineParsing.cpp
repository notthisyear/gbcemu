#include <algorithm>
#include <iostream>
#include <string>

namespace gbcemu {

const char *get_option(char **start, char **end, const std::string &option) {
    auto it = std::find(start, end, option);
    if (it != end && ++it != end)
        return *it;
    return nullptr;
}

std::string fix_path(const std::string &path) {

    bool remove_quotes = path[0] == '"' && path[path.length() - 1] == '"';
    auto number_of_backspaces = std::count(path.begin(), path.end(), '\\');

    auto new_length = path.length() + number_of_backspaces + (remove_quotes ? -2 : 0);
    auto new_path_string = new char[new_length + 1];

    auto i = 0;
    auto j = 0;

    while (true) {
        if (i == 0 && remove_quotes) {
            ;
        }
        if (i == path.length() - 1 && remove_quotes) {
            break;
        } else if (i == path.length()) {
            break;
        } else {

            new_path_string[j] = path[i];
            if (number_of_backspaces > 0) {
                if (path[i] == '\\')
                    new_path_string[++j] = '\\';
            }
        }
        i++;
        j++;
    }

    new_path_string[j] = '\0';
    auto s = std::string(new_path_string);
    delete[] new_path_string;
    return s;
}
}