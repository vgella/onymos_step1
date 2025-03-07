#include <atomic>
#include <thread>

struct Order {
    char type; // 'B' for Buy, 'S' for Sell
    std::string ticker;
    int quantity;
    double price;
    std::atomic<Order*> next;
};

class OrderBook {
public:
    std::atomic<Order*> buyHead;
    std::atomic<Order*> sellHead;
    
    OrderBook() : buyHead(nullptr), sellHead(nullptr) {}
    
    void addOrder(char type, const std::string& ticker, int quantity, double price) {
        Order* newOrder = new Order{type, ticker, quantity, price, nullptr};
        if (type == 'B') {
            insertSortedBuy(newOrder);
        } else {
            insertSortedSell(newOrder);
        }
        matchOrders();
    }
    
    void insertSortedBuy(Order* newOrder) {
        Order* prev = nullptr;
        Order* current = buyHead.load();
        
        while (current && newOrder->price <= current->price) {
            prev = current;
            current = current->next;
        }
        
        newOrder->next = current;
        if (prev) {
            prev->next = newOrder;
        } else {
            buyHead.store(newOrder);
        }
    }
    
    void insertSortedSell(Order* newOrder) {
        Order* prev = nullptr;
        Order* current = sellHead.load();
        
        while (current && newOrder->price >= current->price) {
            prev = current;
            current = current->next;
        }
        
        newOrder->next = current;
        if (prev) {
            prev->next = newOrder;
        } else {
            sellHead.store(newOrder);
        }
    }
    
    void matchOrders() {
        Order* buy = buyHead.load();
        Order* sell = sellHead.load();
        
        while (buy && sell && buy->price >= sell->price) {
            int matchQty = std::min(buy->quantity, sell->quantity);
            
            buy->quantity -= matchQty;
            sell->quantity -= matchQty;
            
            if (buy->quantity == 0) buy = buy->next;
            if (sell->quantity == 0) sell = sell->next;
        }
        
        buyHead.store(buy);
        sellHead.store(sell);
    }
};

void simulateOrders(OrderBook &orderBook) {
    for (int i = 0; i < 100; i++) {
        int qty = rand() % 100 + 1;
        double price = (rand() % 5000) / 100.0;
        char type = (rand() % 2) ? 'B' : 'S';
        orderBook.addOrder(type, "AAPL", qty, price);
    }
}

int main() {
    OrderBook orderBook;
    
    std::thread t1(simulateOrders, std::ref(orderBook));
    std::thread t2(simulateOrders, std::ref(orderBook));
    
    t1.join();
    t2.join();
    
    return 0;
}

