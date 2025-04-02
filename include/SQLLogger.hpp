#ifndef CIPHER_SQL_LOGGER_HPP
#define CIPHER_SQL_LOGGER_HPP

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

namespace CipherDB
{

// SQL Logger singleton for capturing and displaying SQL statements
class SQLLogger
{
   public:
    static SQLLogger& getInstance()
    {
        static SQLLogger instance;
        return instance;
    }

    // Enable or disable logging
    void setEnabled(bool enabled)
    {
        std::lock_guard< std::mutex > lock(mutex_);
        enabled_ = enabled;
    }

    // Log a SQL statement before execution - use toString for sqlpp statements
    template < typename T >
    void logStatement(const T& statement, const std::string& operation = "Executing")
    {
        if (!enabled_)
            return;

        std::lock_guard< std::mutex > lock(mutex_);
        std::string sql_str = statementToString(statement);

        std::stringstream ss;
        ss << operation << " SQL: " << sql_str;

        // Also print to stdout for test output
        std::cout << "SQL: " << ss.str() << std::endl;
    }

    // Log a prepared statement with parameter values
    template < typename T >
    void logPrepared(const T& statement, const std::string& paramName, const std::string& paramValue)
    {
        if (!enabled_)
            return;

        std::lock_guard< std::mutex > lock(mutex_);
        std::string sql_str = statementToString(statement);

        std::stringstream ss;
        ss << "Preparing SQL: " << sql_str << " with " << paramName << "='" << paramValue << "'";

        // Also print to stdout for test output
        std::cout << "SQL: " << ss.str() << std::endl;
    }

   private:
    SQLLogger() : enabled_(false) {}

    // Helper to convert sqlpp11 statements to string
    // Helper to convert sqlpp11 statements to string
    template < typename T >
    std::string statementToString(const T& statement)
    {
        // For regular strings, just return them
        if constexpr (std::is_convertible_v< T, std::string >)
        {
            return statement;
        }
        // For sqlpp11 statements, use this approximation
        else
        {
            std::stringstream ss;
            ss << "[SQL Statement of type: " << typeid(T).name() << "]";

            // Add hints about the statement type
            // Use template specialization to check for member function
            getStatementName< T >(ss, statement, 0);

            return ss.str();
        }
    }

    // Primary template - fallback when method doesn't exist
    template < typename T, typename = void >
    struct has_get_statement_name : std::false_type
    {
    };

    // Specialization for types that have _get_statement_name() method
    template < typename T >
    struct has_get_statement_name< T, std::void_t< decltype(std::declval< T >()._get_statement_name()) > >
        : std::true_type
    {
    };

    // Helper function when _get_statement_name exists
    template < typename T >
    typename std::enable_if< has_get_statement_name< T >::value, void >::type getStatementName(std::stringstream& ss,
                                                                                               const T& statement,
                                                                                               int)
    {
        ss << " (" << statement._get_statement_name() << ")";
    }

    // Helper function when _get_statement_name doesn't exist
    template < typename T >
    typename std::enable_if< !has_get_statement_name< T >::value, void >::type getStatementName(std::stringstream&,
                                                                                                const T&,
                                                                                                ...)
    {
        // Do nothing when the method doesn't exist
    }

    mutable std::mutex mutex_;
    bool enabled_;
};

// Wrapper for automatic logging
template < typename T >
T log_sql(const T& statement, const std::string& operation = "Executing")
{
    SQLLogger::getInstance().logStatement(statement, operation);
    return statement;
}

} // namespace CipherDB

#endif // CIPHER_SQL_LOGGER_HPP
