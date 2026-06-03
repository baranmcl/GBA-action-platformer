#pragma once
#include <cstdio>
#include <vector>
#include <functional>
#include <string>
struct TestCase { std::string name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, std::function<void()> f){ registry().push_back({n,f}); } };
#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()
#define CHECK(cond) do { if(!(cond)){ ++failures(); std::printf("  FAIL %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond);} } while(0)
#define CHECK_EQ(a,b) do { auto _a=(a); auto _b=(b); if(!(_a==_b)){ ++failures(); std::printf("  FAIL %s:%d  CHECK_EQ(%s,%s)  got %lld vs %lld\n", __FILE__, __LINE__, #a,#b,(long long)_a,(long long)_b);} } while(0)
inline int run_all(){ int passed=0; for(auto& t: registry()){ int before=failures(); t.fn(); if(failures()==before){++passed; std::printf("PASS %s\n", t.name.c_str());} } std::printf("\n%d/%zu tests passed, %d checks failed\n", passed, registry().size(), failures()); return failures()? 1:0; }
