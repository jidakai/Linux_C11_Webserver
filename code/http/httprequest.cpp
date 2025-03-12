/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{//判断路径进行拼接
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {//通过后缀判断登录还是注册，对于登录和注册设置不同的标志
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::Init() {//初始化工作
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;//初始化状态为解析请求行
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
//有限状态机解析HTTP请求：根据请求中不同类型的数据，将不同的类型映射为不同的状态（区分请求行、请求头、请求体），然后执行相应的业务逻辑。
//每个状态相互独立，状态之间的转化通过状态机内部驱动
bool HttpRequest::parse(Buffer& buff) {//传递readBuff_
    const char CRLF[] = "\r\n";//获取一行数据，回车换行为结束标志
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    //读缓冲区存储从TCP缓冲区读到的请求数据，解析读缓冲区的数据
    while(buff.ReadableBytes() && state_ != FINISH) {//判断读缓冲区是否还有可读，且只要状态机的状态不为结束则循环处理
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);//确定每次解析的结束位置
        std::string line(buff.Peek(), lineEnd);//确定每次解析的数据，从读的开始位置到结束位置读数据存储到line中
        switch(state_)
        {
        case REQUEST_LINE://解析请求行
            if(!ParseRequestLine_(line)) {//改变状态
                return false;
            }
            ParsePath_();//解析路径资源
            break;    
        case HEADERS://解析请求头
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY://解析请求体
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }//读取位置到达写指针处，解析完毕
        buff.RetrieveUntil(lineEnd + 2);//每次解析更新读指针位置，准备下次解析
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_() {//解析请求访问路径
    if(path_ == "/") {//根路径则进行拼接
        path_ = "/index.html"; //默认让其访问网页资源
    }
    else {
        for(auto &item: DEFAULT_HTML) {//根据DEFAULT_HTML拼接
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");//正则表达式解析请求行
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];//方法
        path_ = subMatch[2];//资源
        version_ = subMatch[3];//版本
        state_ = HEADERS;//改变状态
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");//键值对解析
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];//存放键值对
    }
    else {
        state_ = BODY;//改变状态解析请求体
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();//解析post表单
    state_ = FINISH;//改变状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {//解析post表单，处理登录、注册
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {//判断是否是post方法并且格式对否
        ParseFromUrlencoded_();//解析表单
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {//判断是注册还是登录
                bool isLogin = (tag == 1);
                //登陆则验证用户名和密码，否则为注册将信息插入数据库中
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";//正确提供欢迎页面
                } 
                else {
                    path_ = "/error.html";//错误提供错误页面
                }
            }
        }
    }   
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    string key, value;//键值对形式
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;
//逐个遍历确定键值对username=hello&password=hello
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%'://中文含%
        //简单的加密操作，编码解码
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;//map存放键值对
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;//数据库对象
    SqlConnRAII(&sql,  SqlConnPool::Instance());//rall机制
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    //注册
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());//将sql语句存储在order中
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { //传入连接对象和语句
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);//获取结果
    j = mysql_num_fields(res);//获取字段
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {//登录则判断是否匹配
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}