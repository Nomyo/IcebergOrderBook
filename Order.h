#pragma once

#include <cstdint>

using OrderId = uint32_t;
using Price = uint32_t;

enum class OrderSide : char {
  SELL = 0,
  BUY
};

// In this implementation Iceberg and limit order are represented with a same data structure.
// The distinction between an iceberg order and a limit order resides in the value of peakSize
// that is 0 for a limit order.
// The implementation of the order book is simple with this data structure, however it has as a drawback
// a useless field for limit order (hiddenQuantity).
struct Order {
  explicit Order(Price orderPrice, uint32_t orderQuantity, OrderId orderId, OrderSide orderSide,
                 uint32_t orderPeakSize = 0)
    : price(orderPrice)
    , quantity(orderQuantity)
    , hiddenQuantity(0)
    , peakSize(orderPeakSize)
    , id(orderId)
    , side(orderSide) { }

  const Price price;
  uint32_t quantity;
  uint32_t hiddenQuantity;
  const uint32_t peakSize;
  const OrderId id;
  const OrderSide side;
};
