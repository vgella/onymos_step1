#include <atomic>
#include <thread>
#include <cstring>
#include <cstdlib>


struct Order {
    char type; 
    char ticker[6]; 
    int quantity;
    double price;
    std::atomic<Order*> next;
};

class PriorityQueue {
public:
    std::atomic<Order*> head;
    
    PriorityQueue() : head(nullptr) {}
    
    void insert(Order* newOrder, bool isBuy) {
        Order* prevHead;
        do {
            prevHead = head.load(std::memory_order_acquire);
            newOrder->next.store(prevHead, std::memory_order_relaxed);
        } while (!head.compare_exchange_weak(prevHead, newOrder, std::memory_order_release, std::memory_order_relaxed));
    }
    
    Order* pop() {
        Order* prevHead;
        Order* nextOrder;
        do {
            prevHead = head.load(std::memory_order_acquire);
            if (!prevHead) return nullptr;
            nextOrder = prevHead->next.load(std::memory_order_acquire);
        } while (!head.compare_exchange_weak(prevHead, nextOrder, std::memory_order_release, std::memory_order_relaxed));
        return prevHead;
    }
};

class OrderBook {
public:
    PriorityQueue buyQueues[1024];
    PriorityQueue sellQueues[1024];
    char tickers[1024][6]{};
    std::atomic<int> tickerCount{0};
    
    int getTickerIndex(const char* ticker) {
        for (int i = 0; i < tickerCount.load(std::memory_order_acquire); ++i) {
            if (strcmp(tickers[i], ticker) == 0) return i;
        }
        if (tickerCount.load(std::memory_order_acquire) < 1024) {
            int index = tickerCount.fetch_add(1, std::memory_order_acq_rel);
            strncpy(tickers[index], ticker, 5);
            tickers[index][5] = '\0';
            return index;
        }
        return -1;
    }
    
    void addOrder(char type, const char* ticker, int quantity, double price) {
        int index = getTickerIndex(ticker);
        if (index == -1) return;
        
        Order* newOrder = new Order;
        newOrder->type = type;
        newOrder->quantity = quantity;
        newOrder->price = price;
        newOrder->next.store(nullptr, std::memory_order_relaxed);
        strncpy(newOrder->ticker, ticker, 5);
        newOrder->ticker[5] = '\0';
        
        if (type == 'B') {
            buyQueues[index].insert(newOrder, true);
        } else {
            sellQueues[index].insert(newOrder, false);
        }
    }
    
    void matchOrder() {
        for (int index = 0; index < tickerCount.load(std::memory_order_acquire); ++index) {
            while (true) {
                Order* buy = buyQueues[index].pop();
                Order* sell = sellQueues[index].pop();
                if (!buy || !sell) break;
                
                if (buy->price >= sell->price) {
                    int matchQty = (buy->quantity < sell->quantity) ? buy->quantity : sell->quantity;
                    buy->quantity -= matchQty;
                    sell->quantity -= matchQty;
                    
                    if (buy->quantity > 0) buyQueues[index].insert(buy, true);
                    if (sell->quantity > 0) sellQueues[index].insert(sell, false);
                } else {
                    buyQueues[index].insert(buy, true);
                    sellQueues[index].insert(sell, false);
                    break;
                }
            }
        }
    }
};

void simulateOrders(OrderBook &orderBook, int numEntries, double priceMin, double priceMax, const char* ticker) {
    for (int i = 0; i < numEntries; i++) {
        int qty = rand() % 100 + 1;
        double price = priceMin + (rand() / (RAND_MAX / (priceMax - priceMin)));
        char type = (rand() % 2) ? 'B' : 'S';
        orderBook.addOrder(type, ticker, qty, price);
    }
}

int main() {
    OrderBook orderBook;
    
    std::thread t1(simulateOrders, std::ref(orderBook), 100, 110.0, 150.0, "AAPL");
    std::thread t2(simulateOrders, std::ref(orderBook), 100, 180.0, 250.0, "GOOG");
    
    t1.join();
    t2.join();
    
    orderBook.matchOrder();
    
    return 0;
}
