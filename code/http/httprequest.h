/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {//解析状态代码
        REQUEST_LINE,//正在解析请求行
        HEADERS,//...头
        BODY,//....体
        FINISH,  //完成      
    };

    enum HTTP_CODE {//请求代码
        NO_REQUEST = 0,//无请求
        GET_REQUEST,//获取请求
        BAD_REQUEST,//错误
        NO_RESOURSE,//无资源
        FORBIDDENT_REQUEST,//禁止
        FILE_REQUEST,//文件
        INTERNAL_ERROR,
        CLOSED_CONNECTION,//关闭连接
    };
    
    HttpRequest() { Init(); }//构造函数初始化工作
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);//解析请求行
    void ParseHeader_(const std::string& line);//解析请求头
    void ParseBody_(const std::string& line);//解析请求体

    void ParsePath_();//解析路径
    void ParsePost_();//解析post请求
    void ParseFromUrlencoded_();//解析表单数据

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;//解析状态
    std::string method_, path_, version_, body_;//请求报文：请求方法、请求路径、协议版本、请求体
    std::unordered_map<std::string, std::string> header_;//请求头
    std::unordered_map<std::string, std::string> post_;//post请求表单

    static const std::unordered_set<std::string> DEFAULT_HTML;//默认的网页
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);//转化成16进制
};


#endif //HTTP_REQUEST_H