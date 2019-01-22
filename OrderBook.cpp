\#include "OrderBook.h"

#include <algorithm>
#include <iostream>
#include <vector>

// A debatable solution for readability would have been to have to order book templated:
// one for buy and sell order.
namespace {
  template<OrderSide side>
  inline bool MatchPriceComparator(Price fromBook, Price p) = delete;

  template<>
  inline bool MatchPriceComparator<OrderSide::SELL>(Price fromBook, Price p) { return fromBook >= p; }

  template<>
  inline bool MatchPriceComparator<OrderSide::BUY>(Price fromBook, Price p) { return fromBook <= p; }

  template<OrderSide side>
  inline std::string OrderIdMatch(OrderId fromBook, OrderId id) = delete;

  template<>
  inline std::string OrderIdMatch<OrderSide::SELL>(OrderId fromBook, OrderId id) {
    return std::to_string(fromBook) + " " + std::to_string(id);
  }

  template<>
  inline std::string OrderIdMatch<OrderSide::BUY>(OrderId fromBook, OrderId id) {
    return std::to_string(id) + " " + std::to_string(fromBook);
  }

  // Log the trades when inserting a new order in a fifo way. The trades will be printed when
  // no other trade is possible and eventually the new order is inserted in the order book.
  inline void AddMatchTrade(std::unordered_map<OrderId, uint32_t>& matchOrderQuantityMap,
                            std::vector<std::pair<OrderId, Price>>& matchOrderQueue, OrderId fromBookId,
                            Price fromBookPrice, uint32_t quantityTrade) {
    auto matchOrderQuantityIter = matchOrderQuantityMap.find(fromBookId);
    if (matchOrderQuantityIter == matchOrderQuantityMap.end()) {
      matchOrderQuantityMap[fromBookId] = quantityTrade;
      matchOrderQueue.push_back(std::make_pair(fromBookId, fromBookPrice));
    }
    else {
      matchOrderQuantityMap[fromBookId] += quantityTrade;
    }
  }

  template<OrderSide side>
  inline void PrintTrades(std::unordered_map<OrderId, uint32_t>& matchOrderQuantityMap,
                          std::vector<std::pair<OrderId, Price>>& matchOrderQueue,
                          const Order& order) {
    for (auto& [id, price] : matchOrderQueue) {
      std::cout << "M " << OrderIdMatch<side>(id, order.id)
                << " " << price << " " << matchOrderQuantityMap[id] << std::endl;
    }
  }

} // namespace

// Try to handle trades before insterting in the book.
// This has the advantage to use only one map that stores the orders per price level.
// It has been decided here to template the function so that the check if the order is a buy order or
// a sell one is done only once in the process.
// An other solution could have been, always insert in the book and then look for any trades possible
// which would have may made the code easier to read but less efficient.
// Complexity: several average O(1) for access/erase/pop/ operation + O(log(n)) for the insertion
// in the price heaps *only* when such a price is not already present + the trading part complexity.
// Trading Part complexity: This is the slower part of the core logic since we need to go through
// all the orders until their is no match possible anymore for the new order.
// Hidden quantity for the entry order is managed only before inserting it in the book, this avoids
// multiple updates in the loop.
template<OrderSide side>
void OrderBook::AddOrderInternal(const Order& order, Heap<side>& orderHeap,
                                 OppositeHeap<side>& oppositeOrderHeap) {
  Order tempOrder = order;

  // The two following variables are used to print the different trades that happened.
  std::unordered_map<OrderId, uint32_t> matchOrderQuantityMap;
  std::vector<std::pair<OrderId, Price>> matchOrderQueue;

  while (tempOrder.quantity > 0 && !oppositeOrderHeap.empty()) {
    auto currentBestPrice = oppositeOrderHeap.top();
    if (!MatchPriceComparator<side>(currentBestPrice, tempOrder.price)) {
      break;
    }

    auto& bestPriceOrderList = orderMap_[currentBestPrice];

    if (bestPriceOrderList.empty()) {
      orderMap_.erase(currentBestPrice);
      oppositeOrderHeap.pop();
      continue;
    }

    auto highestPriorityOrderIter = bestPriceOrderList.begin();
    auto quantityTraded = std::min(highestPriorityOrderIter->quantity, tempOrder.quantity);
    highestPriorityOrderIter->quantity -= quantityTraded;

    // Add a match to be print after all the trades.
    AddMatchTrade(matchOrderQuantityMap, matchOrderQueue, highestPriorityOrderIter->id,
                  currentBestPrice, quantityTraded);

    if (highestPriorityOrderIter->quantity == 0) {
      if (highestPriorityOrderIter->hiddenQuantity != 0) {
        // Iceberg shares left, update quantity and put at the back of the queue.
        highestPriorityOrderIter->quantity = std::min(highestPriorityOrderIter->hiddenQuantity,
                                                      highestPriorityOrderIter->peakSize);
        highestPriorityOrderIter->hiddenQuantity -= highestPriorityOrderIter->quantity;
        bestPriceOrderList.splice(bestPriceOrderList.end(), bestPriceOrderList,
                                  highestPriorityOrderIter);
      }
      else {
        // Remove the order, not shares left.
        orders_.erase(highestPriorityOrderIter->id);
        bestPriceOrderList.pop_front();
        if (bestPriceOrderList.empty()) {
          orderMap_.erase(currentBestPrice);
          oppositeOrderHeap.pop();
        }
      }
    }

    tempOrder.quantity -= quantityTraded;
  }

  // Add in order book.
  if (tempOrder.quantity > 0) {
    if (tempOrder.peakSize > 0 && tempOrder.quantity > tempOrder.peakSize) {
      auto totalQuantityTraded = order.quantity - tempOrder.quantity;
      auto visibleQuantity = tempOrder.peakSize - (totalQuantityTraded % tempOrder.peakSize);
      tempOrder.hiddenQuantity = tempOrder.quantity - visibleQuantity;
      tempOrder.quantity = visibleQuantity;
    }
    auto priceLevelIter = orderMap_.find(tempOrder.price);
    if (priceLevelIter == orderMap_.end()) {
      priceLevelIter = orderMap_.emplace(tempOrder.price, std::list<Order>{}).first;
      orderHeap.push(tempOrder.price);
    }
    priceLevelIter->second.push_back(tempOrder);
    orders_[tempOrder.id] = --priceLevelIter->second.end();
  }

  // Print Matchs/Trades
  PrintTrades<side>(matchOrderQuantityMap, matchOrderQueue, order);
}

void OrderBook::AddOrder(const Order& order) {
  if (order.side == OrderSide::SELL) {
    AddOrderInternal<OrderSide::SELL>(order, sellOrderHeap_, buyOrderHeap_);
  }
  else {
    AddOrderInternal<OrderSide::BUY>(order, buyOrderHeap_, sellOrderHeap_);
  }
}

// Average Complexity O(1).
// unordered_map access O(1) + unordered_map/list erase(iterator pos) O(1)
bool OrderBook::CancelOrder(OrderId id) {
  auto orderIter = orders_.find(id);

  if (orderIter == orders_.end()) {
    // The order is not in the book.
    std::cerr << "Error: order: " << id << " not in the book. "
              << "Unable to cancel." << std::endl;
    return false;
  }

  const auto listIter = orderIter->second;
  const auto order = *listIter;

  orders_.erase(order.id);
  orderMap_[order.price].erase(listIter);

  return true;
}

// IO function:
// Accept bad performance here for a faster core logic. This function should not modify
// the order book so a copy of the priority_queue is required since
// the iteration is not possible without "poping" all the elements.
// If the performance was important, here it might have been a good idea to implement a custom
// vector based heap with the possibly to iterate on it. Using std::map as a heap have been
// considered but it would have slow down the core logic.
void OrderBook::Print() const {
  auto copyBuyOrderHeap = buyOrderHeap_;
  auto copySellOrderHeap = sellOrderHeap_;

  auto orderPrinter = [] (const Order& order) {
    std::cout << "O " << (order.side == OrderSide::BUY ? "B " : "S ")
              << order.id << " " << order.price << " " << order.quantity << std::endl;
  };

  auto printHeap = [&orderPrinter, this](auto& orderHeap) {
    while (!orderHeap.empty()) {
      const auto searchIter = orderMap_.find(orderHeap.top());

      if (searchIter != orderMap_.end()) {
        const auto& orderList = searchIter->second;
        std::for_each(orderList.begin(), orderList.end(), orderPrinter);
      }
      else {
        std::cerr << "Internal Error: print: price " << orderHeap.top() << " not found." << std::endl;
      }
      orderHeap.pop();
    }
  };

  printHeap(copyBuyOrderHeap);
  printHeap(copySellOrderHeap);
}
