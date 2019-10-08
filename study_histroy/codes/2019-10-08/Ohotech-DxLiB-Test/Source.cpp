#define _USE_MATH_DEFINES
#define _WINSOCK_DEPRECATED_NO_WARNINGS

/*
	hwm::task_queueを使って、?ル?スレッドで並列にデ??を処理するサンプルゲ??

	参考にしたコ?ド: http://homepage2.nifty.com/natupaji/DxLib/dxprogram.html#N24

	UseMultiThread?クロに1を設定してコンパイルすると、Lasers::Update()関数で並列処理を行う
*/

#include <cmath>
#include <future>
#include <vector>
#include <thread>
#include <random>
#include <map>
#include <memory>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <boost/thread/latch.hpp>
#include "./task/task_queue.hpp"

#include "DxLib.h"

#define UseMultiThread 1

int const kWidth = 1024;		// 画面サイズ
int const kHeight = 768;		// 画面サイズ

static double const kVelocityLimit = 15;    // 速度の制限(ピクセル)
static int const kGameOverTime = 30 * 60;   // フレ??
static int const kNumLaserLimit = 400;		// レ?ザ?の数
static int const kShootInterval = 2;		// 発射の間隔(フレ??)
static int const kAfterGrowTimeLimit = 64;	// 残光の時間(フレ??)
static int const kHitBlinkTime = 120;       // レ?ザ?衝突時の?滅時間
static int const kHitBlinkPeriod = 10;		// ?滅の間隔
static int const kTextSize = 24;			// 標?のテキストサイズ
static int const kScoreTextSize = 60;		// スコア画面のテキストサイズ
static std::string const kGameTitle = "30秒間何とか逃げるゲ??";
static int const kNumVelocityDirections = 12; // レ?ザ?の移動方向の数
static int const kStartButton = PAD_INPUT_12;
static int const kAButton = PAD_INPUT_4;
static int const kL1Button = PAD_INPUT_5;
static int const kR1Button = PAD_INPUT_6;

class Player;	//自?
class Boss;		//?ス。レ?ザ?を一定数発射して、上限に達したら消える
class Laser;	//レ?ザ?。自?を攻撃する
class Lasers;	//レ?ザ?のコンテナ

//! 0.0 .. 1.0の乱数を取得
//! ?当はthread_localが使えればスレッドロ?カルなRandReal変数を作ってmutexを無くしたい
struct RandReal
{
	RandReal()
		:	dist_(0.0, 1.0)
	{}

	double operator()()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return dist_(prng_);
	}

private:
	std::mutex mtx_;
	std::mt19937 prng_;
	std::uniform_real_distribution<> dist_;
};
RandReal randreal;

std::unique_ptr<hwm::task_queue> tq;

struct RGBColor
{
	double r_;
	double g_;
	double b_;

	RGBColor(double r, double g, double b)
		:	r_(r)
		,	g_(g)
		,	b_(b)
	{}

	int ToInt() const
	{
		return ::GetColor(
			static_cast<int>(r_ * 256 + 0.5),
			static_cast<int>(g_ * 256 + 0.5),
			static_cast<int>(b_ * 256 + 0.5)
			);
	}
};

int HSVToRGB(double hue, double saturation, double brightness)
{
	if(saturation == 0) {
		return RGBColor(brightness, brightness, brightness).ToInt();
	}

	double const C = saturation * brightness;

	int const H = static_cast<int>(hue * 6.0);
	double X = C * (1 - fabs(fmod(hue * 6.0, 2) - 1));

	switch(H) {
		case 0: return RGBColor(C, X, 0).ToInt();
		case 1: return RGBColor(X, C, 0).ToInt();
		case 2: return RGBColor(0, C, X).ToInt();
		case 3: return RGBColor(0, X, C).ToInt();
		case 4: return RGBColor(X, 0, C).ToInt();
		case 5: return RGBColor(C, 0, X).ToInt();
		default: return RGBColor(C, 0, X).ToInt();
	}
}

template<class T>
struct Position
{
	Position() {}
	Position(T x, T y) : x_(x), y_(y) {}
	T x_;
	T y_;

	void Set(T x, T y) { x_ = x; y_ = y; }
};

template<class T>
struct Velocity
{
	Velocity() {}
	Velocity(T x, T y) : x_(x), y_(y) {}
	T x_;
	T y_;

	void Set(T x, T y) { x_ = x; y_ = y; }
};

template<class T>
T clip(T minimum, T val, T maximum) {
	return std::max<T>(minimum, std::min<T>(val, maximum));
};

typedef std::unordered_map<int, Velocity<double>> velocity_map_t;
velocity_map_t initialize_map()
{
	velocity_map_t tmp;
	for(size_t i = 0; i < kNumVelocityDirections; ++i) {
		tmp[i].Set(
			cos(i * 2 * M_PI / kNumVelocityDirections),
			-sin(i * 2 * M_PI / kNumVelocityDirections) //画面座標なので下方向でyが増加する
			);
	}

	return tmp;
}

velocity_map_t const velocity_map = initialize_map();

struct AfterGrow
{
	AfterGrow(Position<int> position)
		:	position_(position)
		,	time_to_live_(kAfterGrowTimeLimit)
	{}

	Position<int> position_;
	int time_to_live_;
};

// レ?ザ??造体?宣言
class Laser
{
public:
	Laser()
		:	velocity_(0, 0)
		,	is_active_(false) 
	{
		history_.set_capacity(kAfterGrowTimeLimit);
	}

	Position<double> position_;
	Velocity<double> velocity_;
	int angle_;					// 進んでいる方向
	boost::circular_buffer<AfterGrow> history_;
	bool is_active_;
	double hue_;

	void Init(double pos_x, double pos_y)
	{
		angle_ = 0;
		velocity_ = Velocity<double>(0, 0);
		is_active_ = true;
		position_ = Position<double>(pos_x, pos_y);
		history_.push_back(Position<int>(pos_x, pos_y));
		hue_ = randreal();
	}

	bool IsUsed() const
	{
		return is_active_ || !history_.empty();
	}

	template<class SceneType>
	void Update(SceneType &scene)
	{
		auto &player = scene.player_;
		auto elapsed = scene.elapsed_;
		auto const &pos = position_;

		if(is_active_) {
			if(player.Hit(pos)) {
				is_active_ = false;
			}
		}

		if(!is_active_) {
			return;
		}


		// ************************************************************************
		// ******* レ?ザ?の思考AIと移動の計算処理は非常に重たいものとする *******
		// ************************************************************************
		int volatile dummy_calc = 0;
		for(int i = 0; i < 100; ++i) {
			for(int j = 0; j < 100; ++j) {
				dummy_calc = position_.x_ + i * j + (dummy_calc & 0x12345);
				dummy_calc &= 0xFFFF0FFF;
			}
		}
		position_.x_ += (dummy_calc & 0xF000);
		// ************************************************************************
		// ************************************************************************
		// ************************************************************************

		// bx,by 自分の進んでいる方向 ax,ay ?来進むべき方向  
		double const bx = velocity_map.at(angle_).x_;
		double const by = velocity_map.at(angle_).y_;
		double const ax = player.position_.x_ - pos.x_;
		double const ay = player.position_.y_ - pos.y_;

		// 内積から、レ?ザ?が自?に向かっているかどうかを判定する
		bool const approaching = 
			(ax * velocity_.x_ + ay * velocity_.y_) > 0;
		
		// 離れていってしまってる時は方向?換を多めにする
		int const turn_amount = approaching ? 1 : 3;

		// 外積を利用しレ?ザ?の向きを自?に向ける
		angle_ += ( ax * by - ay * bx > 0.0 ) ? turn_amount : -turn_amount;

		angle_ = (angle_ + kNumVelocityDirections) % kNumVelocityDirections;

		// 速度を変更する
		auto const get_accel = [](int time) { return (1 + time * time) / 200000.0; };
		double const accel = (elapsed > 600) ? get_accel(elapsed - 600) : 1.0;
		velocity_.x_ += velocity_map.at(angle_).x_ * 0.025 * accel;// * (0.8 + randreal() * 4 / 10.0);
		velocity_.y_ += velocity_map.at(angle_).y_ * 0.025 * accel;// * (0.8 + randreal() * 4 / 10.0);

		// 速度が大きすぎる場合は制限する
		double const x_sign = velocity_.x_ < 0 ? -1 : 1;
		double const y_sign = velocity_.y_ < 0 ? -1 : 1;

		double const x2 = velocity_.x_ * velocity_.x_;
		double const y2 = velocity_.y_ * velocity_.y_;
		double const current_speed2 = x2 + y2;
		double const kVelocityLimit2 = kVelocityLimit * kVelocityLimit;
		if(current_speed2 > kVelocityLimit2) {
			double const new_x2 = x2 / (x2 + y2) * kVelocityLimit2;
			double const new_y2 = y2 / (x2 + y2) * kVelocityLimit2;
			velocity_.Set(x_sign * sqrt(new_x2), y_sign * sqrt(new_y2));
		}

		// 再設定した速度を元に位置を移動
		position_.Set(pos.x_ + velocity_.x_, pos.y_ + velocity_.y_);

		history_.push_back(
			Position<int>(static_cast<int>(position_.x_ + 0.5), static_cast<int>(position_.y_ + 0.5))
			);
	}

	void Draw()
	{
		auto &history = history_;

		if(history.empty()) {
			return;
		}

		for(size_t i = 0; i < history.size(); ++i) {
			--history[i].time_to_live_;

			if(i == history.size() - 1) {
				continue;
			}

			auto &f1 = history[i];
			auto &f2 = history[i+1];

			bool const visible = 
				0 <= f1.position_.x_ && f1.position_.x_ < kWidth &&
				0 <= f2.position_.x_ && f2.position_.x_ < kWidth &&
				0 <= f1.position_.y_ && f1.position_.y_ < kHeight &&
				0 <= f2.position_.y_ && f2.position_.y_ < kHeight;

			if(visible) {
				static double const k = 0.9998;

				double const saturation = (history[i].time_to_live_ / (double)kAfterGrowTimeLimit);
				double const brightness = (history[i].time_to_live_ / (double)kAfterGrowTimeLimit);

				int color = HSVToRGB(hue_, saturation, brightness);

				DrawLine(
					f1.position_.x_, f1.position_.y_,
					f2.position_.x_, f2.position_.y_,
					color );
			}
		}

		if(history.front().time_to_live_ == 0) {
			history.pop_front();
		}
	}
};

class Lasers
{
public:
	Lasers()
	{}

	void Init()
	{
		lasers_.resize(0); // clear all lasers
		lasers_.resize(kNumLaserLimit);
	}

	void AddLaser(Laser new_laser)
	{
		for(auto &laser: lasers_) {
			//! 未使用
			if(!laser.IsUsed()) {
				laser = new_laser;
				break;
			}
		}
	}

	template<class SceneType>
	void Update(SceneType &scene)
	{
		// レ?ザ?の移動処理
		// レ?ザ?の思考AIや移動の計算処理が非常に重たい。
		// これを?ル?スレッドで解消する

	#if UseMultiThread == 0
		for(auto &laser: lasers_) {
			laser.Update(scene);
		}
	#else

		// 複数のレ?ザ?を各スレッドで分割して処理する
		size_t const num_threads = tq->num_threads();
		size_t const num_data_per_thread = (std::max<int>)(kNumLaserLimit / num_threads, 1);

		for(size_t index = 0; index < num_threads; ++index) {

			// ?スクキュ?に処理を追加
			tq->enqueue(
				[&scene, index, num_data_per_thread, this] {
					size_t const begin = index * num_data_per_thread;
					size_t const end = std::min<size_t>(kNumLaserLimit, (index+1) * num_data_per_thread);

					// 各?スクは、自分に割り当てられた量のレ?ザ?を処理する
					for(size_t i = begin; i < end; ++i) {
						lasers_[i].Update(scene);
					}
				});
		}

		// 追加したすべての?スクが終わるのを待?
		tq->wait();

	#endif
	}

	void Draw()
	{
		// ?画ブレンドモ?ドを加算半透明にセット
		SetDrawBlendMode( DX_BLENDMODE_ADD , 255 ) ;

		for(auto &laser: lasers_) {
			laser.Draw();
		}

		// ?画ブレンドモ?ドを通常?画モ?ドにセット
		SetDrawBlendMode( DX_BLENDMODE_NOBLEND , 255 ) ;
	}

private:
	std::vector<Laser> lasers_;
};

class Boss
{
public:
	Boss()
		:	had_shot_(0)
		,	is_alive_(true)
	{}

	void Init()
	{
		had_shot_ = 0;
		is_alive_ = true;
		shoot_interval_ = kShootInterval;

		position_.Set(kWidth / 2, 30);
		velocity_.Set(3, 0);
	}

	Velocity<int> velocity_;
	Position<int> position_;
	int shoot_interval_;
	int had_shot_;
	int is_alive_;

	template<class SceneType>
	void Shot(SceneType &scene)
	{
		Laser new_laser;
		new_laser.Init(position_.x_ + 8, position_.y_ + 8);
		scene.lasers_.AddLaser(new_laser);
	}

	template<class SceneType>
	void Update(SceneType &scene)
	{
		if(!is_alive_) {
			return;
		}

		position_.x_ += velocity_.x_;

		// 画面?まで来ていたら反?
		if(position_.x_ > kWidth - 100 || position_.x_ < 100) velocity_.x_ *= -1;

		if(--shoot_interval_ == 0) {
			Shot(scene);

			// 発射間隔カウン?値をセット
			shoot_interval_ = kShootInterval;
			if(++had_shot_ >= kNumLaserLimit) {
				is_alive_ = false;
			}
		}
	}

	void Draw() const
	{
		if(is_alive_) {
			// 砲台の?画
			DrawBox(position_.x_ - 8, position_.y_ - 8, position_.x_ + 8, position_.y_ + 8, GetColor( 255 , 255 , 0 ) , TRUE);
		}
	}
};

class Player
{
public:
	void Init()
	{
		// 自?の座標セット
		position_.x_ = kWidth / 2;
		position_.y_ = kHeight - 30;
		hit_blink_time_ = 0;
		hit_count_ = 0;
		blink_offset_ = 0;
	}

	int hit_blink_time_;
	int blink_offset_;
	int hit_count_;
	Position<int> position_;

	bool Hit(Position<double> laser)
	{
		bool const hit =
			(laser.x_ > position_.x_ - 16 && laser.x_ < position_.x_ + 16) &&
			(laser.y_ > position_.y_ - 16 && laser.y_ < position_.y_ + 16);

		if(hit) {
			hit_blink_time_ = kHitBlinkTime;
			++hit_count_;
		}
		return hit;
	}

	template<class SceneType>
	void Update(SceneType &scene)
	{
		// プレイヤ?の移動処理
		// 入力取得
		int const key = GetJoypadInputState( DX_INPUT_KEY_PAD1 );
		char keybuf[256];
		GetHitKeyStateAll(keybuf);

		int x_delta = 0;
		int y_delta = 0;
		int velocity = 
			( (key & PAD_INPUT_3) ||
			  (keybuf[KEY_INPUT_LSHIFT]) ||
			  (keybuf[KEY_INPUT_RSHIFT]) )
			? 1 : 5;

		if(key & PAD_INPUT_RIGHT) { x_delta += velocity; }
		if(key & PAD_INPUT_LEFT) { x_delta -= velocity; }
		if(key & PAD_INPUT_UP) { y_delta -= velocity; }
		if(key & PAD_INPUT_DOWN) { y_delta += velocity; }

		position_ = 
			Position<int>(
				clip(16, position_.x_ + x_delta, kWidth - 16),
				clip(16, position_.y_ + y_delta, kHeight - 16) );

		if(hit_blink_time_) {
			--hit_blink_time_;
			blink_offset_ = (blink_offset_ + 1) % (kHitBlinkPeriod * 2);
		} else {
			blink_offset_ = 0;
		}
	}

	void Draw() const
	{
		int color = 0;

		if(hit_blink_time_ != 0) {
			color =
				(((blink_offset_ / kHitBlinkPeriod) & 0x1) == 1)
				?   GetColor(255, 40, 20)
				:	GetColor(255, 255, 255);
		} else {
			color = GetColor(255, 255, 255);
		}

		// プレ?ヤ?の?画
		DrawBox(position_.x_ - 16, position_.y_ - 16, position_.x_ + 16 , position_.y_ + 16, color, TRUE);
	}
};

struct FPS
{
	typedef double seconds_t;

	FPS(int frame_period = 60)
		:	frame_period_(frame_period)
		,	counter_(0)
		,	value_(60.0)
	{
		prev_ = GetNowHiPerformanceCount();
	}

	void Update()
	{
		++counter_;
		auto const microsec_to_sec = [](LONGLONG microsec) { return microsec / (1000 * 1000.0); };
		if(counter_ == frame_period_) {
			LONGLONG const now = GetNowHiPerformanceCount();
			value_ = frame_period_ / microsec_to_sec(now - prev_);
			prev_ = now;
			counter_ = 0;
		}
	}

	std::string ToString()
	{
		char buf[64];
		sprintf(buf, "FPS : %06.2f", value_);
		return buf;
	}

private:
	int frame_period_;
	int counter_;
	seconds_t value_;
	LONGLONG prev_;
};

struct Timer
{
	Timer()
	{}

	int frame_;

	void Init()
	{
		frame_ = 0;
	}

	void Update()
	{
		if(frame_ + 1 < 60000) {
			++frame_;
		}
	}

	std::string ToString()
	{
		char buf[64];
		sprintf(buf, "Time: %06.2f", frame_ / 60.0);
		return buf;
	}
};

struct Stage
{
	Boss boss_;
	Player player_;
	Lasers lasers_;
	int elapsed_;
	Timer timer_;

	void Init()
	{
		elapsed_ = 0;
		player_.Init();
		boss_.Init();
		lasers_.Init();
		timer_.Init();
	}

	void Update()
	{
		player_.Update(*this);
		lasers_.Update(*this);
		boss_.Update(*this);
		timer_.Update();
		++elapsed_;
	}

	void Draw()
	{
		lasers_.Draw();
		player_.Draw();
		boss_.Draw();

		{
			char buf[64];
			sprintf(buf, "Hit/Laser : %03d/%03d", player_.hit_count_, boss_.had_shot_);
			int const text_width = GetDrawStringWidth(buf, -1);
			int const text_height = 24;

			static int text_color = GetColor(255, 255, 255);
			static int text_edge_color = GetColor(150, 120, 240);

			DrawString(kWidth - text_width - 10, kHeight - 24 - 10, buf, text_color, text_edge_color);
		}

		{
			std::string time = timer_.ToString();

			int const text_width = GetDrawStringWidth(time.c_str(), -1);

			static int text_color = GetColor(255, 255, 255);
			static int text_edge_color = GetColor(240, 120, 240);

			DrawString(kWidth - text_width - 10, kTextSize, time.c_str(), text_color, text_edge_color); //時間を?示
		}
	}
};

struct Title
{
	void Draw()
	{
		std::string const instruction = "Press Pad 12 Button";  
		int const title_text_width = GetDrawStringWidth(kGameTitle.c_str(), -1);
		int const instruction_text_width = GetDrawStringWidth(instruction.c_str(), -1);

		static int text_color = GetColor(255, 255, 255);
		static int text_edge_color = GetColor(150, 120, 240);

		DrawString((kWidth - title_text_width) / 2 , (kHeight / 2) - kTextSize , kGameTitle.c_str(), text_color, text_edge_color);
		DrawString((kWidth - instruction_text_width) / 2 , (kHeight / 2), instruction.c_str(), text_color, text_edge_color);
	}
};

struct Result
{
	Result()
		:	result_(0)
	{
	}

	void Init()
	{
		result_ = 0;
	}

	void Draw()
	{
		SetFontSize(kScoreTextSize);

		char buf[64];
		sprintf(buf, "Score %d", result_);
		int const text_width = GetDrawStringWidth(buf, -1);

		static int text_color = GetColor(255, 255, 255);
		static int text_edge_color = GetColor(150, 120, 240);

		DrawString((kWidth - text_width) / 2, (kHeight - kScoreTextSize) / 2, buf, text_color, text_edge_color);
		SetFontSize(kTextSize);

	}

	int result_;
};

class Scene
{
public:
	enum class Status {
		kTitle,
		kStage,
		kResult
	};

	Status status_;
	Title title_;
	Result result_;
	Stage stage_;
	int elapsed_;

	FPS fps_;
	std::array<int, 256> keys_;
	std::map<int, int> pad_;

	void Init()
	{
		std::fill(keys_.begin(), keys_.end(), 0);
		status_ = Status::kTitle;
	}

	void UpdateKeys()
	{
		// 参考にしたサイト: http://dixq.net/g/02_09.html

        char tmpKey[256];
        GetHitKeyStateAll(tmpKey);
        for( int i=0; i<256; i++ ){ 
            if(tmpKey[i] != 0){
				// 押されているあいだ加算
                keys_[i]++;
            } else {     
                keys_[i] = 0;
            }
        }
	}

	void UpdatePadKeys()
	{
		unsigned int state = GetJoypadInputState(DX_INPUT_PAD1);
		for(int i = 0; i < 28; ++i, state >>= 1) {
			if(state & 0x1) {
				pad_[1 << i] += 1;
			} else {
				pad_[1 << i] = 0;
			}
		}
	}

	void Update()
	{
		UpdateKeys();
		UpdatePadKeys();

		switch(status_) {
			case Status::kTitle: {
				if( pad_[kStartButton] == 1 ||
					keys_[KEY_INPUT_SPACE] == 1 )
				{
					status_ = Status::kStage;
					stage_.Init();
				}
			} break;

			case Status::kStage: {
				if(stage_.elapsed_ > kGameOverTime) {
					status_ = Status::kResult;
					result_.result_ = kNumLaserLimit - stage_.player_.hit_count_;
					break;
				}
				if( (pad_[kL1Button] && pad_[kR1Button]) ||
					(keys_[KEY_INPUT_BACK]) )
				{ 
					// リセット
					status_ = Status::kTitle;
				} else {
					stage_.Update();
				}
			} break;

			case Status::kResult: {
				if( (pad_[kAButton] == 1 || pad_[kStartButton] == 1) ||
					(keys_[KEY_INPUT_SPACE] == 1) )
				{
					status_ = Status::kTitle;
				} else {
					stage_.Update();
				}
			} break;
		}

		fps_.Update();
	}

	void Draw()
	{
		// 画面の初期化
		ClearDrawScreen();

		switch(status_) {
			case Status::kTitle: {
				title_.Draw();
			} break;

			case Status::kStage: {
				stage_.Draw();
			} break;

			case Status::kResult: {
				result_.Draw();
			} break;
		}
			
		fps_.Update();

		{
			std::string fps = fps_.ToString();

			int const text_width = GetDrawStringWidth(fps.c_str(), -1);

			static int text_color = GetColor(255, 255, 255);
			static int text_edge_color = GetColor(240, 120, 240);

			DrawString(10, kTextSize, fps.c_str(), text_color, text_edge_color); //fpsを?示
		}

		// 裏画面の内容を?画面に反映
		ScreenFlip() ;
	}
};

// WinMain関数
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
					 LPSTR lpCmdLine, int nCmdShow )
{
	// 画面モ?ドの設定
	SetGraphMode( kWidth , kHeight , 32 ) ;

	ChangeWindowMode(true);
	SetAlwaysRunFlag(true);
	SetFontSize(kTextSize);

	// ＤＸライブラリ初期化処理
	if(DxLib_Init() == -1) {
		return -1;
	}

	// ?画先を裏画面にセット
	SetDrawScreen(DX_SCREEN_BACK);

	Scene scene;

	// 初期化処理
	// 適当に、システ?の並行性+2を指定している
	tq.reset(new hwm::task_queue(std::thread::hardware_concurrency() + 2));
	scene.Init();

	// ゲ??ル?プ
	LONGLONG Time = GetNowHiPerformanceCount() + 1000000 / 120;
	while(ProcessMessage() == 0 && CheckHitKey( KEY_INPUT_ESCAPE ) == 0) {
		scene.Update();
		scene.Draw();
				// 時間待ち
		while( GetNowHiPerformanceCount() < Time ){}
		Time += 1000000 / 120 ;
	}

	tq.reset(); //?スクキュ?のインス?ンスを破棄

	DxLib_End() ;				// ＤＸライブラリ使用の終了処理

	return 0 ;				// ?フトの終了
}