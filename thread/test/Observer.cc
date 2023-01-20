#include <algorithm>
#include <vector>
#include <stdio.h>

class Subject;

class Observer
{
 public:
  virtual ~Observer();
  virtual void update() = 0;

  void observe(Subject* s);

 protected:
  Subject* subject_;
};

class Subject
{
 public:
  void register_(Observer* x);
  void unregister(Observer* x);

  void notifyObservers()
  {
    for (size_t i = 0; i < observers_.size(); ++i)
    {
      Observer* x = observers_[i];
      if (x) {
        x->update(); // (3)
      }
    }
  }

 private:
  std::vector<Observer*> observers_;
};

Observer::~Observer()
{
  subject_->unregister(this);
}

void Observer::observe(Subject* s)
{
  s->register_(this);
  subject_ = s;
}

void Subject::register_(Observer* x)
{
  observers_.push_back(x);
}

void Subject::unregister(Observer* x)
{
  std::vector<Observer*>::iterator it = std::find(observers_.begin(), observers_.end(), x);
  if (it != observers_.end())
  {
    std::swap(*it, observers_.back());
    observers_.pop_back();
  }
}

// ---------------------

class Foo : public Observer
{
  virtual void update()
  {
    printf("Foo::update() %p\n", this);
  }
};

int main()
{
  Foo* p = new Foo;
  Subject subject;
  // The observer can directly call the interface to observe the subject
  p->observe(&subject);
  subject.notifyObservers();
  delete p;
  subject.notifyObservers();
}
