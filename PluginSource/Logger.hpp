#pragma once

#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_helpers.h>
#include <stdexcept>
#include <string>
#include <vector>

class Logger {
    static void (*Info)(const char *);
    static std::vector<std::string> messages;

public:
    static void setLoggerCallback(void(*t_callback)(const char *));

    static std::string to_string(const wchar_t *str);

    static void flush();

    static void log(std::string t_message);

    static void log(const std::string &t_actionDescription, NVSDK_NGX_Result t_result);

    static void log(const char *t_message, NVSDK_NGX_Logging_Level /*unused*/, NVSDK_NGX_Feature /*unused*/);
};