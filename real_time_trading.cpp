#include <atomic>
#include <thread>
#include <cstring>
#include <cstdlib>

struct Order {
    char type; // 'B' for Buy, 'S' for Sell
    char ticker[6]; // Max 5 characters + null terminator
    int quantity;
    double price;
    std::atomic<Order*> next;
};

class OrderBook {
public:
    std::atomic<Order*> buyHeads[1024]{};
    std::atomic<Order*> sellHeads[1024]{};
    char tickers[1024][6]{};
    int tickerCount = 0;
    
    int getTickerIndex(const char* ticker) {
        for (int i = 0; i < tickerCount; ++i) {
            if (strcmp(tickers[i], ticker) == 0) return i;
        }
        if (tickerCount < 1024) {
            strncpy(tickers[tickerCount], ticker, 5);
            tickers[tickerCount][5] = '\0'; // Ensure null termination
            return tickerCount++;
        }
        return -1; // No space for new ticker
    }
    
    void addOrder(char type, const char* ticker, int quantity, double price) {
        int index = getTickerIndex(ticker);
        if (index == -1) return; // Ignore orders if max tickers reached
        
        Order* newOrder = new Order; // Fix for atomic pointer assignment
        newOrder->type = type;
        newOrder->quantity = quantity;
        newOrder->price = price;
        newOrder->next = nullptr;
        strncpy(newOrder->ticker, ticker, 5);
        newOrder->ticker[5] = '\0';
        
        insertSortedOrder(newOrder, index, newOrder->type == 'B');
    }
    
    void insertSortedOrder(Order* newOrder, int index, bool isBuy) {
        Order* prev = nullptr;
        Order* current = isBuy ? buyHeads[index].load() : sellHeads[index].load();
        
        while (current && ((isBuy && newOrder->price <= current->price) || (!isBuy && newOrder->price >= current->price))) {
            prev = current;
            current = current->next;
        }
        
        newOrder->next = current;
        if (prev) {
            prev->next.store(newOrder);
        } else {
            if (isBuy) {
                buyHeads[index].store(newOrder);
            } else {
                sellHeads[index].store(newOrder);
            }
        }
    }
    
    void matchOrder() {
        for (int index = 0; index < tickerCount; ++index) {
            Order* buy = buyHeads[index].load();
            Order* prevSell = nullptr;
            Order* sell = sellHeads[index].load();
            
            while (buy && sell) {
                if (buy->price >= sell->price) {
                    int matchQty;
                    if (buy->quantity < sell->quantity) {
                        matchQty = buy->quantity;
                    } else {
                        matchQty = sell->quantity;
                    }
                    buy->quantity -= matchQty;
                    sell->quantity -= matchQty;
                    
                    if (sell->quantity == 0) {
                        if (prevSell) {
                            prevSell->next.store(sell->next); 
                        } else {
                            sellHeads[index].store(sell->next);
                        }
                        sell = sell->next;
                    }
                    if (buy->quantity == 0) buy = buy->next;
                } else {
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

