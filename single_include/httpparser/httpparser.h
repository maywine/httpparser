#pragma once
#include <algorithm>
#include <assert.h>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace httpparser
{
struct Request
{
    Request() : versionMajor(0), versionMinor(0), keepAlive(false) {}

    struct HeaderItem
    {
        std::string name;
        std::string value;
    };

    std::string method;
    std::string uri;
    int versionMajor;
    int versionMinor;
    std::vector<HeaderItem> headers;
    std::vector<char> content;
    bool keepAlive;

    std::string inspect() const
    {
        std::stringstream stream;
        stream << method << " " << uri << " HTTP/" << versionMajor << "." << versionMinor << "\n";

        for (std::vector<Request::HeaderItem>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        {
            stream << it->name << ": " << it->value << "\n";
        }

        std::string data(content.begin(), content.end());
        stream << data << "\n";
        stream << "+ keep-alive: " << keepAlive << "\n";
        ;
        return stream.str();
    }

    struct Response
    {
        Response() : versionMajor(0), versionMinor(0), keepAlive(false), statusCode(0) {}

        struct HeaderItem
        {
            std::string name;
            std::string value;
        };

        int versionMajor;
        int versionMinor;
        std::vector<HeaderItem> headers;
        std::vector<char> content;
        bool keepAlive;

        unsigned int statusCode;
        std::string status;

        std::string inspect() const
        {
            std::stringstream stream;
            stream << "HTTP/" << versionMajor << "." << versionMinor << " " << statusCode << " " << status << "\n";

            for (std::vector<Response::HeaderItem>::const_iterator it = headers.begin(); it != headers.end(); ++it)
            {
                stream << it->name << ": " << it->value << "\n";
            }

            std::string data(content.begin(), content.end());
            stream << data << "\n";
            return stream.str();
        }
    };

    class HttpRequestParser
    {
    public:
        HttpRequestParser() : state(RequestMethodStart), contentSize(0), chunkSize(0), chunked(false) {}

        enum ParseResult
        {
            ParsingCompleted,
            ParsingIncompleted,
            ParsingError
        };

        ParseResult parse(Request& req, const char* begin, const char* end) { return consume(req, begin, end); }

    private:
        static bool checkIfConnection(const Request::HeaderItem& item)
        {
            return strcasecmp(item.name.c_str(), "Connection") == 0;
        }

        ParseResult consume(Request& req, const char* begin, const char* end)
        {
            while (begin != end)
            {
                char input = *begin++;

                switch (state)
                {
                case RequestMethodStart:
                    if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        state = RequestMethod;
                        req.method.push_back(input);
                    }
                    break;
                case RequestMethod:
                    if (input == ' ')
                    {
                        state = RequestUriStart;
                    }
                    else if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        req.method.push_back(input);
                    }
                    break;
                case RequestUriStart:
                    if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        state = RequestUri;
                        req.uri.push_back(input);
                    }
                    break;
                case RequestUri:
                    if (input == ' ')
                    {
                        state = RequestHttpVersion_h;
                    }
                    else if (input == '\r')
                    {
                        req.versionMajor = 0;
                        req.versionMinor = 9;

                        return ParsingCompleted;
                    }
                    else if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        req.uri.push_back(input);
                    }
                    break;
                case RequestHttpVersion_h:
                    if (input == 'H')
                    {
                        state = RequestHttpVersion_ht;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_ht:
                    if (input == 'T')
                    {
                        state = RequestHttpVersion_htt;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_htt:
                    if (input == 'T')
                    {
                        state = RequestHttpVersion_http;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_http:
                    if (input == 'P')
                    {
                        state = RequestHttpVersion_slash;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_slash:
                    if (input == '/')
                    {
                        req.versionMajor = 0;
                        req.versionMinor = 0;
                        state            = RequestHttpVersion_majorStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_majorStart:
                    if (isDigit(input))
                    {
                        req.versionMajor = input - '0';
                        state            = RequestHttpVersion_major;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_major:
                    if (input == '.')
                    {
                        state = RequestHttpVersion_minorStart;
                    }
                    else if (isDigit(input))
                    {
                        req.versionMajor = req.versionMajor * 10 + input - '0';
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_minorStart:
                    if (isDigit(input))
                    {
                        req.versionMinor = input - '0';
                        state            = RequestHttpVersion_minor;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case RequestHttpVersion_minor:
                    if (input == '\r')
                    {
                        state = ResponseHttpVersion_newLine;
                    }
                    else if (isDigit(input))
                    {
                        req.versionMinor = req.versionMinor * 10 + input - '0';
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_newLine:
                    if (input == '\n')
                    {
                        state = HeaderLineStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case HeaderLineStart:
                    if (input == '\r')
                    {
                        state = ExpectingNewline_3;
                    }
                    else if (!req.headers.empty() && (input == ' ' || input == '\t'))
                    {
                        state = HeaderLws;
                    }
                    else if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        req.headers.push_back(Request::HeaderItem());
                        req.headers.back().name.reserve(16);
                        req.headers.back().value.reserve(16);
                        req.headers.back().name.push_back(input);
                        state = HeaderName;
                    }
                    break;
                case HeaderLws:
                    if (input == '\r')
                    {
                        state = ExpectingNewline_2;
                    }
                    else if (input == ' ' || input == '\t')
                    {
                    }
                    else if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        state = HeaderValue;
                        req.headers.back().value.push_back(input);
                    }
                    break;
                case HeaderName:
                    if (input == ':')
                    {
                        state = SpaceBeforeHeaderValue;
                    }
                    else if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        req.headers.back().name.push_back(input);
                    }
                    break;
                case SpaceBeforeHeaderValue:
                    if (input == ' ')
                    {
                        state = HeaderValue;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case HeaderValue:
                    if (input == '\r')
                    {
                        if (req.method == "POST" || req.method == "PUT")
                        {
                            Request::HeaderItem& h = req.headers.back();

                            if (strcasecmp(h.name.c_str(), "Content-Length") == 0)
                            {
                                contentSize = atoi(h.value.c_str());
                                req.content.reserve(contentSize);
                            }
                            else if (strcasecmp(h.name.c_str(), "Transfer-Encoding") == 0)
                            {
                                if (strcasecmp(h.value.c_str(), "chunked") == 0)
                                    chunked = true;
                            }
                        }
                        state = ExpectingNewline_2;
                    }
                    else if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        req.headers.back().value.push_back(input);
                    }
                    break;
                case ExpectingNewline_2:
                    if (input == '\n')
                    {
                        state = HeaderLineStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ExpectingNewline_3:
                {
                    std::vector<Request::HeaderItem>::iterator it =
                        std::find_if(req.headers.begin(), req.headers.end(), checkIfConnection);

                    if (it != req.headers.end())
                    {
                        if (strcasecmp(it->value.c_str(), "Keep-Alive") == 0)
                        {
                            req.keepAlive = true;
                        }
                        else  // == Close
                        {
                            req.keepAlive = false;
                        }
                    }
                    else
                    {
                        if (req.versionMajor > 1 || (req.versionMajor == 1 && req.versionMinor == 1))
                            req.keepAlive = true;
                    }

                    if (chunked)
                    {
                        state = ChunkSize;
                    }
                    else if (contentSize == 0)
                    {
                        if (input == '\n')
                            return ParsingCompleted;
                        else
                            return ParsingError;
                    }
                    else
                    {
                        state = Post;
                    }
                    break;
                }
                case Post:
                    --contentSize;
                    req.content.push_back(input);

                    if (contentSize == 0)
                    {
                        return ParsingCompleted;
                    }
                    break;
                case ChunkSize:
                    if (isalnum(input))
                    {
                        chunkSizeStr.push_back(input);
                    }
                    else if (input == ';')
                    {
                        state = ChunkExtensionName;
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkExtensionName:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '=')
                    {
                        state = ChunkExtensionValue;
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkExtensionValue:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine:
                    if (input == '\n')
                    {
                        chunkSize = strtol(chunkSizeStr.c_str(), NULL, 16);
                        chunkSizeStr.clear();
                        req.content.reserve(req.content.size() + chunkSize);

                        if (chunkSize == 0)
                            state = ChunkSizeNewLine_2;
                        else
                            state = ChunkData;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine_2:
                    if (input == '\r')
                    {
                        state = ChunkSizeNewLine_3;
                    }
                    else if (isalpha(input))
                    {
                        state = ChunkTrailerName;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine_3:
                    if (input == '\n')
                    {
                        return ParsingCompleted;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkTrailerName:
                    if (isalnum(input))
                    {
                        // skip
                    }
                    else if (input == ':')
                    {
                        state = ChunkTrailerValue;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkTrailerValue:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkData:
                    req.content.push_back(input);

                    if (--chunkSize == 0)
                    {
                        state = ChunkDataNewLine_1;
                    }
                    break;
                case ChunkDataNewLine_1:
                    if (input == '\r')
                    {
                        state = ChunkDataNewLine_2;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkDataNewLine_2:
                    if (input == '\n')
                    {
                        state = ChunkSize;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                default:
                    return ParsingError;
                }
            }

            return ParsingIncompleted;
        }

        // Check if a byte is an HTTP character.
        inline bool isChar(int c) { return c >= 0 && c <= 127; }

        // Check if a byte is an HTTP control character.
        inline bool isControl(int c) { return (c >= 0 && c <= 31) || (c == 127); }

        // Check if a byte is defined as an HTTP special character.
        inline bool isSpecial(int c)
        {
            switch (c)
            {
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ',':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '[':
            case ']':
            case '?':
            case '=':
            case '{':
            case '}':
            case ' ':
            case '\t':
                return true;
            default:
                return false;
            }
        }

        // Check if a byte is a digit.
        inline bool isDigit(int c) { return c >= '0' && c <= '9'; }

        // The current state of the parser.
        enum State
        {
            RequestMethodStart,
            RequestMethod,
            RequestUriStart,
            RequestUri,
            RequestHttpVersion_h,
            RequestHttpVersion_ht,
            RequestHttpVersion_htt,
            RequestHttpVersion_http,
            RequestHttpVersion_slash,
            RequestHttpVersion_majorStart,
            RequestHttpVersion_major,
            RequestHttpVersion_minorStart,
            RequestHttpVersion_minor,

            ResponseStatusStart,
            ResponseHttpVersion_ht,
            ResponseHttpVersion_htt,
            ResponseHttpVersion_http,
            ResponseHttpVersion_slash,
            ResponseHttpVersion_majorStart,
            ResponseHttpVersion_major,
            ResponseHttpVersion_minorStart,
            ResponseHttpVersion_minor,
            ResponseHttpVersion_spaceAfterVersion,
            ResponseHttpVersion_statusCodeStart,
            ResponseHttpVersion_spaceAfterStatusCode,
            ResponseHttpVersion_statusTextStart,
            ResponseHttpVersion_newLine,

            HeaderLineStart,
            HeaderLws,
            HeaderName,
            SpaceBeforeHeaderValue,
            HeaderValue,
            ExpectingNewline_2,
            ExpectingNewline_3,

            Post,
            ChunkSize,
            ChunkExtensionName,
            ChunkExtensionValue,
            ChunkSizeNewLine,
            ChunkSizeNewLine_2,
            ChunkSizeNewLine_3,
            ChunkTrailerName,
            ChunkTrailerValue,

            ChunkDataNewLine_1,
            ChunkDataNewLine_2,
            ChunkData,
        } state;

        size_t contentSize;
        std::string chunkSizeStr;
        size_t chunkSize;
        bool chunked;
    };

    class HttpResponseParser
    {
    public:
        HttpResponseParser() : state(ResponseStatusStart), contentSize(0), chunkSize(0), chunked(false) {}

        enum ParseResult
        {
            ParsingCompleted,
            ParsingIncompleted,
            ParsingError
        };

        ParseResult parse(Response& resp, const char* begin, const char* end) { return consume(resp, begin, end); }

    private:
        static bool checkIfConnection(const Response::HeaderItem& item)
        {
            return strcasecmp(item.name.c_str(), "Connection") == 0;
        }

        ParseResult consume(Response& resp, const char* begin, const char* end)
        {
            while (begin != end)
            {
                char input = *begin++;

                switch (state)
                {
                case ResponseStatusStart:
                    if (input != 'H')
                    {
                        return ParsingError;
                    }
                    else
                    {
                        state = ResponseHttpVersion_ht;
                    }
                    break;
                case ResponseHttpVersion_ht:
                    if (input == 'T')
                    {
                        state = ResponseHttpVersion_htt;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_htt:
                    if (input == 'T')
                    {
                        state = ResponseHttpVersion_http;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_http:
                    if (input == 'P')
                    {
                        state = ResponseHttpVersion_slash;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_slash:
                    if (input == '/')
                    {
                        resp.versionMajor = 0;
                        resp.versionMinor = 0;
                        state             = ResponseHttpVersion_majorStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_majorStart:
                    if (isDigit(input))
                    {
                        resp.versionMajor = input - '0';
                        state             = ResponseHttpVersion_major;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_major:
                    if (input == '.')
                    {
                        state = ResponseHttpVersion_minorStart;
                    }
                    else if (isDigit(input))
                    {
                        resp.versionMajor = resp.versionMajor * 10 + input - '0';
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_minorStart:
                    if (isDigit(input))
                    {
                        resp.versionMinor = input - '0';
                        state             = ResponseHttpVersion_minor;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_minor:
                    if (input == ' ')
                    {
                        state           = ResponseHttpVersion_statusCodeStart;
                        resp.statusCode = 0;
                    }
                    else if (isDigit(input))
                    {
                        resp.versionMinor = resp.versionMinor * 10 + input - '0';
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_statusCodeStart:
                    if (isDigit(input))
                    {
                        resp.statusCode = input - '0';
                        state           = ResponseHttpVersion_statusCode;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_statusCode:
                    if (isDigit(input))
                    {
                        resp.statusCode = resp.statusCode * 10 + input - '0';
                    }
                    else
                    {
                        if (resp.statusCode < 100 || resp.statusCode > 999)
                        {
                            return ParsingError;
                        }
                        else if (input == ' ')
                        {
                            state = ResponseHttpVersion_statusTextStart;
                        }
                        else
                        {
                            return ParsingError;
                        }
                    }
                    break;
                case ResponseHttpVersion_statusTextStart:
                    if (isChar(input))
                    {
                        resp.status += input;
                        state = ResponseHttpVersion_statusText;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_statusText:
                    if (input == '\r')
                    {
                        state = ResponseHttpVersion_newLine;
                    }
                    else if (isChar(input))
                    {
                        resp.status += input;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ResponseHttpVersion_newLine:
                    if (input == '\n')
                    {
                        state = HeaderLineStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case HeaderLineStart:
                    if (input == '\r')
                    {
                        state = ExpectingNewline_3;
                    }
                    else if (!resp.headers.empty() && (input == ' ' || input == '\t'))
                    {
                        state = HeaderLws;
                    }
                    else if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        resp.headers.push_back(Response::HeaderItem());
                        resp.headers.back().name.reserve(16);
                        resp.headers.back().value.reserve(16);
                        resp.headers.back().name.push_back(input);
                        state = HeaderName;
                    }
                    break;
                case HeaderLws:
                    if (input == '\r')
                    {
                        state = ExpectingNewline_2;
                    }
                    else if (input == ' ' || input == '\t')
                    {
                    }
                    else if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        state = HeaderValue;
                        resp.headers.back().value.push_back(input);
                    }
                    break;
                case HeaderName:
                    if (input == ':')
                    {
                        state = SpaceBeforeHeaderValue;
                    }
                    else if (!isChar(input) || isControl(input) || isSpecial(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        resp.headers.back().name.push_back(input);
                    }
                    break;
                case SpaceBeforeHeaderValue:
                    if (input == ' ')
                    {
                        state = HeaderValue;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case HeaderValue:
                    if (input == '\r')
                    {
                        Response::HeaderItem& h = resp.headers.back();

                        if (strcasecmp(h.name.c_str(), "Content-Length") == 0)
                        {
                            contentSize = atoi(h.value.c_str());
                            resp.content.reserve(contentSize);
                        }
                        else if (strcasecmp(h.name.c_str(), "Transfer-Encoding") == 0)
                        {
                            if (strcasecmp(h.value.c_str(), "chunked") == 0)
                                chunked = true;
                        }
                        state = ExpectingNewline_2;
                    }
                    else if (isControl(input))
                    {
                        return ParsingError;
                    }
                    else
                    {
                        resp.headers.back().value.push_back(input);
                    }
                    break;
                case ExpectingNewline_2:
                    if (input == '\n')
                    {
                        state = HeaderLineStart;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ExpectingNewline_3:
                {
                    std::vector<Response::HeaderItem>::iterator it =
                        std::find_if(resp.headers.begin(), resp.headers.end(), checkIfConnection);

                    if (it != resp.headers.end())
                    {
                        if (strcasecmp(it->value.c_str(), "Keep-Alive") == 0)
                        {
                            resp.keepAlive = true;
                        }
                        else  // == Close
                        {
                            resp.keepAlive = false;
                        }
                    }
                    else
                    {
                        if (resp.versionMajor > 1 || (resp.versionMajor == 1 && resp.versionMinor == 1))
                            resp.keepAlive = true;
                    }

                    if (chunked)
                    {
                        state = ChunkSize;
                    }
                    else if (contentSize == 0)
                    {
                        if (input == '\n')
                            return ParsingCompleted;
                        else
                            return ParsingError;
                    }

                    else
                    {
                        state = Post;
                    }
                    break;
                }
                case Post:
                    --contentSize;
                    resp.content.push_back(input);

                    if (contentSize == 0)
                    {
                        return ParsingCompleted;
                    }
                    break;
                case ChunkSize:
                    if (isalnum(input))
                    {
                        chunkSizeStr.push_back(input);
                    }
                    else if (input == ';')
                    {
                        state = ChunkExtensionName;
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkExtensionName:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '=')
                    {
                        state = ChunkExtensionValue;
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkExtensionValue:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine:
                    if (input == '\n')
                    {
                        chunkSize = strtol(chunkSizeStr.c_str(), NULL, 16);
                        chunkSizeStr.clear();
                        resp.content.reserve(resp.content.size() + chunkSize);

                        if (chunkSize == 0)
                            state = ChunkSizeNewLine_2;
                        else
                            state = ChunkData;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine_2:
                    if (input == '\r')
                    {
                        state = ChunkSizeNewLine_3;
                    }
                    else if (isalpha(input))
                    {
                        state = ChunkTrailerName;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkSizeNewLine_3:
                    if (input == '\n')
                    {
                        return ParsingCompleted;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkTrailerName:
                    if (isalnum(input))
                    {
                        // skip
                    }
                    else if (input == ':')
                    {
                        state = ChunkTrailerValue;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkTrailerValue:
                    if (isalnum(input) || input == ' ')
                    {
                        // skip
                    }
                    else if (input == '\r')
                    {
                        state = ChunkSizeNewLine;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkData:
                    resp.content.push_back(input);

                    if (--chunkSize == 0)
                    {
                        state = ChunkDataNewLine_1;
                    }
                    break;
                case ChunkDataNewLine_1:
                    if (input == '\r')
                    {
                        state = ChunkDataNewLine_2;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                case ChunkDataNewLine_2:
                    if (input == '\n')
                    {
                        state = ChunkSize;
                    }
                    else
                    {
                        return ParsingError;
                    }
                    break;
                default:
                    return ParsingError;
                }
            }

            return ParsingIncompleted;
        }

        // Check if a byte is an HTTP character.
        inline bool isChar(int c) { return c >= 0 && c <= 127; }

        // Check if a byte is an HTTP control character.
        inline bool isControl(int c) { return (c >= 0 && c <= 31) || (c == 127); }

        // Check if a byte is defined as an HTTP special character.
        inline bool isSpecial(int c)
        {
            switch (c)
            {
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ',':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '[':
            case ']':
            case '?':
            case '=':
            case '{':
            case '}':
            case ' ':
            case '\t':
                return true;
            default:
                return false;
            }
        }

        // Check if a byte is a digit.
        inline bool isDigit(int c) { return c >= '0' && c <= '9'; }

        // The current state of the parser.
        enum State
        {
            ResponseStatusStart,
            ResponseHttpVersion_ht,
            ResponseHttpVersion_htt,
            ResponseHttpVersion_http,
            ResponseHttpVersion_slash,
            ResponseHttpVersion_majorStart,
            ResponseHttpVersion_major,
            ResponseHttpVersion_minorStart,
            ResponseHttpVersion_minor,
            ResponseHttpVersion_statusCodeStart,
            ResponseHttpVersion_statusCode,
            ResponseHttpVersion_statusTextStart,
            ResponseHttpVersion_statusText,
            ResponseHttpVersion_newLine,
            HeaderLineStart,
            HeaderLws,
            HeaderName,
            SpaceBeforeHeaderValue,
            HeaderValue,
            ExpectingNewline_2,
            ExpectingNewline_3,
            Post,
            ChunkSize,
            ChunkExtensionName,
            ChunkExtensionValue,
            ChunkSizeNewLine,
            ChunkSizeNewLine_2,
            ChunkSizeNewLine_3,
            ChunkTrailerName,
            ChunkTrailerValue,

            ChunkDataNewLine_1,
            ChunkDataNewLine_2,
            ChunkData,
        } state;

        size_t contentSize;
        std::string chunkSizeStr;
        size_t chunkSize;
        bool chunked;
    };

    class UrlParser
    {
    public:
        UrlParser() : valid(false) {}

        explicit UrlParser(const std::string& url) : valid(true) { parse(url); }

        bool parse(const std::string& str)
        {
            url = Url();
            parse_(str);

            return isValid();
        }

        bool isValid() const { return valid; }

        std::string scheme() const
        {
            assert(isValid());
            return url.scheme;
        }

        std::string username() const
        {
            assert(isValid());
            return url.username;
        }

        std::string password() const
        {
            assert(isValid());
            return url.password;
        }

        std::string hostname() const
        {
            assert(isValid());
            return url.hostname;
        }

        std::string port() const
        {
            assert(isValid());
            return url.port;
        }

        std::string path() const
        {
            assert(isValid());
            return url.path;
        }

        std::string query() const
        {
            assert(isValid());
            return url.query;
        }

        std::string fragment() const
        {
            assert(isValid());
            return url.fragment;
        }

        uint16_t httpPort() const
        {
            const uint16_t defaultHttpPort  = 80;
            const uint16_t defaultHttpsPort = 443;

            assert(isValid());

            if (url.port.empty())
            {
                if (scheme() == "https")
                    return defaultHttpsPort;
                else
                    return defaultHttpPort;
            }
            else
            {
                return url.integerPort;
            }
        }

    private:
        bool isUnreserved(char ch) const
        {
            if (isalnum(ch))
                return true;

            switch (ch)
            {
            case '-':
            case '.':
            case '_':
            case '~':
                return true;
            }

            return false;
        }

        void parse_(const std::string& str)
        {
            enum
            {
                Scheme,
                SlashAfterScheme1,
                SlashAfterScheme2,
                UsernameOrHostname,
                Password,
                Hostname,
                IPV6Hostname,
                PortOrPassword,
                Port,
                Path,
                Query,
                Fragment
            } state = Scheme;

            std::string usernameOrHostname;
            std::string portOrPassword;

            valid           = true;
            url.path        = "/";
            url.integerPort = 0;

            for (size_t i = 0; i < str.size() && valid; ++i)
            {
                char ch = str[i];

                switch (state)
                {
                case Scheme:
                    if (isalnum(ch) || ch == '+' || ch == '-' || ch == '.')
                    {
                        url.scheme += ch;
                    }
                    else if (ch == ':')
                    {
                        state = SlashAfterScheme1;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case SlashAfterScheme1:
                    if (ch == '/')
                    {
                        state = SlashAfterScheme2;
                    }
                    else if (isalnum(ch))
                    {
                        usernameOrHostname = ch;
                        state              = UsernameOrHostname;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case SlashAfterScheme2:
                    if (ch == '/')
                    {
                        state = UsernameOrHostname;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case UsernameOrHostname:
                    if (isUnreserved(ch) || ch == '%')
                    {
                        usernameOrHostname += ch;
                    }
                    else if (ch == ':')
                    {
                        state = PortOrPassword;
                    }
                    else if (ch == '@')
                    {
                        state = Hostname;
                        std::swap(url.username, usernameOrHostname);
                    }
                    else if (ch == '/')
                    {
                        state = Path;
                        std::swap(url.hostname, usernameOrHostname);
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case Password:
                    if (isalnum(ch) || ch == '%')
                    {
                        url.password += ch;
                    }
                    else if (ch == '@')
                    {
                        state = Hostname;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case Hostname:
                    if (ch == '[' && url.hostname.empty())
                    {
                        state = IPV6Hostname;
                    }
                    else if (isUnreserved(ch) || ch == '%')
                    {
                        url.hostname += ch;
                    }
                    else if (ch == ':')
                    {
                        state = Port;
                    }
                    else if (ch == '/')
                    {
                        state = Path;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case IPV6Hostname:
                    abort();  // TODO
                case PortOrPassword:
                    if (isdigit(ch))
                    {
                        portOrPassword += ch;
                    }
                    else if (ch == '/')
                    {
                        std::swap(url.hostname, usernameOrHostname);
                        std::swap(url.port, portOrPassword);
                        url.integerPort = atoi(url.port.c_str());
                        state           = Path;
                    }
                    else if (isalnum(ch) || ch == '%')
                    {
                        std::swap(url.username, usernameOrHostname);
                        std::swap(url.password, portOrPassword);
                        url.password += ch;
                        state = Password;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case Port:
                    if (isdigit(ch))
                    {
                        portOrPassword += ch;
                    }
                    else if (ch == '/')
                    {
                        std::swap(url.port, portOrPassword);
                        url.integerPort = atoi(url.port.c_str());
                        state           = Path;
                    }
                    else
                    {
                        valid = false;
                        url   = Url();
                    }
                    break;
                case Path:
                    if (ch == '#')
                    {
                        state = Fragment;
                    }
                    else if (ch == '?')
                    {
                        state = Query;
                    }
                    else
                    {
                        url.path += ch;
                    }
                    break;
                case Query:
                    if (ch == '#')
                    {
                        state = Fragment;
                    }
                    else if (ch == '?')
                    {
                        state = Query;
                    }
                    else
                    {
                        url.query += ch;
                    }
                    break;
                case Fragment:
                    url.fragment += ch;
                    break;
                }
            }

            assert(portOrPassword.empty());

            if (!usernameOrHostname.empty())
                url.hostname = usernameOrHostname;
        }

        bool valid;

        struct Url
        {
            Url() : integerPort(0) {}

            std::string scheme;
            std::string username;
            std::string password;
            std::string hostname;
            std::string port;
            std::string path;
            std::string query;
            std::string fragment;
            uint16_t integerPort;
        } url;
    };
};

}  // namespace httpparser