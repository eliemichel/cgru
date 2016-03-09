#ifndef LOGGER_H
#define LOGGER_H

#include <cstdio>
#include <sstream>

namespace af
{

/**
 * @brief Class used in AF_LOG & co logging macros
 * to automatically append timing, position in file, etc.
 */
class Logger
{
public:
    Logger(const char *func, const char *file, int line, const char *type = "INFO");
    ~Logger();

    inline std::ostream &stream() { return m_ss; }

private:
    /**
     * Reduces /a/b/c/d/e/f.foo into e/f.foo
     * TODO: handle filesystems where separator is not a slash (like, a backslash)
     */
    static const char * shorterFilename(const char *filename);

public:
    static std::stringstream *log_batch;

private:
    std::ostringstream m_ss;  ///< accumulation stream
};

} // namespace af

#define AF_DEBUG af::Logger(__func__, __FILE__, __LINE__, "DEBUG").stream()
#define AF_LOG   af::Logger(__func__, __FILE__, __LINE__, "INFO").stream()
#define AF_WARN  af::Logger(__func__, __FILE__, __LINE__, "WARNING").stream()
#define AF_ERR   af::Logger(__func__, __FILE__, __LINE__, "ERROR").stream()

#define AF_LOGBATCH_BEGIN() { if (NULL != af::Logger::log_batch) delete af::Logger::log_batch; af::Logger::log_batch = new std::stringstream(); }
#define AF_LOGBATCH_PRINT() std::cerr << af::Logger::log_batch->str() << std::flush
#define AF_LOGBATCH_END() { if (NULL != af::Logger::log_batch) delete af::Logger::log_batch; af::Logger::log_batch = NULL; }

#endif // LOGGER_H
