#include <algorithm>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>

class Subject;

class Observer : public std::enable_shared_from_this<Observer>
{
public:
  virtual ~Observer();
  Observer() = default;
  Observer(int id) : id(id) {}
  virtual void update() = 0;

  void observe(Subject *s);
  int id;

protected:
  Subject *subject_;
};

class Subject
{
public:
  void register_(std::shared_ptr<Observer> x);
  void unregister(std::shared_ptr<Observer> x);

  void notifyObservers()
  {
    std::lock_guard<std::mutex> lk(mutex_);
    Iterator it = observers_.begin();
    while (it != observers_.end())
    {
      if (*it)
      {
        (*it)->update();
        ++it;
      }
      else
      {
        printf("notifyObservers() erase\n");
        it = observers_.erase(it);
      }
    }
  }

private:
  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<Observer>> observers_;
  typedef std::vector<std::shared_ptr<Observer>>::iterator Iterator;
};

Observer::~Observer()
{
  subject_->unregister(shared_from_this());
}

void Observer::observe(Subject *s)
{
  s->register_(shared_from_this());
  subject_ = s;
}

void Subject::register_(std::shared_ptr<Observer> x)
{
  // i guess here use_count is 3
  observers_.push_back(x);
}

void Subject::unregister(std::shared_ptr<Observer> x)
{
  for (auto it = observers_.begin(); it != observers_.end(); it++)
  {
    // ??? not so sure
    if (*it == x)
    {
      printf("notifyObservers() erase\n");
      it = observers_.erase(it);
    }
  }
}

// ---------------------

class Foo : public Observer
{
public:
  Foo(int id) : Observer(id) {}

  virtual void update()
  {
    printf("Foo::update() %p\n", this);
    std::cout << "foo id is : " << id << std::endl;
  }
};

void process(std::shared_ptr<Foo> sp, Subject &subject)
{
  sp->observe(&subject);
  // std::this_thread::sleep_for(std::chrono::seconds(5));
}

int main()
{
  Subject subject;

  std::shared_ptr<Foo> p1(new Foo(1));
  std::shared_ptr<Foo> p2(new Foo(2));

  std::thread t1(process, p1, std::ref(subject));
  t1.join();
  // when it exit, I guess user_count is 2;
  std::thread t2(process, p2, std::ref(subject));
  t2.join();
  subject.notifyObservers();

  // process(p1, subject);
  // process(p2, subject);
}
