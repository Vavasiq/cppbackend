#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>


#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

using namespace std::literals;
using namespace std::chrono;

using Timer = net::steady_timer;

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        std::osyncstream os{std::cout};
        os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
           << "s] "sv << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

class ThreadChecker {
public:
    explicit ThreadChecker(std::atomic_int& counter)
        : counter_{counter} {
    }

    ThreadChecker(const ThreadChecker&) = delete;
    ThreadChecker& operator=(const ThreadChecker&) = delete;

    ~ThreadChecker() {
        // assert выстрелит, если между вызовом конструктора и деструктора
        // значение expected_counter_ изменится
        assert(expected_counter_ == counter_);
    }

private:
    std::atomic_int& counter_;
    int expected_counter_ = ++counter_;
};

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Order  : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, 
          int id, 
          std::shared_ptr<Bread> bread, 
          std::shared_ptr<Sausage> sausage, 
          GasCooker& gas_cooker, 
          HotDogHandler handler)
            : io_(io)
            , id_(id)
            , bread_(std::move(bread))
            , sausage_(std::move(sausage))
            , gas_cooker_(gas_cooker)
            , handler_{std::move(handler)} {
    }

    void Execute() {
        net::dispatch(io_, [self = shared_from_this()]() {
            self->FrySausage();
        });

        net::dispatch(io_, [self = shared_from_this()]() {
            self->BakeBread();
        });
    }

private:
    net::io_context& io_;
    
    int id_;

    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
    
    GasCooker& gas_cooker_;
    HotDogHandler handler_;
    Logger logger_{std::to_string(id_)};

    std::shared_ptr<Bread> bread_;
    std::shared_ptr<Sausage> sausage_;
    
    Timer frying_timer{io_};
    Timer baking_timer{io_};

    std::atomic_int counter_{0};

    void FrySausage() {
        logger_.LogMessage("Start frying sausage"sv);
        sausage_->StartFry(gas_cooker_, [self = shared_from_this()]() {
            
            self->frying_timer.expires_after(1500ms);
            self->frying_timer.async_wait(
                net::bind_executor(self->strand_, [self](sys::error_code ec) {
                    self->OnFrying(ec);
                })
            );
        });
    }

    void BakeBread() {
        logger_.LogMessage("Start baking bread"sv);
        bread_->StartBake(gas_cooker_, [self = shared_from_this()]() {

            self->baking_timer.expires_after(1000ms);
            self->baking_timer.async_wait(
                net::bind_executor(self->strand_, [self](sys::error_code ec) {
                    self->OnBaking(ec);
                })
            );
        });
    }

    void OnFrying(sys::error_code ec) {
        ThreadChecker checker{counter_};

        if(ec) {
            logger_.LogMessage("Frying error : "s + ec.what());
        } else {
            logger_.LogMessage("Sausage has been fried");
            sausage_->StopFry();
        }

        CheckReadiness();
    }

    void OnBaking(sys::error_code ec) {
        ThreadChecker checker{counter_};

        if(ec) {
            logger_.LogMessage("Baking error : "s + ec.what());
        } else {
            logger_.LogMessage("Bread has been Baked");
            bread_->StopBaking();
        }

        CheckReadiness();
    }

    void CheckReadiness() {
        if (!sausage_->IsCooked() || !bread_->IsCooked()) {
            return;
        }
        handler_(HotDog{id_, sausage_, bread_});
    }
    
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        net::dispatch(strand_,[this, handler = std::move(handler), order_id = ++next_order_id_]() {
            std::make_shared<Order>(io_, order_id, store_.GetBread(), store_.GetSausage(), *gas_cooker_, std::move(handler))->Execute();
        });
        
    }

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
    int next_order_id_ = 0;

    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
