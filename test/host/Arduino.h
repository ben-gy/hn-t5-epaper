// Minimal Arduino shim so the pure-logic sources can be compiled and exercised
// on a desktop. Only what text.cpp / reader.cpp actually use.
#pragma once
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cstdint>
#include <algorithm>

class String {
public:
    std::string s;
    String() {}
    String(const char *p) : s(p ? p : "") {}
    String(const std::string &v) : s(v) {}
    String(char c) { s.push_back(c); }
    String(int v)          { char b[24]; snprintf(b, sizeof b, "%d", v);   s = b; }
    String(long v)         { char b[24]; snprintf(b, sizeof b, "%ld", v);  s = b; }
    String(unsigned v)     { char b[24]; snprintf(b, sizeof b, "%u", v);   s = b; }
    String(unsigned long v){ char b[24]; snprintf(b, sizeof b, "%lu", v);  s = b; }

    size_t length() const { return s.size(); }
    const char *c_str() const { return s.c_str(); }
    char operator[](size_t i) const { return i < s.size() ? s[i] : '\0'; }
    void reserve(size_t n) { s.reserve(n); }

    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *o)   { s += o;   return *this; }
    String &operator+=(char c)          { s += c;   return *this; }

    int indexOf(char c, size_t from = 0) const {
        auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const char *n, size_t from = 0) const {
        auto p = s.find(n, from); return p == std::string::npos ? -1 : (int)p; }
    String substring(int a) const {
        if (a < 0) a = 0; if (a > (int)s.size()) a = s.size(); return String(s.substr(a)); }
    String substring(int a, int b) const {
        if (a < 0) a = 0; if (b > (int)s.size()) b = s.size(); if (b < a) b = a;
        return String(s.substr(a, b - a)); }
    bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
    void trim() {
        size_t a = 0, b = s.size();
        while (a < b && isspace((unsigned char)s[a])) a++;
        while (b > a && isspace((unsigned char)s[b - 1])) b--;
        s = s.substr(a, b - a);
    }
    void toLowerCase() { for (auto &c : s) c = tolower((unsigned char)c); }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *o) const { return s == o; }
};

inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b)   { String r(a); r += b; return r; }
inline String operator+(const char *a, const String &b)   { String r(a); r += b; return r; }
inline String operator+(const String &a, char b)          { String r(a); r += b; return r; }
inline String operator+(const String &a, int b)           { String r(a); r += String(b); return r; }
inline String operator+(const String &a, unsigned b)      { String r(a); r += String(b); return r; }
inline String operator+(const String &a, unsigned long b) { String r(a); r += String(b); return r; }
