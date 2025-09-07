///
/// \file 				Exception.hpp
/// \author 			Geoffrey Hunter (www.mbedded.ninja) <gbmhunter@gmail.com>
/// \edited             n/a
/// \created			2017-11-09
/// \last-modified		2017-11-27
/// \brief 				Contains the Exception class. File originally from https://github.com/mbedded-ninja/CppUtils.
/// \details
///		See README.md in root dir for more info.

#ifndef SERIAL_EXCEPTION_H_
#define SERIAL_EXCEPTION_H_

// System includes
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Serial {

    class Exception : public std::runtime_error {

    public:
        Exception(const char *file, int line, const std::string &arg) :
                std::runtime_error(arg) {
            msg_ = std::string(file) + ":" + std::to_string(line) + ": " + arg;
        }

        ~Exception() throw() {}

        const char *what() const throw() override {
            return msg_.c_str();
        }

    private:
        std::string msg_;
    };

} // namespace Serial

#define THROW_EXCEPT(arg) throw Exception(__FILE__, __LINE__, arg);


#endif // SERIAL_EXCEPTION_H_