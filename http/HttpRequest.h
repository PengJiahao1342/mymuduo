#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <mymuduo/Timestamp.h>

#include <cctype>
#include <string>
#include <unordered_map>
#pragma once

// HttpRequest用于解析HTTP请求
class HttpRequest {
public:
    enum Method { // HTTP请求方法
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete
    };

    enum Version { // HTTP协议版本
        kUnknown,
        kHttp10,
        kHttp11
    };

    HttpRequest()
        : method_(kInvalid)
        , version_(kUnknown)
    {
    }

    void setVersion(Version v) { version_ = v; }
    Version getVersion() const { return version_; }

    bool setMethod(const char* start, const char* end)
    {
        std::string m(start, end);
        if (m == "GET") {
            method_ = kGet;
        } else if (m == "POST") {
            method_ = kPost;
        } else if (m == "HEAD") {
            method_ = kHead;
        } else if (m == "PUT") {
            method_ = kPut;
        } else if (m == "DELETE") {
            method_ = kDelete;
        } else {
            method_ = kInvalid;
        }
        // 返回是否设置成功
        return method_ != kInvalid;
    }
    Method method() const { return method_; }

    const char* methodString() const
    {
        const char* result = "UNKNOWN";
        switch (method_) {
        case kGet:
            result = "GET";
            break;
        case kPost:
            result = "POST";
            break;
        case kHead:
            result = "HEAD";
            break;
        case kPut:
            result = "PUT";
            break;
        case kDelete:
            result = "DELETE";
            break;
        default:
            break;
        }
        return result;
    }

    void setPath(const char* start, const char* end) { path_.assign(start, end); }
    const std::string& path() const { return path_; }

    void setQuery(const char* start, const char* end) { query_.assign(start, end); }
    const std::string& query() const { return query_; }

    void setReceiveTime(Timestamp t) { receiveTime_ = t; }
    Timestamp receiveTime() const { return receiveTime_; }

    // 向map中添加一个请求行内容
    void addHeader(const char* start, const char* colon, const char* end)
    {
        std::string field(start, colon); // colon冒号，冒号之前的是key
        ++colon;

        while (colon < end && std::isspace(*colon)) {
            ++colon; // 跳过冒号后面的空格
        }

        std::string value(colon, end);
        while (!value.empty() && std::isspace(value[value.size() - 1])) {
            value.resize(value.size() - 1); // 去除字符后的空格
        }
        headers_[field] = value; // key-value 添加到map中
    }

    // 从map中获取某个请求字段的内容
    std::string getHeader(const std::string& field) const
    {
        std::string result;
        std::unordered_map<std::string, std::string>::const_iterator it = headers_.find(field);
        if (it != headers_.end()) {
            result = it->second;
        }
        return result;
    }

    const std::unordered_map<std::string, std::string>& headers() const
    {
        return headers_;
    }

    void swap(HttpRequest& that)
    {
        std::swap(method_, that.method_);
        std::swap(version_, that.version_);
        path_.swap(that.path_);
        query_.swap(that.path_);
        headers_.swap(that.headers_);
        receiveTime_.swap(that.receiveTime_);
    }

private:
    Method method_; // HTTP请求方法
    Version version_; // HTTP协议版本
    std::string path_;
    std::string query_;
    Timestamp receiveTime_;
    std::unordered_map<std::string, std::string> headers_; // 请求头通过:冒号分隔key-value，使用map保存
};

#endif