#include "OrderBook.h"
#include "Order.h"

#include <iostream>
#include <limits>
#include <sstream>

// Error handling is not implemented for the input.
// It is always considered valid.
namespace {
  Order createOrderFromLine(const std::string& line) {
    std::stringstream ss{line};
    char type;
    char side;
    Price price;
    uint32_t quantity;
    uint32_t peakSize;
    OrderId id;

    ss >> type;
    ss >> side;
    ss >> id;
    ss >> price;
    ss >> quantity;

    OrderSide orderSide = side == 'B' ? OrderSide::BUY : OrderSide::SELL;

    if (type == 'I') {
      ss >> peakSize;
      return Order(price, quantity, id, orderSide, peakSize);
    }
    return Order(price, quantity, id, orderSide);
  }

} // namespace

int main() {
  OrderBook orderBook;

  // Read orders on stdin.
  for (std::string line; std::getline(std::cin, line);) {

    std::string order = line.substr(0, line.find("#"));
    if (order.empty()) { continue; }

    if (order[0] == 'C') {
      orderBook.CancelOrder(std::stol(order.substr(1, std::string::npos)));
    }
    else {
      orderBook.AddOrder(createOrderFromLine(order));
    }
  }

  orderBook.Print();
  std::cout << std::endl;
  return 0;
}
