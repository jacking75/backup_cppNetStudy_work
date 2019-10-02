#  single-file C/C++ libraries 소개
- 저장소: https://github.com/nothings/single_file_libs  
- C 혹은 C++에서 간단하게 사용할 수 있다
- 1개 이상의 플랫폼 지원
- 32/64비트 플랫폼 모두 지원
- 파일 수는 최대 2개까지
  
아래는 이 저장소에서 소개하는 라이브러리 중 일부 소개  

## 디버그 도움 유틸리티
- https://github.com/wc-duck/dbgtools
- 크로스 플랫폼 지원
- test 디렉토리에 C, C++용 테스트 파일이 있으므로 이것을 통해 사용법을 알 수 있다
- assert.h - implements a replacement for the standard assert() macro supporting callback at assert, and error-message with printf-format.
- callstack.h - implements capturing of callstack/backtrace + translation of captured symbols into name, file, line and offset.
- debugger.h - implements debugger_present to check if a debugger is attached to the process.
- static_assert.h - defines the macro STATIC_ASSERT( condition, message_string ) in an "as good as possible way" depending on compiler features and support. It will try to use builtin support for static_assert and _Static_assert if possible.
- fpe_ctrl.h - implements platform independent functions to get/set floating point exception and enable trapping of the same exceptions.
- hw_breakpoint.h - implements platform independent hardware breakpoints. 


## 프로그램 인자 값 파싱
- https://github.com/sailormoon/flags  
- C++17 표준 사용
- 크로스 플랫폼 지원
- 인자 타입으로 bool, 정수, 문자열   
  
예제 코드  
```
#include "flags.h"
#include <iostream>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);

  const auto count = args.get<int>("count");
  if (!count) {
    std::cerr << "No count supplied. :(" << std::endl;
    return 1;
  }
  std::cout << "That's " << *count << " incredible, colossal credits!" << std::endl;

  if (args.get<bool>("laugh", false)) {
    std::cout << "Ha ha ha ha!" << std::endl;
  }
  return 0;
}
```  
  
<pre>
$ ./program --count=5 --laugh
> That's 5 incredible, colossal credits!
> Ha ha ha ha!
</pre>  
  

## Random access array of tightly packed unsigned integers
- uint 배열을 특정 비트 단위로 나누어서 사용할 수 있다.
- https://github.com/gpakosz/PackedArray  
- 크로스 플랫폼 지원
  
```
PackedArray* a = PackedArray_create(bitsPerItem, count);
...
value = PackedArray_get(a, i);
...
PackedArray_set(a, j, value);
```  
  

## 수식 표현 파싱 
- https://github.com/codeplea/tinyexpr  
- 작고 빠르다
- 수식 계산 문자열을 결과를 계산할 수 있다
- 수식 계산을 스크립트 파일로 사용하고 싶을 때 유용한다
- tinyexpr.c 와 tinyexpr.h 파일 2개를 프로젝트에 포함시키면 된다
  
```
#include "tinyexpr.h"
printf("%f\n", te_interp("5*5", 0)); /* Prints 25. */
```  
  
```
int error;

double a = te_interp("(5+5)", 0); /* Returns 10. */
double b = te_interp("(5+5)", &error); /* Returns 10, error is set to 0. */
double c = te_interp("(5+5", &error); /* Returns NaN, error is set to 4. */
```
  
```
double x, y;
/* Store variable names and pointers. */
te_variable vars[] = {{"x", &x}, {"y", &y}};

int err;
/* Compile the expression with variables. */
te_expr *expr = te_compile("sqrt(x^2+y^2)", vars, 2, &err);

if (expr) {
    x = 3; y = 4;
    const double h1 = te_eval(expr); /* Returns 5. */

    x = 5; y = 12;
    const double h2 = te_eval(expr); /* Returns 13. */

    te_free(expr);
} else {
    printf("Parse error at %d\n", err);
}
```
  
```
double my_sum(double a, double b) {
    /* Example C function that adds two numbers together. */
    return a + b;
}

te_variable vars[] = {
    {"mysum", my_sum, TE_FUNCTION2} /* TE_FUNCTION2 used because my_sum takes two arguments. */
};

te_expr *n = te_compile("mysum(5, 6)", vars, 1, 0);
```
  

## 스트링 풀(string pool)
- https://github.com/mattiasgustavsson/libs  
  
```
#define  STRPOOL_IMPLEMENTATION
#include "strpool.h"

#include <stdio.h> // for printf
#include <string.h> // for strlen

int main( int argc, char** argv )
{
    (void) argc, argv;

    strpool_config_t conf = strpool_default_config;
    //conf.ignore_case = true;

    strpool_t pool;
    strpool_init( &pool, &conf );

    STRPOOL_U64 str_a = strpool_inject( &pool, "This is a test string", (int) strlen( "This is a test string" ) );
    STRPOOL_U64 str_b = strpool_inject( &pool, "THIS IS A TEST STRING", (int) strlen( "THIS IS A TEST STRING" ) );

    printf( "%s\n", strpool_cstr( &pool, str_a ) );
    printf( "%s\n", strpool_cstr( &pool, str_b ) );
    printf( "%s\n", str_a == str_b ? "Strings are the same" : "Strings are different" );

    strpool_term( &pool );
    return 0;
}
``` 
  
  
## thread 
- https://github.com/mattiasgustavsson/libs  
- 크로스 플랫폼 지원
- 스레드 생성/중단/삭제 이외에 atomic, mutex, timer, queue, tls 
  

## 간단한 소켓 라이브러리 
- https://github.com/Smilex/zed_net  
- TCP, UDP 지원
- 크로스 플랫폼 지원
- 1:1 통신 혹은 클라이언트 용으로 적합
   
  
## 프로파일러  
- https://github.com/jonasmr/microprofile   
- 크로스 플랫폼 지원
- 간단하게 사용할 수 있다
- GPU Timing, Counters, Timeline, Live View, Capture view, Dynamic Instrumentation
  

## 유닛테스트  
- https://github.com/catchorg/Catch2
- C++03 혹은 C++11 이상 지원
- 유닛테스트를 쉽게 할 수 있다 

  

## Callback
- https://github.com/codeplea/pluscallback  
- callback.hpp 파일 1개만 포함하면 사용할 수 있다
- 콜백 함수에 C++ 함수를 사용할 수 있다
- 클래스의 상호 참조를 없애는데 도움이 될 듯  
         
```
//Setup callback for TestObject.Foo().
cb::Callback1<int, int> callback(&TestObject, &TestClass::Foo);

//Call TestObject.Foo(5).
callback(5);

//Change callback to a free function.
callback = SomeRandomFunction;

//Call SomeRandomFunction(8).
callback(8);
```