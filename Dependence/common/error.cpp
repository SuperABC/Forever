#include "error.h"


using namespace std;

ExceptionBase::ExceptionBase(const string& message,
    const string& file,
    int line,
    const string& function) noexcept
    : m_message(message)
    , m_file(file)
    , m_line(line)
    , m_function(function) {
    formatWhat();
}

const char* ExceptionBase::what() const noexcept {
    return m_what.data();
}

string ExceptionBase::GetDetailedInfo() const {
    ostringstream oss;
    oss << "Exception: " << m_message << "\n"
        << "File: " << m_file << "\n"
        << "Line: " << m_line << "\n"
        << "Function: " << (m_function.empty() ? "Unknown" : m_function);
    return oss.str();
}

void ExceptionBase::formatWhat() {
    m_what = m_message + " [at " + m_file + ":" + to_string(m_line) + "]";
    if (!m_function.empty()) {
        m_what += " in " + m_function;
    }
}