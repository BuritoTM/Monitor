#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <vector>
#include <locale.h>

class EventMonitor {
private:
    std::mutex mtx;
    std::condition_variable cv;
    bool event_ready = false;
    bool producer_finished = false;
    
    class NonSerializableData {
    public:
        int id;
        std::string message;
        std::vector<double> data;
        
        NonSerializableData(int event_id, const std::string& msg) 
            : id(event_id), message(msg) {
            for (int i = 0; i < 10; ++i) {
                data.push_back(i * 1.5);
            }
        }
        
        void process() {
            std::cout << "Обработка данных: ID=" << id 
                      << ", Сообщение: " << message 
                      << ", Размер данных: " << data.size() << std::endl;
        }
    };
    
    std::unique_ptr<NonSerializableData> current_event;

public:
    void produceEvents(int count) {
        for (int i = 1; i <= count; ++i) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() { return !event_ready; });
                current_event = std::make_unique<NonSerializableData>(
                    i, "Событие №" + std::to_string(i)
                );
                
                event_ready = true;
                std::cout << "Поставщик: отправил событие " << i << std::endl;
            }
            
            cv.notify_one();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            producer_finished = true;
            event_ready = true;
        }
        cv.notify_one();
    }
    
    void consumeEvents() {
        while (true) {
            std::unique_ptr<NonSerializableData> event_to_process;
            
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() { return event_ready; });
                if (producer_finished && !current_event) {
                    break;
                }
                
                if (current_event) {
                    event_to_process = std::move(current_event);
                    event_ready = false;
                    
                    std::cout << "Потребитель: получил событие " 
                              << event_to_process->id << std::endl;
                }
            }
            
            if (event_to_process) {
                event_to_process->process();
            }
            cv.notify_one();
        }
        
        std::cout << "Потребитель: завершил работу" << std::endl;
    }
};

int main() {
    setlocale(LC_ALL, "Russian");
    EventMonitor monitor;
    
    std::cout << "Запуск монитора событий..." << std::endl;
    std::cout << "=================================" << std::endl;
    
    std::thread producer(&EventMonitor::produceEvents, &monitor, 5);
    std::thread consumer(&EventMonitor::consumeEvents, &monitor);
    
    producer.join();
    consumer.join();
    
    std::cout << "=================================" << std::endl;
    std::cout << "Все события обработаны" << std::endl;
    
    return 0;
}