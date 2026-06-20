// This is GNU C++11 compatible implementation for C++23 <print>
// Support: {}, {idx}, {:spec}, {{ → {, }} → }
// std=c++11
#ifndef _PRINT_
#define _PRINT_

#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstddef>
#include <utility>
#include <type_traits>

namespace print_detail {

// 类型包装，统一存放待格式化参数
struct ArgBase {
    virtual ~ArgBase() = default;
    virtual void format_to(char *buf, size_t bufsz, const char *spec) const = 0;
};

template<typename T>
struct Arg : ArgBase {
    T val;
    explicit Arg(T v) : val(std::move(v)) {}

    void format_to(char *buf, size_t bufsz, const char *spec) const override {
        std::snprintf(buf, bufsz, spec, val);
    }
};

// 参数包打包成 vector<ArgBase*>
template<typename...>
struct ArgsPack;

template<>
struct ArgsPack<> {
    static void fill(std::vector<ArgBase*>&) {}
};

template<typename T, typename... Rest>
struct ArgsPack<T, Rest...> {
    static void fill(std::vector<ArgBase*>& vec, T&& arg, Rest&&... rest) {
        vec.push_back(new Arg<typename std::decay<T>::type>(std::forward<T>(arg)));
        ArgsPack<Rest...>::fill(vec, std::forward<Rest>(rest)...);
    }
};

// 解析 {} 格式串，转成 printf 格式串 + 匹配参数索引
std::string parse_format(const std::string& fmt_in, std::vector<int>& arg_index_out) {
    std::string out;
    size_t i = 0;
    const size_t n = fmt_in.size();

    while (i < n) {
        if (fmt_in[i] == '{') {
            if (i + 1 < n && fmt_in[i+1] == '{') {
                // {{ 转义 {
                out += '{';
                i += 2;
                continue;
            }
            // 进入 { ... } 解析
            ++i;
            size_t pos_start = i;
            int idx = -1;
            std::string spec;

            // 读取索引部分 {0:xxx}
            while (i < n && fmt_in[i] != '}' && fmt_in[i] != ':') {
                if (isdigit(static_cast<unsigned char>(fmt_in[i]))) {
                    idx = (idx == -1 ? 0 : idx * 10) + (fmt_in[i] - '0');
                }
                ++i;
            }
            // 读取格式说明符 :xxxx
            if (i < n && fmt_in[i] == ':') {
                ++i;
                pos_start = i;
                while (i < n && fmt_in[i] != '}') {
                    spec += fmt_in[i];
                    ++i;
                }
            }
            if (i >= n || fmt_in[i] != '}') break;
            ++i;

            // 自动编号处理
            static int auto_cnt = 0;
            if (idx == -1) {
                idx = auto_cnt++;
            } else {
                auto_cnt = idx + 1;
            }
            arg_index_out.push_back(idx);

            // 映射 fmt spec → printf spec
            if (spec.empty()) {
                out += "%g";
            } else {
                out += "%";
                out += spec;
            }
        } else if (fmt_in[i] == '}') {
            if (i + 1 < n && fmt_in[i+1] == '}') {
                out += '}';
                i += 2;
                continue;
            }
            out += '}';
            ++i;
        } else {
            out += fmt_in[i];
            ++i;
        }
    }
    return out;
}

// 核心输出逻辑
template<typename... Args>
void print_impl(std::ostream& os, const char* fmt, Args&&... args) {
    std::vector<ArgBase*> args_store;
    ArgsPack<Args...>::fill(args_store, std::forward<Args>(args)...);

    std::vector<int> idx_list;
    std::string printf_fmt = parse_format(std::string(fmt), idx_list);

    char buf[4096];
    size_t pos = 0;
    const char* p = printf_fmt.c_str();
    while (*p) {
        if (*p == '%') {
            const char* q = p;
            ++q;
            while (*q && !strchr("sdfegxiu%", *q)) ++q;
            if (*q) ++q;
            size_t len = q - p;
            if (pos < idx_list.size()) {
                args_store[idx_list[pos]]->format_to(buf, sizeof(buf), p);
                os << buf;
                ++pos;
            } else {
                os.write(p, len);
            }
            p = q;
        } else {
            os.put(*p);
            ++p;
        }
    }

    // 释放内存
    for (auto ptr : args_store) delete ptr;
}

} // namespace print_detail

namespace std {

// 1. print 输出到指定 ostream
template<typename... Args>
void print(std::ostream& os, const char* fmt, Args&&... args) {
    print_detail::print_impl(os, fmt, std::forward<Args>(args)...);
}

// 2. print 默认输出 cout
template<typename... Args>
void print(const char* fmt, Args&&... args) {
    std::print(std::cout, fmt, std::forward<Args>(args)...);
}

// 3. println 输出到指定 ostream 并换行
template<typename... Args>
void println(std::ostream& os, const char* fmt, Args&&... args) {
    std::print(os, fmt, std::forward<Args>(args)...);
    os << '\n';
}

// 4. println 默认 cout 自动换行
template<typename... Args>
void println(const char* fmt, Args&&... args) {
    std::println(std::cout, fmt, std::forward<Args>(args)...);
}

} // namespace std

#endif // _PRINT_
