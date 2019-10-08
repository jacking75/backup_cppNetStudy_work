# Delay Queue
[출처](https://yohhoy.hatenadiary.jp/entry/20180412/p1 )   
   
스레드 세이프한 지연 기능이 있는 · 제한 없는 데이터 큐의 C++ 구현. Java 표준 라이브러리 java.util.concurrent.DelayQueue 와 비슷.  
  
보통 데이터 큐와 달리 큐에 요소를 추가 할 때 "유효 시간"을 지정한다. 해당 요소는 이 유효 시간을 넘기 전까지 큐에 체류하고 큐에서 제거 작업의 대상이 되지 않는다 (지연 기는).  
  
- enqueue 작업
    - 유효 시간을 지정한 요소 값을 큐에 저장한다. 큐 용량은 제한 없기 때문에 enqueue 작업은 항상 성공한다.
- dequeue 작업
    - 현재 시간보다 작고 오래된 유효 시간을 가지는 요소 값을 큐에서 꺼낸다. 유효한 요소가 존재하지 않는 경우 dequeue 작업을 차단 한다.
- close 작업
    - 데이터 끝을 알려준다. close 조작 이후의 enqueue 작업은 실패한다. 큐가 빈 후의 dequeue 작업은 유효하지 않은 값을 반환한다.
     
	 
```
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

struct closed_queue : std::exception {};

template <typename T, typename Clock = std::chrono::system_clock>
class delay_queue {
public:
  using value_type = T;
  using time_point = typename Clock::time_point;

  delay_queue() = default;
  explicit delay_queue(std::size_t initial_capacity)
    { q_.reserve(initial_capacity); }
  ~delay_queue() = default;

  delay_queue(const delay_queue&) = delete;
  delay_queue& operator=(const delay_queue&) = delete;

  void enqueue(value_type v, time_point tp)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (closed_)
      throw closed_queue{};
    q_.emplace_back(std::move(v), tp);
    // descending sort on time_point
    std::sort(begin(q_), end(q_),
              [](auto&& a, auto&& b) { return a.second > b.second; });
    cv_.notify_one();
  }

  value_type dequeue()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    auto now = Clock::now();
    // wait condition: (empty && closed) || (!empty && back.tp <= now)
    while (!(q_.empty() && closed_) && !(!q_.empty() && q_.back().second <= now)) {
      if (q_.empty())
        cv_.wait(lk);
      else
        cv_.wait_until(lk, q_.back().second);
      now = Clock::now();
    }
    if (q_.empty() && closed_)
      return {};  // invalid value
    value_type ret = std::move(q_.back().first);
    q_.pop_back();
    if (q_.empty() && closed_)
      cv_.notify_all();
    return ret;
  }

  void close()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    closed_ = true;  
    cv_.notify_all();
  }

private:
  std::vector<std::pair<value_type, time_point>> q_;
  bool closed_ = false;
  std::mutex mtx_;
  std::condition_variable cv_;
};
```
  
사용 예    
```
template <typename T>
void dump(const T& v, std::chrono::system_clock::time_point epoch)
{
  using namespace std::chrono;
  auto elapsed = duration_cast<milliseconds>(system_clock::now() - epoch).count() / 1000.;
  std::cout << elapsed << ":" << v << std::endl;
}

int main()
{
  auto base = std::chrono::system_clock::now();
  constexpr std::size_t capacity = 5;
  delay_queue<std::unique_ptr<std::string>> q{capacity};

  auto f = std::async(std::launch::async, [&q, base] {
    using namespace std::chrono_literals;
    q.enqueue(std::make_unique<std::string>("two"),   base + 2s);
    q.enqueue(std::make_unique<std::string>("three"), base + 3s);
    q.enqueue(std::make_unique<std::string>("one"),   base + 1s);
    q.close();
  });

  dump("start", base);
  dump(*q.dequeue(), base);  // "one"
  dump(*q.dequeue(), base);  // "two"
  dump(*q.dequeue(), base);  // "three"
  assert(q.dequeue() == nullptr);  // end of data
}
```
    