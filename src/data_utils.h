#ifndef H_DATA_UTILS_
#define H_DATA_UTILS_

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

std::string get_full_path (const std::string &relative_path,
                           bool throw_on_error);
bool starts_with (const std::string &str, const std::string &prefix);
bool directory_exists (const std::string &path);
bool create_directory (const std::string &path);

std::vector<int> getRangeVector (int start, int end);
std::vector<std::string> readFileToVector (const std::string &filename);
std::string getEnvVar (std::string const &key);
bool ends_with_char (const std::string &str, char c);
bool starts_with_char (const std::string &str, char ch);

std::string
get_full_path (const std::string &relative_path, bool throw_on_error)
{
    char full_path[PATH_MAX];
#ifdef _WIN32
    if (GetFullPathName (relative_path.c_str (), MAX_PATH, full_path, nullptr)
        == 0)
        {
            if (throw_on_error)
                {
                    throw std::runtime_error (
                        "Error retrieving full path on Windows");
                }
        }
#else
    if (realpath (relative_path.c_str (), full_path) == nullptr)
        {
            if (throw_on_error)
                {
                    throw std::runtime_error (
                        "Error retrieving full path on POSIX");
                }
        }
#endif
    return std::string (full_path);
}

bool
starts_with (const std::string &str, const std::string &prefix)
{
    if (str.size () < prefix.size ())
        {
            return false;
        }
    return std::equal (prefix.begin (), prefix.end (), str.begin ());
}

// Function to check if a directory exists
bool
directory_exists (const std::string &path)
{
#ifdef _WIN32
    DWORD attribs = GetFileAttributes (path.c_str ());
    return (attribs != INVALID_FILE_ATTRIBUTES
            && (attribs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat info;
    return (stat (path.c_str (), &info) == 0 && (info.st_mode & S_IFDIR));
#endif
}

bool
create_directory (const std::string &path)
{
    if (directory_exists (path))
        {
            return true;
        }

#ifdef _WIN32
    return CreateDirectory (path.c_str (), nullptr)
           || GetLastError () == ERROR_ALREADY_EXISTS;
#else
    return mkdir (path.c_str (), 0755) == 0 || errno == EEXIST;
#endif
}

// Function to create directories recursively from a string path
bool
create_directories (const std::string &path)
{
    std::stringstream ss (path);
    std::string segment;
    std::vector<std::string> directories;

#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '/';
#endif

    if (path.empty ())
        {
            return false;
        }

    while (std::getline (ss, segment, sep))
        {
            if (!segment.empty ())
                {
                    directories.push_back (segment);
                }
        }

    std::string current_path;

    if (!directories.empty () && path[0] == sep)
        {
            current_path += sep;
        }

    for (const auto &dir : directories)
        {
            if (!(current_path.length () == 1 && current_path[0] == sep)
                && !current_path.empty ())
                {
                    current_path += sep;
                }
            current_path += dir;

            if (!create_directory (current_path))
                {
                    std::cerr << "Failed to create directory: " << current_path
                              << std::endl;
                    return false;
                }
        }
    return true;
}

std::vector<int>
getRangeVector (int start, int end)
{
    std::vector<int> result;
    for (int i = start; i <= end; ++i)
        {
            result.push_back (i);
        }
    return result;
}

std::vector<std::string>
readFileToVector (const std::string &filename)
{
    std::vector<std::string> lines;
    std::ifstream file (filename);

    if (file.is_open ())
        {
            std::string line;
            while (std::getline (file, line))
                {
                    lines.push_back (line);
                }

            file.close ();
        }
    else
        {
            std::cerr << "Error: Unable to open file " << filename << std::endl;
        }

    return lines;
}

std::string
getEnvVar (std::string const &key)
{
    char *val = getenv (key.c_str ());
    return val == NULL ? std::string ("") : std::string (val);
}

bool
ends_with_char (const std::string &str, char c)
{
    if (str.empty ())
        {
            return false;
        }
    return str.back () == c;
}

bool
starts_with_char (const std::string &str, char ch)
{
    if (str.empty ())
        {
            return false;
        }
    return str[0] == ch;
}

#endif