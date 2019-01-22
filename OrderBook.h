#pragma once

#include "Order.h"

#include <list>
#include <queue>
#include <unordered_map>

// This implementation is meant to make most of the insertion of an order with a price that
// already exist is in average constant O(1).
// Two heaps are used to store the best current price for selling and buying cases.
// An hash-map is used to store all the orders at with the same price and another one that maps
// an orderid to a a list iterator to have the cancel order operation in average constant.
class OrderBook {

  // A list used as a queue of all the order at the same price level ordered by increasing timestamps.
  using TimeOrderList = std::list<Order>;

  template<OrderSide side>
  using ComparatorHeapFunc = typename std::conditional<side == OrderSide::SELL,
                                                       std::greater<Price>, std::less<Price>>::type;

  // A heap that stores the price as a min heap for selling order and a max heap otherwise.
  template<OrderSide side>
  using Heap = std::priority_queue<Price, std::vector<Price>, ComparatorHeapFunc<side>>;

  template<OrderSide side>
  using OppositeHeap = Heap<(side == OrderSide::SELL ? OrderSide::BUY : OrderSide::SELL)>;

public:
  OrderBook() {}
  ~OrderBook() = default;

  void AddOrder(const Order& order);

  // Cancel an order given an Order ID. Returns true if the order is canceled.
  // Otherwise returns false.
  bool CancelOrder(OrderId id);

  void Print() const;

private:
  template<OrderSide side>
  void AddOrderInternal(const Order& order, Heap<side>& orderHeap,
                        OppositeHeap<side>& oppositeOrderHeap);

private:
  // The reason why there is only a single map is because in this implementation an order
  // is added to the book only after the trades. So there can't be an BUY and SELL order at
  // the same price.
  std::unordered_map<Price, TimeOrderList> orderMap_;

  Heap<OrderSide::BUY> buyOrderHeap_;
  Heap<OrderSide::SELL> sellOrderHeap_;

  std::unordered_map<OrderId, TimeOrderList::iterator> orders_;
};
