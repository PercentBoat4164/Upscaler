
#include "Logger.hpp"

void (*Logger::Info)(const char *){nullptr};
std::vector<std::string> Logger::messages{};

void Logger::setLoggerCallback(void (*t_callback)(const char *)) {
    Info = t_callback;
}

std::string Logger::to_string(const wchar_t *str) {
    std::wstring temp = str;
    return {temp.begin(), temp.end()};
}

void Logger::flush() {
    for (const std::string &message : messages) Info(message.c_str());
    messages.clear();
}

void Logger::log(std::string t_message) {
    if (t_message.back() == '\n') t_message.erase(t_message.length() - 1, 1);
    if (Info == nullptr) messages.push_back(t_message);
    else Info(t_message.c_str());
}

void Logger::log(const std::string &t_actionDescription, NVSDK_NGX_Result t_result) {
    if (NVSDK_NGX_SUCCEED(t_result)) return log("DLSSPlugin: " + t_actionDescription + ": Succeeded");
    log("DLSSPlugin: " + t_actionDescription + ": Failed | " + to_string(GetNGXResultAsString(t_result)) + ".");
}

void Logger::log(const char *t_message, NVSDK_NGX_Logging_Level /*unused*/, NVSDK_NGX_Feature /*unused*/) {
    log(t_message);
}
