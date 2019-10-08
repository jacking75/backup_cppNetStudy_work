# condition_variable
[출처](https://codezine.jp/article/detail/10657?p=3 )   
   
std::condition_variable에 정의된 주요 멤버 함수는 4개가 있는데, 이 중 2개는 대기 기능이다.  
- void wait(unique_lock<mutex>& guard); 
  guard.unlock()과 동시에 자신을 차단하고 대기 상태가 되고, 블록이 풀릴 때 guard.lock()에서 돌아온다. 블록 중에는 guard를 풀어주는 것.  
- template <class Predicate> void wait(unique_lock<mutex>& guard, Predicate pred);  
  pred는 인수 없이 bool을 반환하는 함수 객체로 대기가 풀릴 조건이다. 내용은 while (! pred ()) { wait (guard); } , 즉 조건을 충족 못하는 사이 동안 오로지 wait (guard) 한다.  
  
wait()에 의한 블록을 풀어주는 통지 측도 2개,  
- void notify_one ();  
  wait()에서 기다리고 있는 스레드 중 어느 하나의 블록이 풀린다. 어떤 스레드가 움직일 수 있을지는 모른다.
- void notify_all ();  
  wait()에서 기다리고 있는 모든 스레드 블록이 풀린다.  
  
condition_variable를 사용한 concurrent_box 구현.  
```
template<typename T>
class concurrent_box {
private:
  box<T> box_;    // 박스의 내용
  std::mutex mtx_;
  std::condition_variable can_get_; // 조건 변수:get 할 수 있다
  std::condition_variable can_set_; // 조건 변수:set 할 수 있다
public:
  concurrent_box() : box_() {}
  concurrent_box(const T& value) : box_(value) {}

  void set(const T& value) {
    std::unique_lock<std::mutex> guard(mtx_); 
    // 'set 할 수 있다'를 기다린다
    can_set_.wait(guard, [this]() { return box_.empty(); });
    box_.set(value);
    // 'get 할 수 있다' 를 통지
    can_get_.notify_one();
  }

  T get() { 
    std::unique_lock<std::mutex> guard(mtx_); 
    // 'get 할 수 있다' 를 기다린다
    can_get_.wait(guard, [this]() { return box_.occupied(); });
    T value = box_.get();
    // 'set 할 수 있다' 를 통지
    can_set_.notify_one();
    return value;
  }
};
```
  　
condition_variable를 2개 사용하여  
- get() : 데이터가 들어오는 것을 기다린다 / 비어 있음을 알린다
- set() : 비는 것을 기다린다 / 데이터가 들어간 것을 통지한다
  
로 producer와 consumer의 동기화를 실현하고 있다. "핸드쉐이크"라는 동기 방식이다.  
  
  
## concurrent_queue 구현
  
```
template<typename T>
class concurrent_queue {
public:
  typedef typename std::queue<T>::size_type size_type;
private:
  std::queue<T> queue_;
  size_type capacity_; // 대기 행열 최대 수

  std::mutex mtx_;
  std::condition_variable can_pop_;
  std::condition_variable can_push_;
public:
  concurrent_queue(size_type capacity) : capacity_(capacity) {
    if ( capacity_ == 0 ) {
      throw std::invalid_argument("capacity cannot be zero.");
    }
  }

  void push(const T& value) {
    std::unique_lock<std::mutex> guard(mtx_); 
    can_push_.wait(guard, [this]() { return queue_.size() < capacity_; });
    queue_.push(value);
    can_pop_.notify_one();
  }

  T pop() { 
    std::unique_lock<std::mutex> guard(mtx_); 
    can_pop_.wait(guard, [this]() { return !queue_.empty(); });
    T value = queue_.front();
    queue_.pop();
    can_push_.notify_one();
    return value;
  }
};
```  
  