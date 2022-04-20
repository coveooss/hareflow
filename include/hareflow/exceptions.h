#pragma once

#include <stdexcept>
#include <string>

#include "hareflow/types.h"

namespace hareflow {

class StreamException : public std::runtime_error
{
public:
    StreamException(const std::string& what) : std::runtime_error(what)
    {
    }
    StreamException(const char* what) : std::runtime_error(what)
    {
    }
    StreamException(const StreamException&) = default;
};

class TimeoutException : public StreamException
{
public:
    TimeoutException(const std::string& what) : StreamException(what)
    {
    }
    TimeoutException(const char* what) : StreamException(what)
    {
    }
    TimeoutException(const TimeoutException&) = default;
};

class IOException : public StreamException
{
public:
    IOException(const std::string& what) : StreamException(what)
    {
    }
    IOException(const char* what) : StreamException(what)
    {
    }
    IOException(const IOException&) = default;
};

class ResponseErrorException : public StreamException
{
public:
    HAREFLOW_EXPORT ResponseErrorException(ResponseCode code);
    ResponseErrorException(const ResponseErrorException&) = default;

    ResponseCode code() const
    {
        return m_code;
    }

private:
    ResponseCode m_code;
};

class StreamDoesNotExistException : public ResponseErrorException
{
public:
    StreamDoesNotExistException() : ResponseErrorException(ResponseCode::StreamDoesNotExist)
    {
    }
    StreamDoesNotExistException(const StreamDoesNotExistException&) = default;
};

class InvalidInputException : public StreamException
{
public:
    InvalidInputException(const std::string& what) : StreamException(what)
    {
    }
    InvalidInputException(const char* what) : StreamException(what)
    {
    }
    InvalidInputException(const InvalidInputException&) = default;
};

class ProducerException : public StreamException
{
public:
    ProducerException(ProducerErrorCode code, const std::string& what) : StreamException(what), m_code(code)
    {
    }
    ProducerException(ProducerErrorCode code, const char* what) : StreamException(what), m_code(code)
    {
    }
    ProducerException(const ProducerException&) = default;

    ProducerErrorCode code() const
    {
        return m_code;
    }

private:
    ProducerErrorCode m_code;
};

}  // namespace hareflow