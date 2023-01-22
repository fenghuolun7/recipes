#include <map>
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

using std::string;

class Stock
{
public:
  Stock(const string &name)
      : name_(name)
  {
    printf(" Stock[%p] %s\n", this, name_.c_str());
  }

  ~Stock()
  {
    printf("~Stock[%p] %s\n", this, name_.c_str());
  }

  Stock(const Stock &) = delete;

  const string &key() const { return name_; }

private:
  string name_;
};

namespace version1
{

  // questionable code
  class StockFactory
  {
  public:
    StockFactory(const StockFactory &) = delete;
    StockFactory() = default;

    std::shared_ptr<Stock> get(const string &key)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      std::shared_ptr<Stock> pStock = stocks_[key];
      if (!pStock)
      {
        pStock.reset(new Stock(key));
      }
      return pStock;
    }

  private:
    mutable std::mutex mutex_;
    std::map<string, std::shared_ptr<Stock>> stocks_;
  };

}

namespace version2
{

  class StockFactory
  {
  public:
    StockFactory() = default;
    StockFactory(const StockFactory &) = delete;
    std::shared_ptr<Stock> get(const string &key)
    {
      std::shared_ptr<Stock> pStock;
      std::lock_guard<std::mutex> lock(mutex_);
      std::weak_ptr<Stock> &wkStock = stocks_[key];
      pStock = wkStock.lock();
      if (!pStock)
      {
        pStock.reset(new Stock(key));
        wkStock = pStock;
      }
      return pStock;
    }

  private:
    mutable std::mutex mutex_;
    std::map<string, std::weak_ptr<Stock>> stocks_;
  };

}

namespace version3
{

  class StockFactory
  {
  public:
    StockFactory() = default;
    StockFactory(const StockFactory &) = delete;
    std::shared_ptr<Stock> get(const string &key)
    {
      std::shared_ptr<Stock> pStock;
      std::lock_guard<std::mutex> lock(mutex_);
      std::weak_ptr<Stock> &wkStock = stocks_[key];
      pStock = wkStock.lock();
      if (!pStock)
      {
        pStock.reset(new Stock(key),
                     std::bind(&StockFactory::deleteStock, this, std::ref(pStock)));
        // here, add new created shared_ptr into map
        wkStock = pStock;
      }
      return pStock;
    }

  private:
    void deleteStock(std::shared_ptr<Stock> &pstock)
    {
      printf("deleteStock[%p]\n", pstock);
      if (pstock)
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stocks_.erase(pstock->key()); // This is wrong, see removeStock below for correct implementation.
      }
      delete pstock.get();
    }
    mutable std::mutex mutex_;
    std::map<string, std::weak_ptr<Stock>> stocks_;
  };

}

namespace version4
{

  class StockFactory : public std::enable_shared_from_this<StockFactory>
  {
  public:
    StockFactory(const StockFactory &) = delete;
    std::shared_ptr<Stock> get(const string &key)
    {
      std::shared_ptr<Stock> pStock;
      std::lock_guard<std::mutex> lock(mutex_);
      std::weak_ptr<Stock> &wkStock = stocks_[key];
      pStock = wkStock.lock();
      if (!pStock)
      {
        pStock.reset(new Stock(key),
                     std::bind(&StockFactory::deleteStock,
                               shared_from_this(),
                               pStock.get()));
        wkStock = pStock;
      }
      return pStock;
    }

  private:
    void deleteStock(Stock *stock)
    {
      printf("deleteStock[%p]\n", stock);
      if (stock)
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stocks_.erase(stock->key()); // This is wrong, see removeStock below for correct implementation.
      }
      delete stock; // sorry, I lied
    }
    mutable std::mutex mutex_;
    std::map<string, std::weak_ptr<Stock>> stocks_;
  };

}

class StockFactory : public std::enable_shared_from_this<StockFactory>
{
public:
  StockFactory() = default;
  StockFactory(const StockFactory &) = delete;
  std::shared_ptr<Stock> get(const string &key)
  {
    std::shared_ptr<Stock> pStock;
    std::lock_guard<std::mutex> lock(mutex_);
    std::weak_ptr<Stock> &wkStock = stocks_[key];
    pStock = wkStock.lock();
    if (!pStock)
    {
      pStock.reset(new Stock(key),
                   std::bind(&StockFactory::weakDeleteCallback,
                             std::weak_ptr<StockFactory>(shared_from_this()),
                             pStock.get()));
      wkStock = pStock;
    }
    return pStock;
  }

private:
  static void weakDeleteCallback(const std::weak_ptr<StockFactory> &wkFactory,
                                 Stock *stock)
  {
    printf("weakDeleteStock[%p]\n", stock);
    std::shared_ptr<StockFactory> factory(wkFactory.lock());
    if (factory)
    {
      factory->removeStock(stock);
    }
    else
    {
      printf("factory died.\n");
    }
    delete stock; // sorry, I lied
  }

  void removeStock(Stock *stock)
  {
    if (stock)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = stocks_.find(stock->key());
      if (it != stocks_.end() && it->second.expired())
      {
        stocks_.erase(stock->key());
      }
    }
  }

private:
  mutable std::mutex mutex_;
  std::map<string, std::weak_ptr<Stock>> stocks_;
};

void testLongLifeFactory()
{
  std::shared_ptr<StockFactory> factory(new StockFactory());
  {
    std::shared_ptr<Stock> stock = factory->get("NYSE:IBM");
    std::shared_ptr<Stock> stock2 = factory->get("NYSE:IBM");
    assert(stock == stock2);
    // stock destructs here
  }
  // factory destructs here
}

void testShortLifeFactory()
{
  std::shared_ptr<Stock> stock;
  {
    std::shared_ptr<StockFactory> factory(new StockFactory());
    stock = factory->get("NYSE:IBM");
    std::shared_ptr<Stock> stock2 = factory->get("NYSE:IBM");
    assert(stock == stock2);
    // factory destructs here
  }
  // stock destructs here
}

int main()
{
  // version1::StockFactory sf1;
  // version2::StockFactory sf2;
  version3::StockFactory sf3;
  // std::shared_ptr<version3::StockFactory> sf4(new version3::StockFactory);
  // std::shared_ptr<StockFactory> sf5(new StockFactory);

  {
    std::shared_ptr<Stock> s1 = sf3.get("stock1");
    std::shared_ptr<Stock> s2 = sf3.get("stock2");
    std::shared_ptr<Stock> s3 = sf3.get("stock3");
    std::shared_ptr<Stock> s4 = sf3.get("stock4");
    std::shared_ptr<Stock> s5 = sf3.get("stock4");
  }

  // {
  //   std::shared_ptr<Stock> s2 = sf2.get("stock2");
  // }

  // {
  //   std::shared_ptr<Stock> s3 = sf3.get("stock3");
  // }

  // {
  //   std::shared_ptr<Stock> s4 = sf4->get("stock4");
  // }

  // {
  //   std::shared_ptr<Stock> s5 = sf5->get("stock5");
  // }

  // testLongLifeFactory();
  // testShortLifeFactory();
}
